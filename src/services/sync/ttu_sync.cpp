// SPDX-License-Identifier: GPL-3.0-or-later

#include "ttu_sync.h"

#include "services/statistics/statistics_storage.h"
#include "ttu_sync_sidecars.h"

#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QTemporaryDir>

#include <cmath>
#include <limits>
#include <utility>

namespace niratan::sync {
namespace {

using detail::Batch;
using detail::Error;
using detail::import_audio;
using detail::import_progress;
using detail::reference_seconds;
using detail::RemoteSidecars;
using detail::Rows;
template <typename T> using ResultCallback = TtuDrive::ResultCallback<T>;
constexpr auto reference_epoch = 978307200.0;

GoogleDriveError unavailable(std::string message) {
    return {GoogleDriveErrorKind::unavailable, std::move(message), {}};
}

std::optional<std::string> file_id(const std::optional<DriveFile>& file) {
    return file ? std::optional{file->id} : std::nullopt;
}

std::optional<std::int64_t> unix_milliseconds(double reference_seconds) {
    const auto milliseconds = (reference_seconds + reference_epoch) * 1000;
    constexpr auto minimum = static_cast<double>(std::numeric_limits<std::int64_t>::min());
    if (!std::isfinite(milliseconds) || milliseconds < minimum || milliseconds >= -minimum) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(milliseconds);
}

std::optional<std::int64_t> playback_mtime(const std::filesystem::path& root) {
    const QFileInfo info(QString::fromStdString((root / "sasayaki_playback.json").string()));
    const auto modified = info.lastModified();
    return modified.isValid() ? std::optional<std::int64_t>{modified.toMSecsSinceEpoch()}
                              : std::nullopt;
}

struct SyncRun {
    books::BookMetadata book;
    std::optional<SyncDirection> requested_direction;
    TtuSyncOptions options;
    ResultCallback<TtuResult> callback;
    bool can_retry = true;
    std::string folder;
    std::filesystem::path root;
    std::optional<books::Bookmark> bookmark;
    DriveSyncFiles files = DriveSyncFiles({});
    Rows local_stats;
    std::optional<sasayaki::SasayakiPlaybackData> local_playback;
};

struct ImportRun {
    DriveSyncFiles files;
    ResultCallback<std::filesystem::path> callback;
    std::optional<std::filesystem::path> root;
    RemoteSidecars remote;
    Batch<2> batch;
};

}  // namespace

struct TtuSync::State {
    QPointer<TtuSync> owner;
    TtuDrive& drive;
    books::BookStorage& storage;
    BookDataConverter converter;
    int reset_minutes;
    QTimeZone zone;

    using Run = std::shared_ptr<SyncRun>;
    using Import = std::shared_ptr<ImportRun>;

    void complete(const Run& run, TtuResult::Kind kind, int characters = 0) {
        const auto title =
            kind == TtuResult::Kind::skipped ? std::string{} : run->book.display_title();
        run->callback(TtuResult{kind, title, characters}, std::nullopt);
    }

    void fail(const Run& run, Error error) {
        if (error && error->is_stale_cache_error() && std::exchange(run->can_retry, false)) {
            drive.clear_cache();
            start(run);
        } else {
            run->callback(std::nullopt, error.value_or(GoogleDriveError{}));
        }
    }

    TtuDrive::CoverProvider cover(const Run& run) {
        if (!run->book.cover) {
            return {};
        }
        return {[this, guard = owner, run]() -> std::optional<QByteArray> {
            if (!guard) {
                return std::nullopt;
            }
            const auto path = storage.cover_path(run->book);
            if (!path) {
                return std::nullopt;
            }
            QFile file(QString::fromStdString(path->string()));
            if (!file.open(QIODevice::ReadOnly)) {
                return std::nullopt;
            }
            auto bytes = file.readAll();
            return file.error() == QFileDevice::NoError ? std::optional{std::move(bytes)}
                                                        : std::nullopt;
        }};
    }

    void start(const Run& run) {
        drive.find_root_folder([this, guard = owner, run](auto root, auto error) {
            if (!guard) {
                return;
            }
            if (!root) {
                fail(run, std::move(error));
                return;
            }
            drive.ensure_book_folder(run->book.title, *root, cover(run),
                                     [this, guard, run](auto folder, auto failure) {
                                         if (!guard) {
                                             return;
                                         }
                                         if (!folder) {
                                             fail(run, std::move(failure));
                                             return;
                                         }
                                         list_book(run, std::move(*folder));
                                     });
        });
    }

    void list_book(const Run& run, std::string folder) {
        const auto root = storage.book_directory(run->book.folder);
        if (!root) {
            fail(run, unavailable("Invalid book directory"));
            return;
        }
        run->root = *root;
        run->folder = std::move(folder);
        run->bookmark = storage.load_bookmark(run->book.folder);
        drive.list_sync_files(run->folder, [this, guard = owner, run](auto files, auto error) {
            if (!guard) {
                return;
            }
            if (!files) {
                fail(run, std::move(error));
                return;
            }
            run->files = std::move(*files);
            if (run->options.sync_book_data && !run->options.import_only &&
                run->requested_direction != SyncDirection::import_from_ttu &&
                !run->files.book_data && converter.export_zip) {
                export_book_data(run);
            } else {
                decide(run);
            }
        });
    }

    void export_book_data(const Run& run) {
        const auto temporary = std::make_shared<QTemporaryDir>();
        if (!temporary->isValid()) {
            fail(run, unavailable("Could not create a temporary directory for book data"));
            return;
        }
        const auto zip = converter.export_zip(run->root, temporary->path().toStdString());
        if (!zip) {
            decide(run);
            return;
        }
        drive.upload_book_data(run->folder, *zip,
                               [this, guard = owner, run, temporary](Error error) {
                                   temporary->remove();
                                   if (guard) {
                                       if (error) {
                                           fail(run, std::move(error));
                                       } else {
                                           decide(run);
                                       }
                                   }
                               });
    }

    void decide(const Run& run) {
        using enum SyncDirection;
        const auto direction = run->requested_direction.value_or(
            determine_direction(run->bookmark, run->files.progress));
        if (direction == synced) {
            if (run->options.import_only && run->options.sync_audio_book &&
                should_import_audio_book(run->files.audio_book, playback_mtime(run->root))) {
                import_newer_audio(run);
            } else {
                complete(run, TtuResult::Kind::synced);
            }
        } else if (run->options.import_only && direction != import_from_ttu) {
            complete(run, TtuResult::Kind::skipped);
        } else {
            run->local_stats = run->options.sync_stats
                ? statistics::load_statistics(run->root, reset_minutes, zone).daily
                : Rows{};
            run->local_playback = run->options.sync_audio_book
                ? storage.load_sasayaki_playback(run->book.folder)
                : std::nullopt;
            detail::read_remote(drive, run->files, run->options.sync_stats,
                                run->options.sync_audio_book,
                                [this, guard = owner, run, direction](RemoteSidecars remote) {
                                    if (guard) {
                                        if (auto error = remote.reads.failure()) {
                                            fail(run, std::move(error));
                                        } else if (direction == import_from_ttu) {
                                            apply_import(run, remote);
                                        } else {
                                            export_sidecars(run, remote);
                                        }
                                    }
                                });
        }
    }

    void import_newer_audio(const Run& run) {
        drive.get_audio_book_file(
            run->files.audio_book->id, [this, guard = owner, run](auto audio, auto error) {
                if (!guard) {
                    return;
                }
                if (error) {
                    fail(run, std::move(error));
                } else if (audio) {
                    import_audio(storage, run->book.folder, *audio);
                    complete(run, TtuResult::Kind::imported,
                             run->bookmark ? run->bookmark->character_count : 0);
                } else {
                    complete(run, TtuResult::Kind::synced);
                }
            });
    }

    void apply_import(const Run& run, const RemoteSidecars& remote) {
        if (!remote.progress) {
            complete(run, TtuResult::Kind::skipped);
            return;
        }
        import_progress(storage, run->book.folder, *remote.progress);
        if (remote.stats && !remote.stats->empty()) {
            statistics::import_daily_statistics(run->root, *remote.stats, run->options.stats_mode,
                                                reset_minutes, zone);
        }
        if (remote.audio) {
            import_audio(storage, run->book.folder, *remote.audio);
        }
        complete(run, TtuResult::Kind::imported, remote.progress->explored_char_count);
    }

    void export_progress(const Run& run, const RemoteSidecars& remote,
                         TtuDrive::ErrorCallback then) {
        auto bookmark = *run->bookmark;
        const auto info = storage.load_book_info(run->book.folder);
        const auto ms =
            bookmark.last_modified ? unix_milliseconds(*bookmark.last_modified) : std::nullopt;
        if (!info || !ms) {
            then(std::nullopt);
            return;
        }
        const TtuProgress progress{
            remote.progress ? remote.progress->data_id : 0, bookmark.character_count,
            info->character_count > 0
                ? static_cast<double>(bookmark.character_count) / info->character_count
                : 0.0,
            *ms};
        bookmark.last_modified = reference_seconds(*ms);
        drive.update_progress_file(
            run->folder, file_id(run->files.progress), progress,
            [this, guard = owner, run, bookmark, then = std::move(then)](Error error) {
                if (guard) {
                    if (!error) {
                        storage.save_bookmark(run->book.folder, bookmark);
                    }
                    then(std::move(error));
                }
            });
    }

    void export_sidecars(const Run& run, const RemoteSidecars& remote) {
        if (!run->bookmark) {
            complete(run, TtuResult::Kind::skipped);
            return;
        }
        const auto batch = std::make_shared<Batch<3>>();
        const auto then = [this, guard = owner, run, batch](std::size_t index, Error error) {
            if (guard && batch->finished(index, std::move(error))) {
                if (auto failure = batch->failure()) {
                    fail(run, std::move(failure));
                } else {
                    complete(run, TtuResult::Kind::exported, run->bookmark->character_count);
                }
            }
        };
        auto stats = run->options.sync_stats
            ? merge_statistics(remote.stats.value_or(Rows{}), run->local_stats,
                               run->options.stats_mode)
            : Rows{};
        export_progress(run, remote, [then](Error error) { then(0, std::move(error)); });
        if (!stats.empty()) {
            drive.update_stats_file(run->folder, file_id(run->files.statistics), std::move(stats),
                                    [then](Error error) { then(1, std::move(error)); });
        } else {
            then(1, {});
        }
        if (run->local_playback) {
            auto value = TtuAudioBook{run->book.title, run->local_playback->last_position,
                                      statistics::current_time_milliseconds()};
            drive.update_audio_book_file(run->folder, file_id(run->files.audio_book),
                                         std::move(value),
                                         [then](Error error) { then(2, std::move(error)); });
        } else {
            then(2, {});
        }
    }

    Error extract(const Import& run, const QByteArray& bytes) {
        QTemporaryDir temporary;
        if (!temporary.isValid()) {
            return unavailable("Could not create a temporary directory for book data");
        }
        const auto path = temporary.filePath(QStringLiteral("bookdata.zip"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
            !file.flush()) {
            return unavailable("Could not stage the downloaded book data");
        }
        file.close();
        const auto books_dir = storage.books_directory();
        std::error_code error;
        std::filesystem::create_directories(books_dir, error);
        if (error) {
            return unavailable(error.message());
        }
        run->root = converter.import_zip(path.toStdString(), books_dir);
        if (!run->root) {
            return unavailable("Could not convert the downloaded book data");
        }
        return std::nullopt;
    }

    void finish_import(const Import& run, std::size_t index, Error error) {
        if (!run->batch.finished(index, std::move(error))) {
            return;
        }
        if (auto failure = run->batch.failure()) {
            run->callback(std::nullopt, std::move(failure));
            return;
        }
        const auto folder = run->root->lexically_relative(storage.books_directory()).string();
        if (storage.book_directory(folder) != run->root) {
            run->callback(std::nullopt, unavailable("Invalid converted book directory"));
            return;
        }
        const auto& failures = run->remote.reads.errors;
        if (failures[0]) {
            run->callback(std::nullopt, failures[0]);
            return;
        }
        if (run->remote.progress) {
            import_progress(storage, folder, *run->remote.progress);
        }
        if (failures[1]) {
            run->callback(std::nullopt, failures[1]);
            return;
        }
        if (run->remote.stats && !run->remote.stats->empty()) {
            std::string reason;
            if (!statistics::import_daily_statistics(
                    *run->root, *run->remote.stats, statistics::StatisticsSyncMode::merge,
                    reset_minutes, zone, statistics::current_time_milliseconds(), &reason)) {
                run->callback(std::nullopt, unavailable(std::move(reason)));
                return;
            }
        }
        if (failures[2]) {
            run->callback(std::nullopt, failures[2]);
            return;
        }
        if (run->remote.audio) {
            import_audio(storage, folder, *run->remote.audio);
        }
        run->callback(run->root, std::nullopt);
    }

    void import_book(const Import& run, bool stats, bool audio, DriveTransport::Progress progress) {
        drive.download_file(run->files.book_data->id, std::move(progress),
                            [this, guard = owner, run](auto bytes, auto error) {
                                if (guard) {
                                    finish_import(run, 0,
                                                  bytes
                                                      ? extract(run, *bytes)
                                                      : Error{error.value_or(GoogleDriveError{})});
                                }
                            });
        detail::read_remote(drive, run->files, stats, audio,
                            [this, guard = owner, run](RemoteSidecars data) {
                                if (guard) {
                                    run->remote = std::move(data);
                                    finish_import(run, 1, {});
                                }
                            });
    }
};

TtuSync::TtuSync(TtuDrive& drive, books::BookStorage& storage, BookDataConverter converter,
                 int reset_minutes, QTimeZone zone, QObject* parent)
    : QObject(parent),
      state_(std::make_unique<State>(
          State{this, drive, storage, std::move(converter), reset_minutes, std::move(zone)})) {}

TtuSync::~TtuSync() = default;

void TtuSync::sync_book(books::BookMetadata book, std::optional<SyncDirection> direction,
                        TtuSyncOptions options, ResultCallback<TtuResult> callback) {
    state_->start(std::make_shared<SyncRun>(SyncRun{std::move(book),
                                                    direction,
                                                    options,
                                                    std::move(callback),
                                                    true,
                                                    {},
                                                    {},
                                                    {},
                                                    DriveSyncFiles({}),
                                                    {},
                                                    {}}));
}

void TtuSync::import_google_drive_book(DriveSyncFiles files, bool sync_stats, bool sync_audio_book,
                                       DriveTransport::Progress progress,
                                       ResultCallback<std::filesystem::path> callback) {
    if (!files.book_data) {
        callback(std::nullopt, GoogleDriveError{});
    } else if (!state_->converter.import_zip) {
        callback(std::nullopt, unavailable("Book data conversion is not configured"));
    } else {
        auto run = std::make_shared<ImportRun>(
            ImportRun{std::move(files), std::move(callback), {}, {}, {}});
        state_->import_book(run, sync_stats, sync_audio_book, std::move(progress));
    }
}

}  // namespace niratan::sync
