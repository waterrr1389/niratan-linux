// SPDX-License-Identifier: GPL-3.0-or-later

#include "sasayaki_view_model.h"

#include <QAudioOutput>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "services/books/book_storage.h"
#include "services/sasayaki/cue_timeline.h"
#include "services/sasayaki/srt_parser.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"
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
constexpr const char* kPlaybackFileName = "sasayaki_playback.json";
constexpr const char* kMatchFileName = "sasayaki_match.json";

// The reader exposes the folder of the book it has open, so the sidecars are
// found without comparing display titles.
std::optional<std::filesystem::path> resolve_book_root() {
    const ReaderViewModel* reader = ReaderViewModel::instance();
    if (!reader->has_book()) {
        return std::nullopt;
    }
    niratan::books::BookStorage storage(niratan::storage::data_root());
    const std::optional<std::filesystem::path> directory =
        storage.book_directory(reader->book_folder().toStdString());
    std::error_code error;
    if (!directory.has_value() || !std::filesystem::is_directory(*directory, error)) {
        return std::nullopt;
    }
    return directory;
}

QVariantMap cue_entry(const SasayakiCue& cue) {
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), QString::fromStdString(cue.id));
    entry.insert(QStringLiteral("start"), cue.start_time);
    entry.insert(QStringLiteral("end"), cue.end_time);
    entry.insert(QStringLiteral("text"), QString::fromStdString(cue.text));
    return entry;
}

}  // namespace

struct SasayakiViewModel::State {
    QMediaPlayer* player = nullptr;
    QAudioOutput* output = nullptr;
    QTimer* seek_timer = nullptr;

    std::filesystem::path root;
    bool has_root = false;

    SasayakiPlaybackData playback;
    std::optional<SasayakiMatchData> match;
    std::vector<SasayakiCue> match_cues;
    std::vector<SasayakiCue> subtitle_cues;
    CueTimeline timeline;

    QString audio_path;
    QString audio_name;
    QString error;
    QString current_cue_id;
    QString current_cue_text;
    QVariantList cue_list;

    double position = 0.0;
    double duration = 0.0;
    double rate = 1.0;
    double delay = 0.0;

    bool has_audio = false;
    bool has_match = false;
    bool playing = false;
    bool reloading = false;
    bool needs_initial_seek = false;
    std::optional<double> pending_seek;
    int last_second = -1;
};

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

QVariantList SasayakiViewModel::chapters() const { return {}; }

QString SasayakiViewModel::error() const { return state_->error; }

QString SasayakiViewModel::audio_name() const { return state_->audio_name; }

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
    QFile handle(path);
    if (!handle.open(QIODevice::ReadOnly)) {
        set_error(tr("The subtitle file could not be read: %1").arg(path));
        return false;
    }
    const QByteArray bytes = handle.readAll();
    std::vector<SasayakiCue> cues = niratan::sasayaki::parse_cues(
        std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
    if (cues.empty()) {
        set_error(tr("No cues were found in the subtitle file: %1").arg(path));
        return false;
    }

    state_->subtitle_cues = std::move(cues);
    rebuild_timeline();
    set_error(QString());
    return true;
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
    state_->reloading = true;
    if (state_->has_root) {
        save_playback();
    }
    state_->player->stop();
    state_->player->setSource(QUrl());

    state_->root.clear();
    state_->has_root = false;
    state_->playback = SasayakiPlaybackData{};
    state_->match.reset();
    state_->match_cues.clear();
    state_->subtitle_cues.clear();
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
    state_->error.clear();

    const std::optional<std::filesystem::path> root = resolve_book_root();
    if (root.has_value()) {
        state_->root = *root;
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
    emit playingChanged();
    emit positionChanged();
    emit durationChanged();
    emit rateChanged();
    emit delayChanged();
    emit currentCueChanged();
    emit cuesChanged();
    emit chaptersChanged();
    emit audioNameChanged();
    emit errorChanged();
    update_cue();
}

void SasayakiViewModel::load_playback() {
    state_->playback = SasayakiPlaybackData{};
    const niratan::storage::JsonReadResult read =
        niratan::storage::read_json_file(state_->root / kPlaybackFileName);
    if (read.status == niratan::storage::JsonReadStatus::success) {
        if (const auto decoded = niratan::sasayaki::playback_data_from_json(read.document)) {
            state_->playback = *decoded;
        }
    }
    state_->position = std::max(0.0, state_->playback.last_position);
    state_->delay = state_->playback.delay;
    // A stored speed outside the control range can come from another client;
    // QMediaPlayer rejects a non-positive rate, so keep it inside the range.
    state_->rate =
        std::clamp(static_cast<double>(state_->playback.rate), kMinimumRate, kMaximumRate);
    state_->last_second = static_cast<int>(std::floor(state_->position));
}

void SasayakiViewModel::load_match() {
    state_->match_cues.clear();
    const niratan::storage::JsonReadResult read =
        niratan::storage::read_json_file(state_->root / kMatchFileName);
    if (read.status != niratan::storage::JsonReadStatus::success) {
        return;
    }
    const auto decoded = niratan::sasayaki::match_data_from_json(read.document);
    if (!decoded.has_value()) {
        return;
    }
    state_->match = *decoded;
    state_->has_match = true;
    state_->match_cues.reserve(decoded->matches.size());
    for (const niratan::sasayaki::SasayakiMatch& match : decoded->matches) {
        SasayakiCue cue;
        cue.id = match.id;
        cue.start_time = match.start_time;
        cue.end_time = match.end_time;
        cue.text = match.text;
        state_->match_cues.push_back(std::move(cue));
    }
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

void SasayakiViewModel::open_audio_source() {
    state_->needs_initial_seek = true;
    state_->player->setSource(QUrl::fromLocalFile(state_->audio_path));
    state_->player->setPlaybackRate(state_->rate);
}

void SasayakiViewModel::rebuild_timeline() {
    state_->timeline = CueTimeline(state_->has_match ? state_->match_cues : state_->subtitle_cues);
    state_->current_cue_id.clear();
    state_->current_cue_text.clear();
    rebuild_cue_list();
    update_cue();
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

void SasayakiViewModel::update_cue() {
    QString id;
    QString text;
    if (state_->has_audio && !state_->timeline.empty()) {
        if (const SasayakiCue* cue =
                state_->timeline.cue_at(state_->position - state_->delay)) {
            id = QString::fromStdString(cue->id);
            text = QString::fromStdString(cue->text);
        }
    }
    if (id == state_->current_cue_id) {
        return;
    }
    state_->current_cue_id = id;
    state_->current_cue_text = text;
    emit currentCueChanged();
    emit activeCueChanged(id);
}

void SasayakiViewModel::apply_position(double seconds, bool persist) {
    if (!std::isfinite(seconds)) {
        return;
    }
    state_->position = std::max(0.0, seconds);
    emit positionChanged();
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
    niratan::storage::write_json_file(state_->root / kPlaybackFileName,
                                      niratan::sasayaki::playback_data_to_json(data));
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
