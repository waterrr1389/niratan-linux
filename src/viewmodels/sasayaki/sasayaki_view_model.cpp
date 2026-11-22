// SPDX-License-Identifier: GPL-3.0-or-later

#include "sasayaki_view_model.h"

#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "models/sasayaki/sasayaki_settings.h"
#include "sasayaki_player_policy.h"
#include "sasayaki_view_model_state.h"
#include "services/books/book_storage.h"
#include "services/sasayaki/cue_audio.h"
#include "services/settings/global_settings.h"
#include "services/sasayaki/cue_timeline.h"
#include "services/sasayaki/srt_parser.h"
#include "services/storage/app_paths.h"
#include "viewmodels/reader/reader_view_model.h"

namespace niratan::viewmodels {
namespace {

using niratan::sasayaki::CueTimeline;
using niratan::sasayaki::SasayakiCue;
using niratan::sasayaki::SasayakiPlaybackData;


niratan::books::BookStorage book_storage() {
    return niratan::books::BookStorage(niratan::storage::data_root());
}

// The reader exposes the folder of the book it has open, so the sidecars are
// found without comparing display titles.
std::optional<std::string> resolve_book_folder() {
    const ReaderViewModel* reader = ReaderViewModel::instance();
    if (!reader->has_book()) {
        return std::nullopt;
    }
    std::string folder = reader->book_folder().toStdString();
    if (!book_storage().has_book_directory(folder)) {
        return std::nullopt;
    }
    return folder;
}

}  // namespace
SasayakiViewModel::SasayakiViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->player = new QMediaPlayer(this);
    state_->output = new QAudioOutput(this);
    state_->player->setAudioOutput(state_->output);

    state_->seek_timer = new QTimer(this);
    state_->seek_timer->setSingleShot(true);
    connect(state_->seek_timer, &QTimer::timeout, this, [this]() {
        // The landing tick never came: drop the target and its options
        // without applying them.
        state_->pending_seek.reset();
        state_->seek_start_playback = false;
        state_->seek_update_cue = true;
        state_->seek_stop_time.reset();
    });

    connect(state_->player, &QMediaPlayer::positionChanged, this, [this](qint64 milliseconds) {
        if (state_->reloading) {
            return;
        }
        position_tick(static_cast<double>(milliseconds) / 1000.0);
    });

    connect(state_->player, &QMediaPlayer::durationChanged, this, [this](qint64 milliseconds) {
        const double seconds = static_cast<double>(milliseconds) / 1000.0;
        if (!std::isfinite(seconds) || seconds < 0.0 || seconds == state_->duration) {
            return;
        }
        state_->duration = seconds;
        emit durationChanged();
    });

    connect(state_->player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState playback_state) {
                if (state_->reloading) {
                    return;
                }
                const bool playing = playback_state == QMediaPlayer::PlayingState;
                if (playing != state_->playing) {
                    state_->playing = playing;
                    emit playingChanged();
                }
                if (playback_state == QMediaPlayer::PausedState) {
                    persist_position(static_cast<double>(state_->player->position()) / 1000.0);
                }
            });

    connect(state_->player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (state_->reloading) {
                    return;
                }
                if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
                    state_->player->setPlaybackRate(state_->rate);
                    if (state_->needs_initial_seek) {
                        state_->needs_initial_seek = false;
                        seek(state_->position);
                    }
                    update_cue();
                } else if (status == QMediaPlayer::EndOfMedia) {
                    persist_position(static_cast<double>(state_->player->position()) / 1000.0);
                } else if (status == QMediaPlayer::InvalidMedia) {
                    const QString message = state_->player->errorString();
                    set_error(message.isEmpty() ? tr("The audio file could not be played.")
                                                : message);
                }
            });

    connect(state_->player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& message) {
                set_error(message.isEmpty() ? tr("The audio file could not be played.") : message);
            });

    connect(ReaderViewModel::instance(), &ReaderViewModel::bookChanged, this,
            [this]() { reload_book(); });
    connect(ReaderViewModel::instance(), &ReaderViewModel::chapterChanged, this, [this]() {
        rebuild_cue_list();
        update_cue();
    });

    reload_book();
}

SasayakiViewModel::~SasayakiViewModel() {
    if (state_->match_worker.joinable()) {
        // The worker may still queue a completion. Join while this QObject
        // can receive it; QObject destruction removes the unprocessed event.
        state_->match_worker.request_stop();
        state_->match_worker.join();
    }
    // Swift's teardown flushes first: a pending target is written as-is, a
    // paused player writes nothing.
    flushPlayback();
    if (state_->player != nullptr) {
        state_->player->stop();
    }
}

bool SasayakiViewModel::has_audio() const { return state_->has_audio; }

bool SasayakiViewModel::has_match() const { return state_->has_match; }

bool SasayakiViewModel::playing() const { return state_->playing; }

double SasayakiViewModel::position() const { return state_->position; }

double SasayakiViewModel::duration() const { return state_->duration; }

double SasayakiViewModel::rate() const { return state_->rate; }

double SasayakiViewModel::delay() const { return state_->delay; }

QString SasayakiViewModel::current_cue_id() const { return state_->current_cue_id; }

QString SasayakiViewModel::current_cue_text() const { return state_->current_cue_text; }

QVariantList SasayakiViewModel::cues() const { return state_->cue_list; }

QString SasayakiViewModel::error() const { return state_->error; }

QString SasayakiViewModel::audio_name() const { return state_->audio_name; }

QString SasayakiViewModel::subtitle_name() const { return state_->subtitle_name; }

bool SasayakiViewModel::openAudio(const QUrl& file) {
    QString path;
    if (!take_local_file(file, &path)) {
        return false;
    }
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        set_error(tr("The audio file does not exist: %1").arg(path));
        return false;
    }
    if (!state_->has_root) {
        reload_book();
    }
    if (!state_->has_root) {
        set_error(tr("Open a book before loading audio."));
        return false;
    }

    save_playback(state_->position);
    state_->player->stop();
    state_->audio_path = info.absoluteFilePath();
    state_->audio_name = info.fileName();
    state_->playback.audio_bookmark = state_->audio_path.toUtf8();
    state_->has_audio = true;
    if (state_->cue_audio_exporter) {
        state_->cue_audio_exporter->clear_cache();
    }
    state_->duration = 0.0;
    state_->last_second = static_cast<int>(std::floor(state_->position));
    open_audio_source();
    save_playback(state_->position);

    set_error(QString());
    emit hasAudioChanged();
    emit audioNameChanged();
    emit durationChanged();
    return true;
}

bool SasayakiViewModel::openSubtitles(const QUrl& file) {
    QString path;
    if (!take_local_file(file, &path)) {
        return false;
    }
    std::optional<std::vector<SasayakiCue>> read =
        niratan::sasayaki::read_cue_file(path.toStdString());
    if (!read.has_value()) {
        set_error(tr("The subtitle file could not be read: %1").arg(path));
        return false;
    }
    std::vector<SasayakiCue> cues = std::move(*read);
    if (cues.empty()) {
        set_error(tr("No cues were found in the subtitle file: %1").arg(path));
        return false;
    }

    cancelSubtitleMatch();
    state_->subtitle_cues = std::move(cues);
    state_->subtitle_name = QFileInfo(path).fileName();
    set_match_error({});
    emit subtitleNameChanged();
    rebuild_timeline();
    set_error(QString());
    return true;
}

void SasayakiViewModel::reload_book() {
    cancelSubtitleMatch();
    if (state_->cue_audio_exporter) {
        state_->cue_audio_exporter->clear_cache();
    }
    reloadSasayakiSettings();
    state_->reloading = true;
    if (state_->has_root) {
        save_playback(state_->position);
    }
    state_->player->stop();
    state_->player->setSource(QUrl());

    state_->folder.clear();
    state_->has_root = false;
    state_->playback = SasayakiPlaybackData{};
    state_->match.reset();
    state_->match_cues.clear();
    state_->subtitle_cues.clear();
    state_->subtitle_name.clear();
    state_->match_error.clear();
    state_->timeline = CueTimeline();
    state_->audio_path.clear();
    state_->audio_name.clear();
    state_->current_cue_id.clear();
    state_->current_cue_text.clear();
    state_->cue_list.clear();
    state_->position = 0.0;
    state_->duration = 0.0;
    state_->rate = 1.0;
    state_->delay = 0.0;
    state_->has_audio = false;
    state_->has_match = false;
    state_->playing = false;
    state_->needs_initial_seek = false;
    state_->pending_seek.reset();
    state_->seek_start_playback = false;
    state_->seek_update_cue = true;
    state_->seek_stop_time.reset();
    state_->stop_playback_time.reset();
    state_->pending_cue.reset();
    state_->reveal_pending_cue_on_restore = false;
    state_->chapter_transition = false;
    state_->should_resume = false;
    state_->last_second = -1;
    state_->audiobook_metadata = {};
    state_->audiobook_chapters.clear();
    state_->loading_audiobook_chapters = false;
    state_->last_audiobook_chapter_id = -1;
    state_->error.clear();

    std::optional<std::string> folder = resolve_book_folder();
    if (folder.has_value()) {
        state_->folder = std::move(*folder);
        state_->has_root = true;
        load_playback();
        load_match();
        if (state_->has_match) {
            state_->timeline = CueTimeline(state_->match_cues);
        }
        rebuild_cue_list();
        restore_audio();
    } else if (ReaderViewModel::instance()->has_book()) {
        state_->error = tr("The folder for the open book could not be found.");
    }

    state_->reloading = false;

    emit hasAudioChanged();
    emit hasMatchChanged();
    emit matchDataChanged();
    emit matchErrorChanged();
    emit subtitleNameChanged();
    emit playingChanged();
    emit positionChanged();
    emit durationChanged();
    emit rateChanged();
    emit delayChanged();
    emit currentCueChanged();
    emit cuesChanged();
    emit chaptersChanged();
    emit audiobookMetadataChanged();
    emit audiobookChaptersChanged();
    emit audioNameChanged();
    emit errorChanged();
    update_cue();
}

void SasayakiViewModel::reloadSasayakiSettings() {
    const sasayaki::SasayakiSettings settings =
        settings::GlobalSettings(book_storage().app_directory() / "settings.json")
            .sasayaki_settings();
    state_->auto_scroll = settings.auto_scroll;
    state_->auto_pause = settings.auto_pause;
}

void SasayakiViewModel::load_playback() {
    state_->playback =
        book_storage().load_sasayaki_playback(state_->folder).value_or(SasayakiPlaybackData{});
    state_->position = std::max(0.0, state_->playback.last_position);
    state_->delay = state_->playback.delay;
    // A stored speed outside the control range can come from another client;
    // QMediaPlayer rejects a non-positive rate, so keep it inside the range.
    state_->rate = clamp_rate(static_cast<double>(state_->playback.rate));
    state_->last_second = static_cast<int>(std::floor(state_->position));
}

void SasayakiViewModel::restore_audio() {
    if (!state_->playback.audio_bookmark.has_value()) {
        return;
    }
    const QString path = QString::fromUtf8(*state_->playback.audio_bookmark);
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        set_error(tr("The saved audio file is missing: %1").arg(path));
        return;
    }
    state_->audio_path = info.absoluteFilePath();
    state_->audio_name = info.fileName();
    state_->has_audio = true;
    if (state_->cue_audio_exporter) {
        state_->cue_audio_exporter->clear_cache();
    }
    open_audio_source();
    emit hasAudioChanged();
    emit audioNameChanged();
}

void SasayakiViewModel::rebuild_timeline(bool refresh_highlight) {
    state_->timeline = CueTimeline(state_->has_match ? state_->match_cues : state_->subtitle_cues);
    rebuild_cue_list();
    update_cue(refresh_highlight);
}

void SasayakiViewModel::set_error(const QString& message) {
    if (state_->error == message) {
        return;
    }
    state_->error = message;
    emit errorChanged();
}

bool SasayakiViewModel::take_local_file(const QUrl& file, QString* path) {
    const QString local = file.toLocalFile();
    if (!file.isLocalFile() || local.isEmpty()) {
        set_error(tr("Only local files can be loaded."));
        return false;
    }
    *path = local;
    return true;
}

}  // namespace niratan::viewmodels
