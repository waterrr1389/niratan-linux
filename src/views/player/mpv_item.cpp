// SPDX-License-Identifier: GPL-3.0-or-later
//
// The libmpv surface behind MpvItem. The item owns the mpv handle and polls
// the properties the player UI binds to on the GUI thread; mpv_renderer.cpp
// owns the render context and the lifetime link between the two.

#include "mpv_item.h"
#include "mpv_renderer.h"

#include "services/player/video_settings.h"
#include "services/storage/app_paths.h"

#include <QByteArray>
#include <QChar>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QMetaObject>
#include <QProcess>
#include <QSize>
#include <QString>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>

#include <mpv/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace {

constexpr int kPollIntervalMs = 100;
constexpr int kLoadRetryMs = 100;
constexpr double kPositionEpsilon = 0.02;
constexpr double kValueEpsilon = 1e-6;
constexpr double kSpeedStep = 0.1;
constexpr double kMaxDelaySeconds = 60.0;
constexpr quint64 kChaptersObservation = 1;
constexpr quint64 kTracksObservation = 2;
constexpr quint64 kDurationObservation = 3;
constexpr qint64 kMaxSubtitleDocumentBytes = 16 * 1024 * 1024;
constexpr int kResumeSaveIntervalMs = 5000;
QVariantMap node_fields(const mpv_node& node) {
    QVariantMap fields;
    if (node.format != MPV_FORMAT_NODE_MAP || node.u.list == nullptr) {
        return fields;
    }
    for (int i = 0; i < node.u.list->num; ++i) {
        const QString key = QString::fromUtf8(node.u.list->keys[i]);
        const mpv_node& value = node.u.list->values[i];
        switch (value.format) {
            case MPV_FORMAT_STRING: fields.insert(key, QString::fromUtf8(value.u.string)); break;
            case MPV_FORMAT_FLAG: fields.insert(key, value.u.flag != 0); break;
            case MPV_FORMAT_INT64: fields.insert(key, qlonglong(value.u.int64)); break;
            case MPV_FORMAT_DOUBLE: fields.insert(key, value.u.double_); break;
            default: break;
        }
    }
    return fields;
}

QVariantList node_list(mpv_handle* handle, const char* name) {
    mpv_node node{};
    if (mpv_get_property(handle, name, MPV_FORMAT_NODE, &node) < 0) {
        return {};
    }
    QVariantList list;
    if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list != nullptr) {
        list.reserve(node.u.list->num);
        for (int i = 0; i < node.u.list->num; ++i) {
            list.append(node_fields(node.u.list->values[i]));
        }
    }
    mpv_free_node_contents(&node);
    return list;
}

}  // namespace

MpvItem::MpvItem(QQuickItem* parent) : QQuickFramebufferObject(parent),
    video_settings_(niratan::player::load_video_settings(niratan::storage::data_root())) {
    niratan::player::link_render_item(this);

    handle_ = mpv_create();
    if (handle_ == nullptr) {
        error_ = QStringLiteral("Could not create the mpv instance");
        return;
    }

    const auto set_option = [this](const char* name, const char* value) {
        mpv_set_option_string(handle_, name, value);
    };
    set_option("config", "no");
    set_option("osc", "no");
    set_option("input-default-bindings", "no");
    set_option("input-cursor", "no");
    set_option("sid", "no");
    set_option("vo", "libmpv");
    set_option("keep-open", "yes");
    set_option("hwdec", "auto-safe");
    set_option("terminal", "no");
    set_option("msg-level", "all=warn");

    const int status = mpv_initialize(handle_);
    if (status < 0) {
        error_ = QString::fromUtf8(mpv_error_string(status));
        mpv_destroy(handle_);
        handle_ = nullptr;
        return;
    }

    set_property("pause", false);
    set_property("volume", 100.0);
    set_property("speed", 1.0);
    apply_video_settings();
    mpv_observe_property(handle_, kChaptersObservation, "chapter-list", MPV_FORMAT_NONE);
    mpv_observe_property(handle_, kTracksObservation, "track-list", MPV_FORMAT_NONE);
    mpv_observe_property(handle_, kDurationObservation, "duration", MPV_FORMAT_NONE);

    poll_timer_.setInterval(kPollIntervalMs);
    connect(&poll_timer_, &QTimer::timeout, this, &MpvItem::poll);
    poll_timer_.start();
}

MpvItem::~MpvItem() {
    poll_timer_.stop();
    save_playback_position();

    const bool renderer_alive = niratan::player::unlink_render_item(this);

    // A live renderer still holds a render context for this handle, and mpv
    // requires that context to be freed first. The renderer destructor then
    // takes the handle over.
    if (handle_ != nullptr && !renderer_alive) {
        mpv_terminate_destroy(handle_);
    }
    handle_ = nullptr;
}

QQuickFramebufferObject::Renderer* MpvItem::createRenderer() const {
    return niratan::player::create_mpv_renderer(const_cast<MpvItem*>(this), handle_);
}

qreal MpvItem::position() const {
    return position_;
}

qreal MpvItem::duration() const {
    return duration_;
}

bool MpvItem::paused() const {
    return paused_;
}

void MpvItem::setPaused(bool paused) {
    if (paused_ == paused) {
        return;
    }
    paused_ = paused;
    set_property("pause", paused_);
    emit pausedChanged();
}

bool MpvItem::loaded() const {
    return loaded_;
}

bool MpvItem::muted() const {
    return muted_;
}

void MpvItem::setMuted(bool muted) {
    if (muted_ == muted) {
        return;
    }
    muted_ = muted;
    set_property("mute", muted_);
    emit mutedChanged();
}

qreal MpvItem::volume() const {
    return volume_;
}

void MpvItem::setVolume(qreal volume) {
    const qreal clamped = std::clamp(volume, 0.0, 100.0);
    if (volume_ == clamped) {
        return;
    }
    volume_ = clamped;
    set_property("volume", static_cast<double>(volume_));
    emit volumeChanged();
}

qreal MpvItem::speed() const {
    return speed_;
}

void MpvItem::setSpeed(qreal speed) {
    const qreal normalized = niratan::player::normalized_speed(speed);
    if (speed_ == normalized) {
        return;
    }
    speed_ = normalized;
    set_property("speed", static_cast<double>(speed_));
    emit speedChanged();
}

qreal MpvItem::loopStart() const { return ab_loop_.start().value_or(-1.0); }
qreal MpvItem::loopEnd() const { return ab_loop_.end().value_or(-1.0); }

qreal MpvItem::subtitleDelay() const {
    return subtitle_delay_;
}

void MpvItem::setSubtitleDelay(qreal delay) {
    const qreal clamped = std::clamp(delay, -kMaxDelaySeconds, kMaxDelaySeconds);
    if (subtitle_delay_ == clamped) {
        return;
    }
    subtitle_delay_ = clamped;
    set_property("sub-delay", static_cast<double>(subtitle_delay_));
    emit subtitleDelayChanged();
}

qreal MpvItem::audioDelay() const {
    return audio_delay_;
}

void MpvItem::setAudioDelay(qreal delay) {
    const qreal clamped = std::clamp(delay, -kMaxDelaySeconds, kMaxDelaySeconds);
    if (audio_delay_ == clamped) {
        return;
    }
    audio_delay_ = clamped;
    set_property("audio-delay", static_cast<double>(audio_delay_));
    emit audioDelayChanged();
}

bool MpvItem::looping() const {
    return looping_;
}

void MpvItem::setLooping(bool looping) {
    if (looping_ == looping) {
        return;
    }
    looping_ = looping;
    set_property("loop-file", looping_ ? QStringLiteral("inf") : QStringLiteral("no"));
    emit loopingChanged();
}

QString MpvItem::error() const {
    return error_;
}

QVariantList MpvItem::chapters() const { return chapters_; }
QVariantList MpvItem::tracks() const { return tracks_; }
bool MpvItem::resumeAvailable() const { return resume_position_ > 0.0; }
qreal MpvItem::resumePosition() const { return resume_position_; }
bool MpvItem::extractingTranscript() const { return transcript_process_ != nullptr; }
QString MpvItem::subtitleText() const { return subtitle_text_; }
qreal MpvItem::subtitleStart() const { return subtitle_start_; }
qreal MpvItem::subtitleEnd() const { return subtitle_end_; }
bool MpvItem::subtitlesVisible() const { return subtitles_visible_; }

void MpvItem::setSubtitlesVisible(bool visible) {
    set_property("sub-visibility", visible);
    if (subtitles_visible_ != visible) {
        subtitles_visible_ = visible;
        emit subtitlesVisibleChanged();
    }
}

void MpvItem::open(const QUrl& file) {
    if (handle_ == nullptr) {
        if (error_.isEmpty()) {
            error_ = QStringLiteral("The video engine is unavailable");
            emit errorChanged();
        }
        return;
    }

    if (file.isEmpty()) {
        return;
    }
    replace_file(file);
}

void MpvItem::unload() {
    if (handle_ != nullptr) {
        replace_file({});
    }
}

void MpvItem::replace_file(const QUrl& file) {
    const QString next_path = file.isLocalFile() && !file.toLocalFile().isEmpty()
        ? QFileInfo(file.toLocalFile()).absoluteFilePath() : QString{};
    // Persist the outgoing file before the path changes.
    save_playback_position();
    local_video_path_ = next_path;
    set_resume_position(0.0);
    resume_save_timer_.invalidate();
    const quint64 generation = ++load_generation_;
    waiting_for_context_ = !file.isEmpty();

    // A-B loop and chapter belong to the previous file until mpv reports the
    // new ones, and a stale loop would immediately cut the new file.
    clearABLoop();
    ended_ = false;
    chapter_index_ = -1;
    file_ready_ = false;
    selected_subtitle_id_ = -1;
    cancel_transcript();
    emit embeddedTranscriptChanged({}, {});
    chapters_dirty_ = tracks_dirty_ = true;
    if (!chapters_.isEmpty()) {
        chapters_.clear();
        emit chaptersChanged();
    }
    if (!tracks_.isEmpty()) {
        tracks_.clear();
        emit tracksChanged();
    }
    if (!subtitle_document_.isEmpty()) {
        subtitle_document_.clear();
        emit subtitleDocumentChanged({}, {}, false);
    }
    set_subtitle_cue({}, -1.0, -1.0);
    set_property("video-rotate", 0);
    set_property("sid", QStringLiteral("no"));
    setSubtitlesVisible(true);

    if (position_ != 0.0) {
        position_ = 0.0;
        emit positionChanged();
    }
    if (duration_ != 0.0) {
        duration_ = 0.0;
        emit durationChanged();
    }
    if (loaded_) {
        loaded_ = false;
        emit loadedChanged();
    }
    if (!error_.isEmpty()) {
        error_.clear();
        emit errorChanged();
    }
    if (file.isEmpty()) {
        command("stop");
    } else {
        open_when_ready(file, generation);
    }
}

void MpvItem::open_when_ready(const QUrl& file, quint64 generation) {
    if (generation != load_generation_) {
        return;
    }
    // mpv aborts a file whose vo=libmpv starts before a render context exists,
    // which happens when the item is asked to play before its first frame.
    if (!niratan::player::render_context_ready(this)) {
        QTimer::singleShot(kLoadRetryMs, this,
                           [this, file, generation] { open_when_ready(file, generation); });
        return;
    }

    // mpv takes the path as one argument, never as a parsed command line, so a
    // path with spaces or quotes needs no escaping.
    while (mpv_wait_event(handle_, 0)->event_id != MPV_EVENT_NONE) {
    }
    waiting_for_context_ = false;
    const QByteArray path = (file.isLocalFile() ? file.toLocalFile() : file.toString()).toUtf8();
    const char* args[] = {"loadfile", path.constData(), "replace", nullptr};
    const int status = mpv_command(handle_, args);
    if (status < 0) {
        error_ = QString::fromUtf8(mpv_error_string(status));
        emit errorChanged();
    }
}

void MpvItem::save_playback_position() {
    if (!video_settings_.value("videoRememberPlaybackPosition").toBool()
        || !file_ready_ || ended_ || local_video_path_.isEmpty()) {
        return;
    }
    if (get_bool("eof-reached", false)) {
        record_finished_playback();
        return;
    }
    const double position = get_double("time-pos", position_);
    if (!std::isfinite(position) || !std::isfinite(duration_)) {
        return;
    }
    playback_history_.save(local_video_path_, position, duration_);
}

void MpvItem::record_finished_playback() {
    if (video_settings_.value("videoRememberPlaybackPosition").toBool()
        && !local_video_path_.isEmpty() && std::isfinite(duration_)) {
        playback_history_.save(local_video_path_, duration_, duration_);
    }
}

void MpvItem::clear_playback_position() {
    if (!local_video_path_.isEmpty()) {
        playback_history_.clear(local_video_path_);
    }
}

void MpvItem::set_resume_position(double position) {
    if (resume_position_ != position) {
        resume_position_ = position;
        emit resumeChanged();
    }
}

void MpvItem::restore_playback_position() {
    if (!video_settings_.value("videoRememberPlaybackPosition").toBool()
        || local_video_path_.isEmpty()) {
        return;
    }
    const auto position = playback_history_.resume_position(local_video_path_);
    if (!position) {
        return;
    }
    const double target = duration_ > 0.0 ? std::min(*position, duration_) : *position;
    seek(target);
    set_resume_position(target);
}

void MpvItem::restartFromBeginning() {
    if (loaded_) {
        seek(0.0);
        clear_playback_position();
        resume_save_timer_.start();
        set_resume_position(0.0);
    }
}

void MpvItem::togglePause() {
    if (handle_ == nullptr) {
        return;
    }
    setPaused(!paused_);
}

void MpvItem::seek(qreal seconds) {
    if (handle_ == nullptr) {
        return;
    }

    // duration is unavailable until mpv parsed the file; clamping to it then
    // would collapse every seek to zero.
    double target = std::max(0.0, static_cast<double>(seconds));
    if (duration_ > 0.0) {
        target = std::min(target, static_cast<double>(duration_));
    }
    command("seek " + QByteArray::number(target, 'f', 3) + " absolute+exact");

    // Reflecting the seek immediately keeps the timeline responsive while
    // paused; before a file is loaded the position is mpv's to report.
    if (loaded_ && position_ != target) {
        position_ = target;
        emit positionChanged();
    }
}

void MpvItem::skip(qreal delta) {
    seek(position_ + delta);
}

void MpvItem::seekToChapter(int index) {
    if (handle_ == nullptr || index < 0) {
        return;
    }
    set_property("chapter", index);
}

bool MpvItem::loadExternalSubtitle(const QUrl& file) {
    if (handle_ == nullptr || !file.isLocalFile() || file.toLocalFile().isEmpty()) {
        return false;
    }
    const QByteArray path = file.toLocalFile().toUtf8();
    const char* args[] = {"sub-add", path.constData(), "select", nullptr};
    if (mpv_command(handle_, args) < 0) {
        return false;
    }
    setSubtitlesVisible(true);
    tracks_dirty_ = true;
    set_subtitle_cue({}, -1.0, -1.0);
    return true;
}

void MpvItem::selectSubtitleTrack(int id) {
    if (id <= 0) {
        return;
    }
    set_property("sid", id);
    setSubtitlesVisible(true);
    if (file_ready_) {
        refresh_tracks();
    }
    set_subtitle_cue({}, -1.0, -1.0);
}

void MpvItem::cancel_transcript() {
    if (auto* process = std::exchange(transcript_process_, nullptr)) {
        process->disconnect(this);
        process->kill();
        process->deleteLater();
        emit extractingTranscriptChanged();
    }
}

void MpvItem::extractTranscript() {
    if (!file_ready_ || transcript_process_) {
        return;
    }
    const auto track_it = std::ranges::find_if(tracks_, [this](const QVariant& entry) {
        const auto track = entry.toMap();
        return track.value(QStringLiteral("type")).toString() == QStringLiteral("sub") &&
            track.value(QStringLiteral("id")).toInt() == selected_subtitle_id_ &&
            !track.value(QStringLiteral("isExternal")).toBool();
    });
    if (track_it == tracks_.end()) {
        return;
    }
    const auto report_error = [this](const QString& error) {
        emit embeddedTranscriptChanged({}, error);
    };
    const QFileInfo video(local_video_path_);
    if (!video.isFile()) {
        report_error(tr("Embedded transcripts require a local video file."));
        return;
    }
    const auto track = track_it->toMap();
    if (track.value(QStringLiteral("isImage")).toBool()) {
        report_error(tr("This image-based subtitle track cannot be extracted as text."));
        return;
    }
    bool index_valid = false;
    const int index = track.value(QStringLiteral("ffIndex")).toInt(&index_valid);
    if (!index_valid || index < 0) {
        report_error(tr("mpv did not provide a stream index for this subtitle track."));
        return;
    }
    const auto root = niratan::storage::data_root();
    const auto directory = QString::fromStdString((root / "Cache/video-subtitles").string());
    if (root.empty() || !QDir().mkpath(directory)) {
        report_error(tr("Could not create the embedded transcript cache."));
        return;
    }
    const auto hash = QCryptographicHash::hash(local_video_path_.toUtf8(), QCryptographicHash::Sha256);
    const QString cache = QDir(directory).filePath(QStringLiteral("%1-%2.srt")
        .arg(QString::fromLatin1(hash.toHex())).arg(selected_subtitle_id_));
    const auto mtime = video.lastModified();
    const auto read_transcript = [this, report_error](const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            report_error(tr("Could not read the extracted transcript."));
            return;
        }
        const auto bytes = file.read(kMaxSubtitleDocumentBytes + 1);
        if (file.error() != QFile::NoError || bytes.size() > kMaxSubtitleDocumentBytes) {
            report_error(tr("Could not read the extracted transcript, or it is too large."));
            return;
        }
        if (bytes.isEmpty()) {
            report_error(tr("No readable cues were found in the embedded subtitle track."));
            return;
        }
        emit embeddedTranscriptChanged(bytes, {});
    };
    if (mtime.isValid() && QFileInfo(cache).lastModified() == mtime) {
        read_transcript(cache);
        return;
    }
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        report_error(tr("ffmpeg was not found on PATH."));
        return;
    }

    // Keep partial output out of the shared cache, even if the track changes
    // or a second player extracts the same track.
    auto* process = new QProcess(this);
    auto* staging = new QTemporaryFile(cache + QStringLiteral(".XXXXXX.srt"), process);
    if (!staging->open()) {
        delete process;
        report_error(tr("Could not create a temporary transcript file."));
        return;
    }
    staging->close();
    const auto finish = [this, process] {
        transcript_process_ = nullptr;
        process->deleteLater();
        emit extractingTranscriptChanged();
    };
    connect(process, &QProcess::errorOccurred, this, [this, finish, report_error](auto error) {
        if (error == QProcess::FailedToStart) {
            finish();
            report_error(tr("Could not start ffmpeg to extract the transcript."));
        }
    });
    connect(process, &QProcess::finished, this,
            [this, finish, report_error, read_transcript, staging, cache,
             mtime, video_path = local_video_path_](int code, QProcess::ExitStatus status) {
        finish();
        if (status != QProcess::NormalExit || code != 0) {
            report_error(tr("ffmpeg could not extract this subtitle track as text."));
            return;
        }
        if (QFileInfo(video_path).lastModified() != mtime) {
            report_error(tr("The video changed while extracting its transcript. Please try again."));
            return;
        }
        QFile output(staging->fileName());
        if (mtime.isValid() && output.open(QIODevice::ReadWrite) &&
            output.setFileTime(mtime, QFileDevice::FileModificationTime)) {
            output.close();
            std::error_code error;
            std::filesystem::rename(staging->fileName().toStdString(), cache.toStdString(), error);
            if (!error) {
                read_transcript(cache);
                return;
            }
        }
        read_transcript(staging->fileName());
    });
    transcript_process_ = process;
    emit embeddedTranscriptChanged({}, {});
    emit extractingTranscriptChanged();
    process->setStandardOutputFile(QProcess::nullDevice());
    process->setStandardErrorFile(QProcess::nullDevice());
    process->start(ffmpeg, {QStringLiteral("-nostdin"), QStringLiteral("-y"),
        QStringLiteral("-i"), local_video_path_, QStringLiteral("-map"),
        QStringLiteral("0:%1").arg(index), QStringLiteral("-c:s"), QStringLiteral("srt"),
        staging->fileName()});
}

void MpvItem::disableSubtitles() {
    set_property("sid", QStringLiteral("no"));
    setSubtitlesVisible(false);
    tracks_dirty_ = true;
    set_subtitle_cue({}, -1.0, -1.0);
}

bool MpvItem::screenshot(const QUrl& file) {
    if (handle_ == nullptr || !file.isLocalFile() || file.toLocalFile().isEmpty()) {
        return false;
    }
    const QByteArray path = file.toLocalFile().toUtf8();
    const char* args[] = {"screenshot-to-file", path.constData(), "video", nullptr};
    // A capture can wait for the render thread; do not block the GUI thread.
    return mpv_command_async(handle_, 0, args) >= 0;
}

void MpvItem::cycleSubtitles() {
    if (handle_ == nullptr) {
        return;
    }
    int first_id = -1;
    const int selected_id = static_cast<int>(get_double("sid", -1.0));
    bool next = false;
    for (const QVariant& entry : tracks_) {
        const QVariantMap track = entry.toMap();
        if (track.value(QStringLiteral("type")).toString() != QStringLiteral("sub")) {
            continue;
        }
        const int id = track.value(QStringLiteral("id")).toInt();
        if (first_id < 0) {
            first_id = id;
        }
        if (!subtitles_visible_ && id == selected_id) {
            setSubtitlesVisible(true);
            return;
        }
        if (next) {
            selectSubtitleTrack(id);
            return;
        }
        next = subtitles_visible_ && id == selected_id;
    }
    if (next) {
        disableSubtitles();
    } else if (first_id >= 0) {
        selectSubtitleTrack(first_id);
    } else {
        setSubtitlesVisible(!subtitles_visible_);
    }
}

void MpvItem::toggleSubtitles() {
    if (handle_ == nullptr) {
        return;
    }
    if (!subtitles_visible_ && get_double("sid", -1.0) <= 0) {
        cycleSubtitles();
        return;
    }
    setSubtitlesVisible(!subtitles_visible_);
}

bool MpvItem::setABLoopStart() {
    return setABLoopStartAt(position_);
}

bool MpvItem::setABLoopEnd() {
    return setABLoopEndAt(position_);
}

bool MpvItem::setABLoopStartAt(qreal time) {
    const bool was_active = ab_loop_.end().has_value();
    if (!loaded_ || !ab_loop_.set_start(time, duration_)) {
        return false;
    }
    if (was_active) {
        set_property("ab-loop-a", QStringLiteral("no"));
        set_property("ab-loop-b", QStringLiteral("no"));
    }
    emit abLoopChanged();
    return true;
}

bool MpvItem::setABLoopEndAt(qreal time) {
    if (!loaded_ || !ab_loop_.set_end(time, duration_)) {
        return false;
    }
    set_property("ab-loop-a", static_cast<double>(loopStart()));
    set_property("ab-loop-b", static_cast<double>(loopEnd()));
    emit abLoopChanged();
    return true;
}

void MpvItem::clearABLoop() {
    if (!ab_loop_.start()) {
        return;
    }
    ab_loop_.clear();
    set_property("ab-loop-a", QStringLiteral("no"));
    set_property("ab-loop-b", QStringLiteral("no"));
    emit abLoopChanged();
}

void MpvItem::rotateClockwise() {
    if (handle_ == nullptr) {
        return;
    }
    int rotation = static_cast<int>(std::lround(get_double("video-rotate", 0.0)));
    rotation = ((rotation % 360) + 360) % 360;
    set_property("video-rotate", (rotation + 90) % 360);
}

void MpvItem::stepSpeed(int direction) {
    if (handle_ == nullptr || direction == 0) {
        return;
    }
    const double step = direction > 0 ? kSpeedStep : -kSpeedStep;
    setSpeed(speed_ + step);
}

void MpvItem::resetSpeed() {
    setSpeed(1.0);
}

QString MpvItem::speedLabel(qreal value) const {
    return niratan::player::speed_label(value);
}

bool MpvItem::acceptSpeedInput(const QString& input) {
    const auto value = niratan::player::parse_speed(input);
    if (!loaded_ || !value) {
        return false;
    }
    setSpeed(*value);
    return true;
}

QVariantMap MpvItem::videoSettings() const {
    return video_settings_.toVariantMap();
}

bool MpvItem::changeVideoSetting(const QString& key, const QVariant& value) {
    return save_video_settings({{key, QJsonValue::fromVariant(value)}});
}

bool MpvItem::resetSubtitleAppearance() {
    return save_video_settings(niratan::player::subtitle_appearance_defaults());
}

bool MpvItem::save_video_settings(const QJsonObject& changes) {
    const auto root = niratan::storage::data_root();
    if (!niratan::player::store_video_settings(root, changes)) {
        return false;
    }
    video_settings_ = niratan::player::load_video_settings(root);
    apply_video_settings();
    if (!video_settings_.value("videoRememberPlaybackPosition").toBool()) {
        set_resume_position(0.0);
    }
    emit videoSettingsChanged();
    return true;
}

void MpvItem::apply_video_settings() {
    const auto& settings = video_settings_;
    set_property("hwdec", settings.value("videoHardwareDecodingEnabled").toBool()
        ? QStringLiteral("auto-safe") : QStringLiteral("no"));
    set_property("deinterlace", settings.value("videoDeinterlacingEnabled").toBool());
    set_property("hdr-compute-peak", settings.value("videoHDREnhancementEnabled").toBool()
        ? QStringLiteral("yes") : QStringLiteral("no"));
    set_property("tone-mapping", QStringLiteral("auto"));
    for (const auto& [option, key] : {std::pair{"brightness", "videoBrightness"},
                                     {"contrast", "videoContrast"}, {"saturation", "videoSaturation"},
                                     {"gamma", "videoGamma"}, {"hue", "videoHue"}}) {
        set_property(option, settings.value(QLatin1StringView(key)).toDouble());
    }
    const auto family = settings.value("videoSubtitleFontFamily").toString();
    set_property("sub-font", family.isEmpty() ? QStringLiteral("sans-serif") : family);
    const double size = settings.value("videoSubtitleFontSize").toDouble();
    set_property("sub-font-size", size);
    set_property("sub-pos", 100.0 * settings.value("videoSubtitleVerticalPositionFraction").toDouble());
    const int weight = settings.value("videoSubtitleFontWeight").toInt();
    set_property("sub-bold", weight >= 700);
    set_property("sub-font-weight", weight);
    set_property("sub-color", niratan::player::mpv_color(settings.value("videoSubtitleColor").toString()));
    set_property("sub-ass-override", settings.value("videoRespectASSStyle").toBool()
        ? QStringLiteral("no") : QStringLiteral("strip"));
    const double opacity = settings.value("videoSubtitleBackgroundDisabled").toBool()
        ? 0.0 : settings.value("videoSubtitleBackgroundOpacity").toDouble();
    const auto alpha = QString::number(std::lround(opacity * 255), 16)
        .rightJustified(2, QLatin1Char('0'));
    set_property("sub-back-color", u'#' + alpha + QStringLiteral("000000"));
    set_property("sub-border-style", opacity > 0
        ? QStringLiteral("background-box") : QStringLiteral("outline-and-shadow"));
    const auto edges = niratan::player::subtitle_edges(settings.value("videoSubtitleEdgeStyle").toString(),
        settings.value("videoSubtitleEdgeStrength").toDouble(), size);
    set_property("sub-border-size", edges.outline);
    set_property("sub-shadow-offset", edges.shadow);
    set_property("sub-scale-by-window", false);
}

void MpvItem::refresh_chapters() {
    QVariantList chapters;
    const QVariantList nodes = node_list(handle_, "chapter-list");
    for (qsizetype i = 0; i < nodes.size(); ++i) {
        const QVariantMap fields = nodes[i].toMap();
        const auto start_key = QStringLiteral("time");
        if (!fields.contains(start_key)) {
            continue;
        }
        const double start = fields.value(start_key).toDouble();
        QVariantMap entry{
            {QStringLiteral("id"), int(i)},
            {QStringLiteral("title"), fields.value(QStringLiteral("title")).toString()},
            {QStringLiteral("start"), start},
        };
        // As in the study list, use the next strictly later start, then
        // fall back to the file duration for the last chapter.
        double end = start;
        for (const QVariant& other : nodes) {
            const double candidate = other.toMap().value(start_key).toDouble();
            if (candidate > start && (end <= start || candidate < end)) {
                end = candidate;
            }
        }
        if (end <= start && duration_ > start) {
            end = duration_;
        }
        if (end > start) {
            entry.insert(QStringLiteral("duration"), end - start);
        }
        chapters.append(entry);
    }
    if (chapters_ != chapters) {
        chapters_ = std::move(chapters);
        emit chaptersChanged();
    }
}

void MpvItem::refresh_tracks() {
    QVariantList tracks;
    QUrl subtitle_document;
    const int sid = static_cast<int>(get_double("sid", -1.0));
    for (const QVariant& node : node_list(handle_, "track-list")) {
        const QVariantMap fields = node.toMap();
        const QString type = fields.value(QStringLiteral("type")).toString();
        const int id = fields.value(QStringLiteral("id")).toInt();
        const QString external = fields.value(QStringLiteral("external-filename")).toString();
        if (type == QStringLiteral("sub") && id == sid && !external.isEmpty()) {
            // The player only imports local subtitle documents. A subtitle
            // URL supplied by mpv must not turn into a local file read.
            if (external.startsWith(QLatin1Char('/'))) {
                subtitle_document = QUrl::fromLocalFile(external);
            }
        }
        QVariantMap track{
            {QStringLiteral("type"), type},
            {QStringLiteral("id"), id},
            {QStringLiteral("title"), fields.value(QStringLiteral("title")).toString()},
            {QStringLiteral("language"), fields.value(QStringLiteral("lang")).toString()},
            {QStringLiteral("codec"), fields.value(QStringLiteral("codec")).toString()},
            {QStringLiteral("isImage"), fields.value(QStringLiteral("image")).toBool()},
            {QStringLiteral("isSelected"), fields.value(QStringLiteral("selected")).toBool()},
            {QStringLiteral("isExternal"), fields.value(QStringLiteral("external")).toBool()
                                          || !external.isEmpty()},
            {QStringLiteral("externalFilename"), external},
        };
        const auto index = fields.value(QStringLiteral("ff-index"));
        if (index.isValid()) {
            track.insert(QStringLiteral("ffIndex"), index);
        }
        tracks.append(track);
    }
    if (selected_subtitle_id_ != sid) {
        selected_subtitle_id_ = sid;
        cancel_transcript();
        emit embeddedTranscriptChanged({}, {});
    }
    if (tracks_ != tracks) {
        tracks_ = std::move(tracks);
        emit tracksChanged();
    }
    if (subtitle_document_ != subtitle_document) {
        subtitle_document_ = subtitle_document;
        if (subtitle_document_.isEmpty()) {
            emit subtitleDocumentChanged({}, {}, false);
        } else {
            QFile file(subtitle_document_.toLocalFile());
            const bool opened = QFileInfo(file).isFile() && file.open(QIODevice::ReadOnly);
            const QByteArray bytes = opened ? file.read(kMaxSubtitleDocumentBytes + 1)
                                            : QByteArray{};
            const bool readable = opened && file.error() == QFile::NoError &&
                                  bytes.size() <= kMaxSubtitleDocumentBytes;
            emit subtitleDocumentChanged(subtitle_document_, readable ? bytes : QByteArray{},
                                         readable);
        }
    }
}

void MpvItem::set_subtitle_cue(const QString& text, double start, double end) {
    if (subtitle_text_ != text || subtitle_start_ != start || subtitle_end_ != end) {
        subtitle_text_ = text;
        subtitle_start_ = start;
        subtitle_end_ = end;
        emit subtitleCueChanged();
    }
}

void MpvItem::poll() {
    if (handle_ == nullptr) {
        return;
    }

    for (mpv_event* event = mpv_wait_event(handle_, 0); event->event_id != MPV_EVENT_NONE;
         event = mpv_wait_event(handle_, 0)) {
        if (waiting_for_context_) {
            continue;
        }
        switch (event->event_id) {
            case MPV_EVENT_START_FILE:
                file_ready_ = false;
                break;
            case MPV_EVENT_FILE_LOADED:
                file_ready_ = true;
                chapters_dirty_ = tracks_dirty_ = true;
                break;
            case MPV_EVENT_END_FILE: {
                const auto* end = static_cast<const mpv_event_end_file*>(event->data);
                if (!file_ready_ && end != nullptr && end->reason == MPV_END_FILE_REASON_ERROR) {
                    error_ = QString::fromUtf8(mpv_error_string(end->error));
                    emit errorChanged();
                }
                if (file_ready_) {
                    if (end != nullptr && end->reason == MPV_END_FILE_REASON_EOF) {
                        if (!ended_) {
                            record_finished_playback();
                        }
                        set_resume_position(0.0);
                        if (!ended_) {
                            ended_ = true;
                            emit ended();
                        }
                    } else {
                        save_playback_position();
                    }
                    file_ready_ = false;
                    cancel_transcript();
                }
                break;
            }
            case MPV_EVENT_PROPERTY_CHANGE:
                if (event->reply_userdata == kTracksObservation) {
                    tracks_dirty_ = true;
                } else if (event->reply_userdata == kChaptersObservation ||
                           event->reply_userdata == kDurationObservation) {
                    chapters_dirty_ = true;
                }
                break;
            default:
                break;
        }
    }

    const double time_pos = file_ready_ ? get_double("time-pos", position_) : position_;
    const double duration = file_ready_ ? get_double("duration", 0.0) : 0.0;
    const bool paused = get_bool("pause", paused_);
    const bool loaded = duration > 0.0 && error_.isEmpty();
    const double volume = get_double("volume", volume_);
    const bool muted = get_bool("mute", muted_);
    const double speed = get_double("speed", speed_);
    const double subtitle_delay = get_double("sub-delay", subtitle_delay_);
    const double audio_delay = get_double("audio-delay", audio_delay_);
    const int chapter = file_ready_ ? static_cast<int>(get_double("chapter", -1.0)) : -1;

    const bool loaded_changed = loaded != loaded_;
    const bool paused_changed = paused != paused_;

    // A position that moves by a frame or more is worth a binding update; a
    // pause or load state change also refreshes the time shown next to it.
    if (std::abs(time_pos - position_) >= kPositionEpsilon || loaded_changed ||
        paused_changed) {
        position_ = time_pos;
        emit positionChanged();
    }
    if (std::abs(duration - duration_) > kValueEpsilon) {
        duration_ = duration;
        emit durationChanged();
    }
    if (file_ready_) {
        if (chapters_dirty_ || chapter != chapter_index_) {
            chapters_dirty_ = false;
            refresh_chapters();
        }
        if (tracks_dirty_) {
            tracks_dirty_ = false;
            refresh_tracks();
        }
        const QString text = get_string("sub-text");
        const double start = text.isEmpty() ? -1.0 : get_double("sub-start", -1.0);
        const double end = text.isEmpty() ? -1.0 : get_double("sub-end", -1.0);
        set_subtitle_cue(text, std::isfinite(start) ? start : -1.0,
                        std::isfinite(end) ? end : -1.0);
    }
    if (loaded_changed) {
        loaded_ = loaded;
        if (loaded_) {
            restore_playback_position();
            resume_save_timer_.start();
        }
        emit loadedChanged();
    }
    if (paused_changed) {
        paused_ = paused;
        emit pausedChanged();
    }
    if (std::abs(volume - volume_) > kValueEpsilon) {
        volume_ = volume;
        emit volumeChanged();
    }
    if (muted != muted_) {
        muted_ = muted;
        emit mutedChanged();
    }
    if (std::abs(speed - speed_) > kValueEpsilon) {
        speed_ = speed;
        emit speedChanged();
    }
    if (std::abs(subtitle_delay - subtitle_delay_) > kValueEpsilon) {
        subtitle_delay_ = subtitle_delay;
        emit subtitleDelayChanged();
    }
    if (std::abs(audio_delay - audio_delay_) > kValueEpsilon) {
        audio_delay_ = audio_delay;
        emit audioDelayChanged();
    }
    if (chapter != chapter_index_) {
        chapter_index_ = chapter;
        emit chapterChanged(chapter_index_);
    }

    if (file_ready_ && get_bool("eof-reached", false)) {
        if (!ended_) {
            ended_ = true;
            record_finished_playback();
            set_resume_position(0.0);
            emit ended();
        }
    } else if (file_ready_) {
        ended_ = false;
        if (loaded_ && !paused_ && !local_video_path_.isEmpty() &&
            resume_save_timer_.hasExpired(kResumeSaveIntervalMs - 1)) {
            resume_save_timer_.restart();
            save_playback_position();
        }
    }
}

void MpvItem::command(const QByteArray& name) {
    if (handle_ == nullptr) {
        return;
    }
    mpv_command_string(handle_, name.constData());
}

void MpvItem::set_property(const char* name, const QString& value) {
    if (handle_ == nullptr) {
        return;
    }
    const QByteArray utf8 = value.toUtf8();
    mpv_set_property_string(handle_, name, utf8.constData());
}

void MpvItem::set_property(const char* name, double value) {
    if (handle_ == nullptr) {
        return;
    }
    mpv_set_property(handle_, name, MPV_FORMAT_DOUBLE, &value);
}

void MpvItem::set_property(const char* name, int value) {
    if (handle_ == nullptr) {
        return;
    }
    std::int64_t wide = value;
    mpv_set_property(handle_, name, MPV_FORMAT_INT64, &wide);
}

void MpvItem::set_property(const char* name, bool value) {
    if (handle_ == nullptr) {
        return;
    }
    int flag = value ? 1 : 0;
    mpv_set_property(handle_, name, MPV_FORMAT_FLAG, &flag);
}

double MpvItem::get_double(const char* name, double fallback) const {
    if (handle_ == nullptr) {
        return fallback;
    }
    double value = 0.0;
    if (mpv_get_property(handle_, name, MPV_FORMAT_DOUBLE, &value) < 0) {
        return fallback;
    }
    return value;
}

bool MpvItem::get_bool(const char* name, bool fallback) const {
    if (handle_ == nullptr) {
        return fallback;
    }
    int value = 0;
    if (mpv_get_property(handle_, name, MPV_FORMAT_FLAG, &value) < 0) {
        return fallback;
    }
    return value != 0;
}

QString MpvItem::get_string(const char* name) const {
    if (handle_ == nullptr) {
        return {};
    }
    char* value = mpv_get_property_string(handle_, name);
    if (value == nullptr) {
        return {};
    }
    const QString text = QString::fromUtf8(value);
    mpv_free(value);
    return text;
}
