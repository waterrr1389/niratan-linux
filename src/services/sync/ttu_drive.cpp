// SPDX-License-Identifier: GPL-3.0-or-later

#include "ttu_drive.h"

#include "ttu_naming.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <utility>

namespace niratan::sync {
namespace {

constexpr auto files_path = "/drive/v3/files";
constexpr auto upload_path = "/upload/drive/v3/files";
constexpr auto folder_mime = "application/vnd.google-apps.folder";
using ErrorCallback = TtuDrive::ErrorCallback;
using Rows = std::vector<statistics::Statistics>;
template <typename T> using ResultCallback = TtuDrive::ResultCallback<T>;

QUrlQuery query_item(const char* key, const std::string& value) {
    QUrlQuery query;
    auto text = QString::fromStdString(value);
    // QUrlQuery accepts already encoded text as well as literal characters.
    text.replace(u'%', QStringLiteral("%25"));
    query.addQueryItem(QLatin1StringView(key), text);
    return query;
}

QUrlQuery list_query(const std::string& filter, std::string_view fields) {
    auto query = query_item("q", filter);
    query.addQueryItem("fields", QString::fromLatin1(fields.data(), fields.size()));
    return query;
}

std::string folder_filter(const std::string& parent) {
    return "trashed=false and '" + parent + "' in parents and mimeType='" + folder_mime + "'";
}

QByteArray json(const QJsonObject& object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::string describe_double(double value) {
    char buffer[64];
    const auto magnitude = std::abs(value);
    const auto format = magnitude >= 1e16 || (magnitude != 0 && magnitude < 1e-4)
        ? std::chars_format::scientific
        : std::chars_format::fixed;
    const auto number = std::to_chars(std::begin(buffer), std::end(buffer), value, format);
    std::string result(buffer, number.ptr);
    if (std::isfinite(value) && result.find_first_of(".e") == std::string::npos) {
        result += ".0";
    }
    return result;
}

constexpr std::pair<const char*, int statistics::Statistics::*> speed_fields[] = {
    {"minReadingSpeed", &statistics::Statistics::min_reading_speed},
    {"altMinReadingSpeed", &statistics::Statistics::alt_min_reading_speed},
    {"lastReadingSpeed", &statistics::Statistics::last_reading_speed},
    {"maxReadingSpeed", &statistics::Statistics::max_reading_speed},
};

template <typename Int> bool integer(const QJsonValue& value, Int& out) {
    constexpr auto min = std::numeric_limits<qint64>::min();
    const auto number = value.toInteger(min);
    if ((number == min && value.toInteger(0) != min) || !std::in_range<Int>(number)) {
        return false;
    }
    out = static_cast<Int>(number);
    return true;
}

bool decode_statistics(QByteArrayView bytes, Rows& rows, std::string&) {
    const auto document = QJsonDocument::fromJson(bytes.toByteArray());
    if (!document.isArray()) {
        return false;
    }
    for (const auto value : document.array()) {
        const auto object = value.toObject();
        const auto title = object.value("title");
        const auto date = object.value("dateKey");
        const auto time = object.value("readingTime");
        statistics::Statistics row;
        if (!title.isString() || !date.isString() || !time.isDouble() ||
            !integer(object.value("charactersRead"), row.characters_read) ||
            !integer(object.value("lastStatisticModified"), row.last_statistic_modified)) {
            return false;
        }
        for (const auto& [key, field] : speed_fields) {
            if (!integer(object.value(QLatin1StringView(key)), row.*field)) {
                return false;
            }
        }
        row.title = title.toString().toStdString();
        row.date_key = date.toString().toStdString();
        row.reading_time = time.toDouble();
        rows.push_back(std::move(row));
    }
    return true;
}

QByteArray encode_statistics(const Rows& rows) {
    QJsonArray array;
    for (const auto& row : rows) {
        QJsonObject object{{"title", QString::fromStdString(row.title)},
                           {"dateKey", QString::fromStdString(row.date_key)},
                           {"charactersRead", row.characters_read},
                           {"readingTime", row.reading_time},
                           {"lastStatisticModified", qint64(row.last_statistic_modified)}};
        for (const auto& [key, field] : speed_fields) {
            object.insert(QLatin1StringView(key), row.*field);
        }
        array.append(object);
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

template <typename T>
DriveTransport::Callback decode(bool (*parser)(QByteArrayView, T&, std::string&),
                                ResultCallback<T> callback) {
    return [parser, callback = std::move(callback)](DriveResponse response) {
        if (!response.body) {
            callback(std::nullopt, std::move(response.error));
            return;
        }
        T value;
        std::string error;
        if (!parser(*response.body, value, error)) {
            callback(std::nullopt, GoogleDriveError{});
        } else {
            callback(std::move(value), std::nullopt);
        }
    };
}

std::string_view cover_extension(QByteArrayView data) {
    if (data.size() >= 4) {
        if (data.startsWith("\x89PNG")) {
            return "png";
        }
        if (data.startsWith("GIF8")) {
            return "gif";
        }
        if (data.startsWith("BM")) {
            return "bmp";
        }
        if (data.startsWith("RIFF")) {
            return "webp";
        }
    }
    return "jpeg";
}

}  // namespace

struct TtuDrive::State {
    QPointer<TtuDrive> owner;
    DriveTransport& transport;
    std::optional<std::string> root;
    std::map<std::string, std::string> folders;

    void list(QUrlQuery query, ResultCallback<DriveFileList> callback) {
        transport.request(DriveMethod::get, files_path, query, {}, {},
                          decode(parse_drive_file_list, std::move(callback)));
    }

    struct Pages {
        std::vector<QUrlQuery> queries;
        std::size_t index = 0;
        std::optional<std::string> page_token;
        std::vector<DriveFile> files;
        ResultCallback<std::vector<DriveFile>> callback;
    };

    void next_page(const std::shared_ptr<Pages>& pages) {
        if (pages->index == pages->queries.size()) {
            pages->callback(std::move(pages->files), std::nullopt);
            return;
        }
        auto query = pages->queries[pages->index];
        if (pages->page_token) {
            auto token = QString::fromStdString(*pages->page_token);
            token.replace(u'%', QStringLiteral("%25"));
            query.addQueryItem("pageToken", token);
        }
        list(query, [this, guard = owner, pages](auto page, auto error) {
            if (!guard) {
                return;
            }
            if (!page) {
                pages->callback(std::nullopt, std::move(error));
                return;
            }
            std::ranges::move(page->files, std::back_inserter(pages->files));
            pages->page_token = std::move(page->next_page_token);
            if (!pages->page_token) {
                ++pages->index;
            }
            next_page(pages);
        });
    }

    void pages(std::vector<QUrlQuery> queries, ResultCallback<std::vector<DriveFile>> callback) {
        next_page(
            std::make_shared<Pages>(Pages{std::move(queries), 0, {}, {}, std::move(callback)}));
    }

    void create_folder(const std::string& title, const std::string& parent, QUrlQuery query,
                       ResultCallback<std::string> callback) {
        const auto body = json({{"name", QString::fromStdString(title)},
                                {"mimeType", folder_mime},
                                {"parents", QJsonArray{QString::fromStdString(parent)}}});
        transport.request(DriveMethod::post, files_path, query, body, "application/json",
                          [callback = std::move(callback)](DriveResponse response) {
                              if (!response.body) {
                                  callback(std::nullopt, std::move(response.error));
                                  return;
                              }
                              const auto id =
                                  QJsonDocument::fromJson(*response.body).object().value("id");
                              if (id.isString()) {
                                  callback(id.toString().toStdString(), std::nullopt);
                              } else {
                                  callback(std::nullopt, GoogleDriveError{});
                              }
                          });
    }

    void upload(const std::string& folder, const std::optional<std::string>& id,
                const std::string& name, std::string_view mime, QByteArrayView data,
                ErrorCallback callback) {
        QJsonObject metadata{{"name", QString::fromStdString(name)}};
        if (!id) {
            metadata.insert("parents", QJsonArray{QString::fromStdString(folder)});
        }
        auto multipart = build_multipart_related(json(metadata), mime, data);
        transport.request(id ? DriveMethod::patch : DriveMethod::post,
                          id ? std::string(upload_path) + "/" + *id : upload_path,
                          query_item("uploadType", "multipart"), std::move(multipart.body),
                          std::move(multipart.content_type),
                          [callback = std::move(callback)](DriveResponse response) {
                              callback(std::move(response.error));
                          });
    }

    template <typename T>
    void get_media(const std::string& id, bool (*parser)(QByteArrayView, T&, std::string&),
                   ResultCallback<T> callback) {
        transport.request(DriveMethod::get, std::string(files_path) + "/" + id,
                          query_item("alt", "media"), {}, {}, decode(parser, std::move(callback)));
    }
};

TtuDrive::TtuDrive(DriveTransport& transport, QObject* parent)
    : QObject(parent), state_(std::make_unique<State>(State{this, transport, {}, {}})) {}

TtuDrive::~TtuDrive() = default;

void TtuDrive::find_root_folder(ResultCallback<std::string> callback) {
    if (state_->root) {
        callback(state_->root, std::nullopt);
        return;
    }
    auto cache = [this, guard = QPointer(this), callback = std::move(callback)](auto id,
                                                                                auto error) {
        if (guard) {
            if (id) {
                state_->root = id;
            }
            callback(std::move(id), std::move(error));
        }
    };
    state_->list(
        list_query(folder_filter("root") + " and name = 'ttu-reader-data'", "files(id, name)"),
        [this, guard = QPointer(this),
         cache = ResultCallback<std::string>(std::move(cache))](auto list, auto error) mutable {
            if (!guard) {
                return;
            }
            if (!list) {
                cache(std::nullopt, std::move(error));
            } else if (!list->files.empty()) {
                cache(list->files.front().id, std::nullopt);
            } else {
                state_->create_folder("ttu-reader-data", "root", query_item("fields", "id"),
                                      std::move(cache));
            }
        });
}

void TtuDrive::list_books(std::string root_folder,
                          ResultCallback<std::vector<DriveFile>> callback) {
    state_->pages({list_query(folder_filter(root_folder), "nextPageToken, files(id, name)")},
                  std::move(callback));
}

void TtuDrive::list_sync_files(std::vector<std::string> folder_ids,
                               ResultCallback<FolderFiles> callback) {
    std::vector<QUrlQuery> queries;
    for (std::size_t start = 0; start < folder_ids.size(); start += 50) {
        std::string filter = "trashed=false and (";
        for (auto index = start; index < std::min(start + 50, folder_ids.size()); ++index) {
            if (index != start) {
                filter += " or ";
            }
            filter += "'" + folder_ids[index] + "' in parents";
        }
        filter += ") and mimeType != '" + std::string(folder_mime) + "'";
        queries.push_back(
            list_query(filter, "nextPageToken, files(id, name, parents, thumbnailLink)"));
    }
    state_->pages(std::move(queries), [callback = std::move(callback)](auto files, auto error) {
        if (!files) {
            callback(std::nullopt, std::move(error));
            return;
        }
        std::map<std::string, std::vector<DriveFile>> grouped;
        for (auto& file : *files) {
            if (file.parents && !file.parents->empty()) {
                grouped[file.parents->front()].push_back(std::move(file));
            }
        }
        FolderFiles result;
        for (auto& [folder, group] : grouped) {
            result.try_emplace(folder, std::move(group));
        }
        callback(std::move(result), std::nullopt);
    });
}

void TtuDrive::list_sync_files(std::string folder_id, ResultCallback<DriveSyncFiles> callback) {
    auto ids = std::vector{folder_id};
    list_sync_files(
        std::move(ids),
        [id = std::move(folder_id), callback = std::move(callback)](auto files, auto error) {
            if (!files) {
                callback(std::nullopt, std::move(error));
                return;
            }
            auto found = files->find(id);
            callback(found == files->end() ? DriveSyncFiles({}) : std::move(found->second),
                     std::nullopt);
        });
}

void TtuDrive::download_file(std::string file_id, DriveTransport::Progress progress,
                             ResultCallback<QByteArray> callback) {
    state_->transport.download(file_id, std::move(progress),
                               [callback = std::move(callback)](DriveResponse response) {
                                   callback(std::move(response.body), std::move(response.error));
                               });
}

void TtuDrive::trash_file(std::string file_id, ErrorCallback callback) {
    state_->transport.request(DriveMethod::patch, std::string(files_path) + "/" + file_id,
                              query_item("fields", "id"), R"({"trashed":true})", "application/json",
                              [callback = std::move(callback)](DriveResponse response) {
                                  callback(std::move(response.error));
                              });
}

void TtuDrive::get_progress_file(std::string file_id, ResultCallback<TtuProgress> callback) {
    state_->get_media(file_id, parse_ttu_progress, std::move(callback));
}

void TtuDrive::get_stats_file(std::string file_id, ResultCallback<Rows> callback) {
    state_->get_media(file_id, decode_statistics, std::move(callback));
}

void TtuDrive::get_audio_book_file(std::string file_id, ResultCallback<TtuAudioBook> callback) {
    state_->get_media(file_id, parse_ttu_audio_book, std::move(callback));
}

void TtuDrive::update_progress_file(std::string folder_id, std::optional<std::string> file_id,
                                    TtuProgress value, ErrorCallback callback) {
    const auto name = "progress_1_6_" + std::to_string(value.last_bookmark_modified) + "_" +
        describe_double(value.progress) + ".json";
    state_->upload(folder_id, file_id, name, "application/json", encode_ttu_progress(value),
                   std::move(callback));
}

void TtuDrive::update_stats_file(std::string folder_id, std::optional<std::string> file_id,
                                 Rows value, ErrorCallback callback) {
    state_->upload(folder_id, file_id, ttu_statistics_file_name(value), "application/json",
                   encode_statistics(value), std::move(callback));
}

void TtuDrive::update_audio_book_file(std::string folder_id, std::optional<std::string> file_id,
                                      TtuAudioBook value, ErrorCallback callback) {
    const auto name = "audioBook_1_6_" + std::to_string(value.last_audio_book_modified) + "_" +
        describe_double(value.playback_position) + ".json";
    state_->upload(folder_id, file_id, name, "application/json", encode_ttu_audio_book(value),
                   std::move(callback));
}

void TtuDrive::ensure_book_folder(std::string title, std::string root_folder, CoverProvider cover,
                                  ResultCallback<std::string> callback) {
    auto sanitized = sanitize_ttu_title(title);
    const auto cached = state_->folders.find(sanitized);
    if (cached != state_->folders.end()) {
        callback(cached->second, std::nullopt);
        return;
    }
    auto query = list_query(folder_filter(root_folder) + " and name=\"" + sanitized + "\"",
                            "files(id, name)");
    state_->list(
        query,
        [this, guard = QPointer(this), title = std::move(sanitized), root = std::move(root_folder),
         cover = std::move(cover), callback = std::move(callback)](auto list, auto error) mutable {
            if (!guard) {
                return;
            }
            if (!list) {
                callback(std::nullopt, std::move(error));
                return;
            }
            if (!list->files.empty()) {
                auto id = list->files.front().id;
                state_->folders[title] = id;
                callback(std::move(id), std::nullopt);
                return;
            }
            state_->create_folder(
                title, root, {},
                [this, guard, title, cover = std::move(cover),
                 callback = std::move(callback)](auto id, auto failure) mutable {
                    if (!guard) {
                        return;
                    }
                    if (!id) {
                        callback(std::nullopt, std::move(failure));
                        return;
                    }
                    state_->folders[title] = *id;
                    auto bytes = cover.data ? cover.data() : std::nullopt;
                    if (!guard) {
                        return;
                    }
                    if (!bytes) {
                        callback(std::move(id), std::nullopt);
                        return;
                    }
                    const std::string extension(cover_extension(*bytes));
                    auto folder = *id;
                    state_->upload(folder, std::nullopt, "cover_1_6." + extension,
                                   "image/" + extension, *bytes,
                                   [id = std::move(id), callback = std::move(callback)](auto) {
                                       callback(id, std::nullopt);
                                   });
                });
        });
}

void TtuDrive::upload_book_data(std::string folder_id, std::filesystem::path zip,
                                ErrorCallback callback) {
    QFile file(QString::fromStdString(zip.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        callback(GoogleDriveError{
            GoogleDriveErrorKind::unavailable, file.errorString().toStdString(), {}});
        return;
    }
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        callback(GoogleDriveError{
            GoogleDriveErrorKind::unavailable, file.errorString().toStdString(), {}});
        return;
    }
    state_->upload(folder_id, std::nullopt, zip.filename().string(), "application/zip", bytes,
                   std::move(callback));
}

void TtuDrive::clear_cache() {
    state_->root.reset();
    state_->folders.clear();
}

}  // namespace niratan::sync
