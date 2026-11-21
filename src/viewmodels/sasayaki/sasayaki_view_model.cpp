// SPDX-License-Identifier: GPL-3.0-or-later

#include "sasayaki_view_model.h"

#include <QAudioOutput>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <exception>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "sasayaki_view_model_state.h"
#include "services/audiobook/audiobook_metadata.h"
#include "services/books/book_storage.h"
#include "services/reader/book_session.h"
#include "services/reader/character_normalizer.h"
#include "services/sasayaki/cue_timeline.h"
#include "services/sasayaki/matcher.h"
#include "services/sasayaki/srt_parser.h"
#include "services/storage/app_paths.h"
#include "viewmodels/reader/reader_view_model.h"

namespace niratan::viewmodels {
namespace {

using niratan::sasayaki::CueTimeline;
using niratan::sasayaki::SasayakiCue;
using niratan::sasayaki::SasayakiMatchData;
using niratan::sasayaki::SasayakiPlaybackData;

constexpr double kSeekLandingTolerance = 0.75;
constexpr double kMinimumRate = 0.5;
constexpr double kMaximumRate = 2.5;
constexpr double kMinimumDelay = -2.0;
constexpr double kMaximumDelay = 2.0;
constexpr int kDefaultSearchWindow = 200;
constexpr int kMinimumSearchWindow = 50;
constexpr int kMaximumSearchWindow = 1000;
constexpr int kSearchWindowIncrement = 50;

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

QVariantMap cue_entry(const SasayakiCue& cue) {
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), QString::fromStdString(cue.id));
    entry.insert(QStringLiteral("start"), cue.start_time);
    entry.insert(QStringLiteral("end"), cue.end_time);
    entry.insert(QStringLiteral("text"), QString::fromStdString(cue.text));
    return entry;
}

int audiobook_chapter_at(const std::vector<sasayaki::SasayakiAudiobookChapter>& chapters,
                         double position) {
    return audiobook::current_audiobook_chapter_id(chapters, position).value_or(-1);
}

}  // namespace

SasayakiViewModel::SasayakiViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->player = new QMediaPlayer(this);
    state_->output = new QAudioOutput(this);
    state_->player->setAudioOutput(state_->output);

    state_->seek_timer = new QTimer(this);
    state_->seek_timer->setSingleShot(true);
    connect(state_->seek_timer, &QTimer::timeout, this,
            [this]() { state_->pending_seek.reset(); });

    connect(state_->player, &QMediaPlayer::positionChanged, this, [this](qint64 milliseconds) {
        if (state_->reloading) {
            return;
        }
        const double seconds = static_cast<double>(milliseconds) / 1000.0;
        if (state_->pending_seek.has_value()) {
            if (std::abs(seconds - *state_->pending_seek) > kSeekLandingTolerance) {
                return;
            }
            state_->pending_seek.reset();
            state_->seek_timer->stop();
        }
        apply_position(seconds, true);
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
    // Swift's flushPlayback samples the player only while it is playing;
    // otherwise currentTime already holds the last tick or seek target.
    if (state_->has_audio && state_->playing && state_->player != nullptr) {
        const double seconds = static_cast<double>(state_->player->position()) / 1000.0;
        if (std::isfinite(seconds)) {
            state_->position = std::max(0.0, seconds);
        }
    }
    save_playback();
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

bool SasayakiViewModel::matching() const { return state_->match_worker.joinable(); }

double SasayakiViewModel::match_progress() const { return state_->match_progress; }

int SasayakiViewModel::matched_count() const {
    return state_->match ? static_cast<int>(state_->match->matches.size()) : 0;
}

int SasayakiViewModel::unmatched_count() const {
    return state_->match ? state_->match->unmatched : 0;
}

int SasayakiViewModel::inferred_count() const {
    if (!state_->match) {
        return 0;
    }
    return static_cast<int>(std::ranges::count_if(state_->match->matches,
                                                &sasayaki::SasayakiMatch::context_inferred));
}

QString SasayakiViewModel::match_rate_label() const {
    const int matched = matched_count();
    const auto total = static_cast<qint64>(matched) + unmatched_count();
    const double percentage = total > 0 ? 100.0 * matched / static_cast<double>(total) : 0.0;
    return tr("%1/%2 (%3%)").arg(matched).arg(total).arg(percentage, 0, 'f', 1);
}

QString SasayakiViewModel::match_error() const { return state_->match_error; }

int SasayakiViewModel::match_search_window() const { return state_->match_search_window; }

void SasayakiViewModel::set_match_search_window(int value) {
    const int clamped = std::clamp(value, kMinimumSearchWindow, kMaximumSearchWindow);
    const int rounded =
        ((clamped + kSearchWindowIncrement / 2) / kSearchWindowIncrement) * kSearchWindowIncrement;
    if (rounded != state_->match_search_window) {
        state_->match_search_window = rounded;
        emit matchSearchWindowChanged();
    }
}

void SasayakiViewModel::reset_match_search_window() {
    set_match_search_window(kDefaultSearchWindow);
}

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

    save_playback();
    state_->player->stop();
    state_->audio_path = info.absoluteFilePath();
    state_->audio_name = info.fileName();
    state_->playback.audio_bookmark = state_->audio_path.toUtf8();
    state_->has_audio = true;
    state_->duration = 0.0;
    state_->last_second = static_cast<int>(std::floor(state_->position));
    open_audio_source();
    save_playback();

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

void SasayakiViewModel::matchSubtitles(int searchWindow) {
    if (matching()) {
        // A programmatic retry supersedes the current run without overlapping it.
        cancelSubtitleMatch();
        state_->pending_match_window = searchWindow;
        return;
    }
    if (state_->subtitle_cues.empty()) {
        set_match_error(tr("Load a subtitle file first."));
        return;
    }
    if (!state_->has_root) {
        set_match_error(tr("The EPUB for the open book could not be found."));
        return;
    }

    set_match_search_window(searchWindow);
    const int window = state_->match_search_window;
    const auto folder = ReaderViewModel::instance()->book_folder().toStdString();
    const auto data_root = niratan::storage::data_root();
    set_match_error({});
    set_match_progress(0.0);

    try {
        state_->match_worker = std::jthread(
            [this, folder, data_root, window, cues = state_->subtitle_cues](std::stop_token stop) {
                std::optional<SasayakiMatchData> result;
                QString error;
                try {
                    const niratan::books::BookStorage storage(data_root);
                    const auto metadata = storage.load_metadata(folder);
                    const auto epub = metadata ? storage.epub_path(*metadata) : std::nullopt;
                    const auto book = epub && !stop.stop_requested()
                                          ? niratan::reader::BookSession::open(*epub)
                                          : std::nullopt;
                    if (!book) {
                        error = tr("The EPUB for the open book could not be opened.");
                    } else {
                        std::vector<sasayaki::MatchChapter> chapters;
                        for (int index = 0; index < book->spine_count() && !stop.stop_requested();
                             ++index) {
                            if (const auto entry = book->book().read(book->spine_path(index))) {
                                chapters.push_back({index, reader::filtered_text(*entry)});
                            }
                        }
                        if (!stop.stop_requested()) {
                            QMetaObject::invokeMethod(
                                this,
                                [this, stop]() {
                                    if (!stop.stop_requested()) {
                                        set_match_progress(0.5);
                                    }
                                },
                                Qt::QueuedConnection);
                            result = sasayaki::match_cues(
                                cues, chapters, window, [&stop]() { return stop.stop_requested(); });
                        }
                    }
                } catch (const std::exception&) {
                    error = tr("Could not match subtitles.");
                }

                QMetaObject::invokeMethod(
                    this,
                    [this, stop, result = std::move(result), error = std::move(error)]() {
                        // Keep one worker at a time even while it is unwinding
                        // after cancellation. Never save a stale result.
                        if (!stop.stop_requested()) {
                            if (!result || !error.isEmpty()) {
                                set_match_error(error.isEmpty() ? tr("Could not match subtitles.")
                                                               : error);
                            } else {
                                apply_match_result(*result);
                            }
                        }
                        if (state_->match_progress != 1.0) {
                            set_match_progress(0.0);
                        }
                        state_->match_worker.join();
                        emit matchingChanged();
                        if (const auto pending = std::exchange(state_->pending_match_window,
                                                               std::nullopt)) {
                            matchSubtitles(*pending);
                        }
                    },
                    Qt::QueuedConnection);
            });
    } catch (const std::system_error&) {
        set_match_error(tr("Could not start subtitle matching."));
        return;
    }
    emit matchingChanged();
}

void SasayakiViewModel::cancelSubtitleMatch() {
    state_->pending_match_window.reset();
    state_->match_worker.request_stop();
    set_match_progress(0.0);
}

void SasayakiViewModel::apply_match_result(const SasayakiMatchData& result) {
    if (result.matches.empty()) {
        set_match_error(tr("No subtitles matched the book. Existing matches were kept."));
        return;
    }
    std::string reason;
    if (!book_storage().save_sasayaki_match(state_->folder, result, &reason)) {
        set_match_error(tr("Could not save subtitle matches: %1").arg(QString::fromStdString(reason)));
    } else if (!load_match()) {
        set_match_error(tr("Could not reload saved subtitle matches."));
    } else {
        rebuild_timeline(true);
        emit hasMatchChanged();
        emit matchDataChanged();
        set_match_progress(1.0);
    }
}

void SasayakiViewModel::play() {
    if (!state_->has_audio) {
        set_error(tr("Load an audio file first."));
        return;
    }
    state_->player->setPlaybackRate(state_->rate);
    state_->player->play();
}

void SasayakiViewModel::pause() {
    if (!state_->has_audio) {
        return;
    }
    state_->player->pause();
    persist_position(static_cast<double>(state_->player->position()) / 1000.0);
}

void SasayakiViewModel::togglePlayback() {
    if (state_->playing) {
        pause();
    } else {
        play();
    }
}

void SasayakiViewModel::seek(double seconds) {
    if (!state_->has_audio || !std::isfinite(seconds)) {
        return;
    }
    double target = seconds;
    if (state_->duration > 0.0) {
        target = std::clamp(target, 0.0, state_->duration);
    } else {
        target = std::max(0.0, target);
    }
    state_->pending_seek = target;
    state_->seek_timer->start(1500);
    persist_position(target);
    state_->player->setPosition(static_cast<qint64>(std::llround(target * 1000.0)));
}

void SasayakiViewModel::skip(double seconds) {
    if (!state_->has_audio || !std::isfinite(seconds)) {
        return;
    }
    seek(state_->position + seconds);
}

void SasayakiViewModel::previousCue() {
    if (!state_->has_audio || state_->timeline.empty()) {
        return;
    }
    const double lookup = state_->position - state_->delay;
    const SasayakiCue* current = state_->timeline.cue_at(lookup);
    const double reference = current != nullptr ? current->start_time : std::max(0.0, lookup);
    const SasayakiCue* previous = state_->timeline.previous_cue_before(reference);
    if (previous == nullptr) {
        seek(0.0);
        return;
    }
    navigate_to_cue(*previous);
}

void SasayakiViewModel::nextCue() {
    if (!state_->has_audio || state_->timeline.empty()) {
        return;
    }
    const double lookup = state_->position - state_->delay;
    const SasayakiCue* current = state_->timeline.cue_at(lookup);
    const double reference = current != nullptr ? current->start_time : lookup;
    const SasayakiCue* next = state_->timeline.next_cue_after(reference);
    if (next == nullptr) {
        return;
    }
    navigate_to_cue(*next);
}

void SasayakiViewModel::replayCue() {
    if (!state_->has_audio || state_->timeline.empty()) {
        return;
    }
    const SasayakiCue* current = state_->timeline.cue_at(state_->position - state_->delay);
    if (current == nullptr) {
        seek(0.0);
        return;
    }
    navigate_to_cue(*current);
}

void SasayakiViewModel::jumpCue(const QString& cueId) {
    if (!state_->has_audio || state_->timeline.empty()) {
        return;
    }
    const std::vector<SasayakiCue>& cues = state_->timeline.cues();
    for (auto it = cues.rbegin(); it != cues.rend(); ++it) {
        if (QString::fromStdString(it->id) == cueId) {
            navigate_to_cue(*it);
            return;
        }
    }
}

void SasayakiViewModel::setRate(double rate) {
    if (!std::isfinite(rate)) {
        return;
    }
    const double clamped = std::clamp(rate, kMinimumRate, kMaximumRate);
    if (clamped == state_->rate) {
        return;
    }
    state_->rate = clamped;
    state_->player->setPlaybackRate(clamped);
    emit rateChanged();
    save_playback();
}

void SasayakiViewModel::setDelay(double delay) {
    if (!std::isfinite(delay)) {
        return;
    }
    const double clamped = std::clamp(delay, kMinimumDelay, kMaximumDelay);
    if (clamped == state_->delay) {
        return;
    }
    state_->delay = clamped;
    emit delayChanged();
    save_playback();
    update_cue();
}

QString SasayakiViewModel::applyCuesJson() const {
    QJsonArray ranges;
    if (state_->match.has_value()) {
        const int chapter = ReaderViewModel::instance()->chapter_index();
        for (const niratan::sasayaki::SasayakiMatch& match : state_->match->matches) {
            if (match.chapter_index != chapter) {
                continue;
            }
            QJsonObject entry;
            entry.insert(QStringLiteral("id"), QString::fromStdString(match.id));
            entry.insert(QStringLiteral("start"), match.start);
            entry.insert(QStringLiteral("length"), match.length);
            ranges.append(entry);
        }
    }
    return QString::fromUtf8(QJsonDocument(ranges).toJson(QJsonDocument::Compact));
}

void SasayakiViewModel::reload_book() {
    cancelSubtitleMatch();
    state_->reloading = true;
    if (state_->has_root) {
        save_playback();
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

void SasayakiViewModel::load_playback() {
    state_->playback =
        book_storage().load_sasayaki_playback(state_->folder).value_or(SasayakiPlaybackData{});
    state_->position = std::max(0.0, state_->playback.last_position);
    state_->delay = state_->playback.delay;
    // A stored speed outside the control range can come from another client;
    // QMediaPlayer rejects a non-positive rate, so keep it inside the range.
    state_->rate =
        std::clamp(static_cast<double>(state_->playback.rate), kMinimumRate, kMaximumRate);
    state_->last_second = static_cast<int>(std::floor(state_->position));
}

bool SasayakiViewModel::load_match() {
    auto decoded = book_storage().load_sasayaki_match(state_->folder);
    if (!decoded.has_value()) {
        return false;
    }
    state_->match_cues.clear();
    state_->match_cues.reserve(decoded->matches.size());
    for (const niratan::sasayaki::SasayakiMatch& match : decoded->matches) {
        SasayakiCue cue;
        cue.id = match.id;
        cue.start_time = match.start_time;
        cue.end_time = match.end_time;
        cue.text = match.text;
        state_->match_cues.push_back(std::move(cue));
    }
    state_->has_match = !decoded->matches.empty();
    state_->match = std::move(decoded);
    return state_->has_match;
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
    open_audio_source();
    emit hasAudioChanged();
    emit audioNameChanged();
}

void SasayakiViewModel::rebuild_timeline(bool refresh_highlight) {
    state_->timeline = CueTimeline(state_->has_match ? state_->match_cues : state_->subtitle_cues);
    rebuild_cue_list();
    update_cue(refresh_highlight);
}

void SasayakiViewModel::rebuild_cue_list() {
    QVariantList list;
    if (state_->has_match) {
        const int chapter = ReaderViewModel::instance()->chapter_index();
        for (const niratan::sasayaki::SasayakiMatch& match : state_->match->matches) {
            if (match.chapter_index != chapter) {
                continue;
            }
            SasayakiCue cue;
            cue.id = match.id;
            cue.start_time = match.start_time;
            cue.end_time = match.end_time;
            cue.text = match.text;
            list.append(cue_entry(cue));
        }
    } else {
        for (const SasayakiCue& cue : state_->subtitle_cues) {
            list.append(cue_entry(cue));
        }
    }
    state_->cue_list = std::move(list);
    emit cuesChanged();
}

void SasayakiViewModel::update_cue(bool refresh_highlight) {
    QString id;
    QString text;
    if (state_->has_audio && !state_->timeline.empty()) {
        if (const SasayakiCue* cue =
                state_->timeline.cue_at(state_->position - state_->delay)) {
            id = QString::fromStdString(cue->id);
            text = QString::fromStdString(cue->text);
        }
    }
    const bool changed = id != state_->current_cue_id || text != state_->current_cue_text;
    if (changed) {
        state_->current_cue_id = id;
        state_->current_cue_text = text;
        emit currentCueChanged();
    }
    if (refresh_highlight) {
        // Install new ranges even if the playback cue still has the same ID.
        emit matchApplied();
    }
    if (changed || refresh_highlight) {
        emit activeCueChanged(id);
    }
}

void SasayakiViewModel::apply_position(double seconds, bool persist) {
    if (!std::isfinite(seconds)) {
        return;
    }
    state_->position = std::max(0.0, seconds);
    emit positionChanged();
    const int chapter = audiobook_chapter_at(state_->audiobook_chapters, state_->position);
    if (chapter != state_->last_audiobook_chapter_id) {
        state_->last_audiobook_chapter_id = chapter;
        emit audiobookChaptersChanged();
    }
    const int second = static_cast<int>(std::floor(state_->position));
    if (persist && second != state_->last_second) {
        state_->last_second = second;
        save_playback();
    }
    update_cue();
}

void SasayakiViewModel::persist_position(double seconds) {
    if (!std::isfinite(seconds)) {
        return;
    }
    state_->position = std::max(0.0, seconds);
    state_->last_second = static_cast<int>(std::floor(state_->position));
    emit positionChanged();
    const int chapter = audiobook_chapter_at(state_->audiobook_chapters, state_->position);
    if (chapter != state_->last_audiobook_chapter_id) {
        state_->last_audiobook_chapter_id = chapter;
        emit audiobookChaptersChanged();
    }
    save_playback();
    update_cue();
}

void SasayakiViewModel::save_playback() {
    if (!state_->has_root) {
        return;
    }
    SasayakiPlaybackData data;
    data.last_position = state_->position;
    data.delay = state_->delay;
    data.rate = static_cast<float>(state_->rate);
    data.audio_bookmark = state_->playback.audio_bookmark;
    book_storage().save_sasayaki_playback(state_->folder, data);
}

void SasayakiViewModel::navigate_to_cue(const SasayakiCue& cue) {
    if (!state_->has_audio) {
        return;
    }
    seek(niratan::sasayaki::seek_target(cue, state_->delay));
    if (!state_->match.has_value()) {
        return;
    }
    for (const niratan::sasayaki::SasayakiMatch& match : state_->match->matches) {
        if (match.id != cue.id) {
            continue;
        }
        ReaderViewModel* reader = ReaderViewModel::instance();
        if (match.chapter_index != reader->chapter_index()) {
            reader->goToChapter(match.chapter_index);
            // The chapter load re-applies cue ranges, which clears the
            // highlight that the seek just requested.
            emit activeCueChanged(state_->current_cue_id);
        }
        return;
    }
}

void SasayakiViewModel::set_error(const QString& message) {
    if (state_->error == message) {
        return;
    }
    state_->error = message;
    emit errorChanged();
}

void SasayakiViewModel::set_match_error(const QString& message) {
    if (state_->match_error != message) {
        state_->match_error = message;
        emit matchErrorChanged();
    }
}

void SasayakiViewModel::set_match_progress(double progress) {
    if (state_->match_progress != progress) {
        state_->match_progress = progress;
        emit matchProgressChanged();
    }
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
