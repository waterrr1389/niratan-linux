// SPDX-License-Identifier: GPL-3.0-or-later
//
// The libmpv surface behind MpvItem. The item owns the mpv handle and polls
// the properties the player UI binds to; the renderer owns the render context
// and draws into the Quick framebuffer object.
//
// Threading: the handle is created and used on the GUI thread, where the
// polling timer also runs. The render context is created, used and freed on
// the scene graph thread, the only thread where the OpenGL context of the
// window is current. Qt creates the renderer during a scene graph sync and
// destroys it when the node goes away, in an order the item cannot observe, so
// both objects publish their lifetime in the link table below: the handle is
// destroyed by whichever object outlives the other, and never before the
// render context that uses it.

#include "mpv_item.h"

#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

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
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QProcess>
#include <QSize>
#include <QString>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace {

constexpr int kPollIntervalMs = 100;
constexpr int kLoadRetryMs = 100;
constexpr double kPositionEpsilon = 0.02;
constexpr double kValueEpsilon = 1e-6;
constexpr double kMinSpeed = 0.25;
constexpr double kMaxSpeed = 5.0;
constexpr double kSpeedStep = 0.1;
constexpr double kMaxDelaySeconds = 60.0;
constexpr int kMinSubtitleSize = 12;
constexpr int kMaxSubtitleSize = 72;
constexpr int kMinSubtitleWeight = 100;
constexpr int kMaxSubtitleWeight = 900;
constexpr quint64 kChaptersObservation = 1;
constexpr quint64 kTracksObservation = 2;
constexpr quint64 kDurationObservation = 3;
constexpr qint64 kMaxSubtitleDocumentBytes = 16 * 1024 * 1024;
constexpr int kResumeSaveIntervalMs = 5000;
constexpr double kResumeMarginSeconds = 10.0;

std::filesystem::path playback_history_path() {
    const auto root = niratan::storage::data_root();
    return root.empty() ? root : root / "video_playback.json";
}

void update_playback_history(const QString& key, const QJsonObject& entry = {}) {
    const auto path = playback_history_path();
    if (key.isEmpty() || path.empty()) {
        return;
    }
    const auto read = niratan::storage::read_json_file(path);
    if (read.status == niratan::storage::JsonReadStatus::invalid ||
        (read.status == niratan::storage::JsonReadStatus::success && !read.document.isObject())) {
        return;
    }
    auto records = read.document.object();
    if (entry.isEmpty()) {
        if (!records.contains(key)) {
            return;
        }
        records.remove(key);
    } else {
        const auto previous = records.value(key).toObject();
        if (previous.value(QStringLiteral("position")) == entry.value(QStringLiteral("position")) &&
            previous.value(QStringLiteral("duration")) == entry.value(QStringLiteral("duration"))) {
            return;
        }
        records.insert(key, entry);
    }
    niratan::storage::write_json_file(path, QJsonDocument(records));
}

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

// Qt keeps the renderer, which owns the render context, alive in the scene
// graph node after the item is destroyed at window teardown. Both sides
// register here so the context is always freed before the handle it belongs
// to, whichever of them survives the other.
struct SharedLink {
    bool item_alive = true;
    bool renderer_alive = false;
};

QMutex g_links_mutex;
std::unordered_map<const MpvItem*, SharedLink> g_links;

class MpvRenderer : public QQuickFramebufferObject::Renderer {
   public:
    explicit MpvRenderer(MpvItem* item);
    ~MpvRenderer() override;

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override;
    void render() override;
    void synchronize(QQuickFramebufferObject*) override {}

   private:
    static void* resolve_gl_function(void* context, const char* name);
    static void report_update(void* data);

    MpvItem* item_ = nullptr;
    mpv_handle* handle_ = nullptr;
    mpv_render_context* context_ = nullptr;
    QOpenGLFunctions* gl_ = nullptr;
};

MpvRenderer::MpvRenderer(MpvItem* item) : item_(item), handle_(item->handle()) {
    QOpenGLContext* current = QOpenGLContext::currentContext();
    if (handle_ == nullptr || current == nullptr) {
        return;
    }
    gl_ = current->functions();

    mpv_opengl_init_params init = {};
    init.get_proc_address = &MpvRenderer::resolve_gl_function;
    init.get_proc_address_ctx = nullptr;
    const char* api_type = MPV_RENDER_API_TYPE_OPENGL;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(api_type)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &init},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    mpv_render_context* context = nullptr;
    const int status = mpv_render_context_create(&context, handle_, params);
    if (status < 0) {
        // The item cannot report this: error_ has no setter the renderer may
        // call, and QML would need a queued round trip to notice.
        qWarning("mpv: could not create the render context: %s", mpv_error_string(status));
        return;
    }
    context_ = context;

    {
        QMutexLocker locker(&g_links_mutex);
        const auto link = g_links.find(item_);
        if (link != g_links.end()) {
            link->second.renderer_alive = true;
        } else {
            g_links.emplace(item_, SharedLink{.item_alive = true, .renderer_alive = true});
        }
    }
    item_->setRenderContext(context_);
    mpv_render_context_set_update_callback(context_, &MpvRenderer::report_update, this);
}

MpvRenderer::~MpvRenderer() {
    if (context_ == nullptr) {
        return;
    }

    // mpv requires the render context to be freed before the core is torn down,
    // and to be freed with the same OpenGL context that created it.
    mpv_render_context_set_update_callback(context_, nullptr, nullptr);
    mpv_render_context_free(context_);
    context_ = nullptr;

    bool item_alive = false;
    {
        QMutexLocker locker(&g_links_mutex);
        const auto link = g_links.find(item_);
        if (link != g_links.end()) {
            link->second.renderer_alive = false;
            item_alive = link->second.item_alive;
            if (!item_alive) {
                g_links.erase(link);
            }
        }
    }

    if (item_alive) {
        item_->setRenderContext(nullptr);
        return;
    }

    // The item is already gone, so its destructor left the handle for us.
    if (handle_ != nullptr) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}

QOpenGLFramebufferObject* MpvRenderer::createFramebufferObject(const QSize& size) {
    return new QOpenGLFramebufferObject(size);
}

void MpvRenderer::render() {
    QOpenGLFramebufferObject* fbo = framebufferObject();
    if (context_ == nullptr || fbo == nullptr || gl_ == nullptr) {
        return;
    }

    // mpv clears the target only when it draws a frame; without this the item
    // shows uninitialized texture content before the first frame.
    fbo->bind();
    gl_->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl_->glClear(GL_COLOR_BUFFER_BIT);

    mpv_opengl_fbo target = {};
    target.fbo = static_cast<int>(fbo->handle());
    target.w = fbo->width();
    target.h = fbo->height();
    target.internal_format = 0;
    // Qt 6 presents the framebuffer texture without mirroring, so mpv writes
    // the frame in its normal orientation. Flipping here shows the video
    // upside down.
    int flip_y = 0;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &target},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    mpv_render_context_render(context_, params);
    mpv_render_context_report_swap(context_);
}

void* MpvRenderer::resolve_gl_function(void*, const char* name) {
    QOpenGLContext* current = QOpenGLContext::currentContext();
    if (current == nullptr || name == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<void*>(current->getProcAddress(name));
}

void MpvRenderer::report_update(void* data) {
    auto* renderer = static_cast<MpvRenderer*>(data);
    if (renderer == nullptr) {
        return;
    }

    // The callback runs on the render thread and the item may already be
    // destroyed; only the pointer is compared, never dereferenced.
    {
        QMutexLocker locker(&g_links_mutex);
        const auto link = g_links.find(renderer->item_);
        if (link == g_links.end() || !link->second.item_alive) {
            return;
        }
    }
    QMetaObject::invokeMethod(renderer->item_, "update", Qt::QueuedConnection);
}

}  // namespace

MpvItem::MpvItem(QQuickItem* parent) : QQuickFramebufferObject(parent) {
    {
        QMutexLocker locker(&g_links_mutex);
        g_links[this] = SharedLink{};
    }

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

    bool renderer_alive = false;
    {
        QMutexLocker locker(&g_links_mutex);
        const auto link = g_links.find(this);
        if (link != g_links.end()) {
            link->second.item_alive = false;
            renderer_alive = link->second.renderer_alive;
            if (!renderer_alive) {
                g_links.erase(link);
            }
        }
    }

    // A live renderer still holds a render context for this handle, and mpv
    // requires that context to be freed first. The renderer destructor then
    // takes the handle over.
    if (handle_ != nullptr && !renderer_alive) {
        mpv_terminate_destroy(handle_);
    }
    handle_ = nullptr;
}

QQuickFramebufferObject::Renderer* MpvItem::createRenderer() const {
    return new MpvRenderer(const_cast<MpvItem*>(this));
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
    const qreal clamped = std::clamp(speed, kMinSpeed, kMaxSpeed);
    if (speed_ == clamped) {
        return;
    }
    speed_ = clamped;
    set_property("speed", static_cast<double>(speed_));
    emit speedChanged();
}

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
    if (!file.isEmpty() && next_path != local_video_path_) {
        clear_playback_position();
    } else {
        save_playback_position();
    }
    local_video_path_ = next_path;
    set_resume_position(0.0);
    resume_save_timer_.invalidate();
    const quint64 generation = ++load_generation_;
    waiting_for_context_ = !file.isEmpty();

    // A-B loop and chapter belong to the previous file until mpv reports the
    // new ones, and a stale loop would immediately cut the new file.
    ab_loop_start_ = -1.0;
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
    set_property("ab-loop-a", QStringLiteral("no"));
    set_property("ab-loop-b", QStringLiteral("no"));
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
    bool context_ready = false;
    {
        QMutexLocker locker(&g_links_mutex);
        context_ready = render_context_ != nullptr;
    }
    if (!context_ready) {
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
    if (!file_ready_ || ended_ || local_video_path_.isEmpty()) {
        return;
    }
    if (get_bool("eof-reached", false)) {
        clear_playback_position();
        return;
    }
    const double position = get_double("time-pos", position_);
    if (!std::isfinite(position) || !std::isfinite(duration_) || duration_ <= 0.0) {
        return;
    }
    if (position <= 0.0) {
        clear_playback_position();
        return;
    }
    update_playback_history(local_video_path_, {
        {QStringLiteral("position"), position},
        {QStringLiteral("duration"), duration_},
        {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
    });
}

void MpvItem::clear_playback_position() {
    update_playback_history(local_video_path_);
}

void MpvItem::set_resume_position(double position) {
    if (resume_position_ != position) {
        resume_position_ = position;
        emit resumeChanged();
    }
}

void MpvItem::restore_playback_position() {
    const auto path = playback_history_path();
    if (local_video_path_.isEmpty() || path.empty()) {
        return;
    }
    const auto records = niratan::storage::read_json_file(path).document.object();
    const double position = records.value(local_video_path_).toObject()
        .value(QStringLiteral("position")).toDouble();
    if (position > kResumeMarginSeconds && duration_ - position > kResumeMarginSeconds) {
        seek(position);
        set_resume_position(position);
    }
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

void MpvItem::setABLoopStart() {
    if (handle_ == nullptr) {
        return;
    }
    ab_loop_start_ = position_;
    set_property("ab-loop-b", QStringLiteral("no"));
    set_property("ab-loop-a", static_cast<double>(ab_loop_start_));
}

void MpvItem::setABLoopEnd() {
    if (handle_ == nullptr || ab_loop_start_ < 0.0 || position_ <= ab_loop_start_) {
        return;
    }
    set_property("ab-loop-b", static_cast<double>(position_));
}

void MpvItem::clearABLoop() {
    if (handle_ == nullptr) {
        return;
    }
    ab_loop_start_ = -1.0;
    set_property("ab-loop-a", QStringLiteral("no"));
    set_property("ab-loop-b", QStringLiteral("no"));
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
    const double stepped =
        std::round((static_cast<double>(speed_) + step) * 10.0) / 10.0;
    setSpeed(std::clamp(stepped, kMinSpeed, kMaxSpeed));
}

void MpvItem::resetSpeed() {
    setSpeed(1.0);
}

void MpvItem::applySubtitleStyle(int font_size, int position_percent, int weight,
                                 double background_opacity) {
    if (handle_ == nullptr) {
        return;
    }

    const int size = std::clamp(font_size, kMinSubtitleSize, kMaxSubtitleSize);
    const int position = std::clamp(position_percent, 0, 100);
    const int font_weight =
        std::clamp(weight, kMinSubtitleWeight, kMaxSubtitleWeight);
    const double opacity = std::clamp(background_opacity, 0.0, 1.0);
    const int alpha = static_cast<int>(std::lround(opacity * 255.0));

    set_property("sub-font-size", size);
    set_property("sub-pos", position);
    // Subtitle weight exists only in builds that provide sub-font-weight; an
    // unknown property name costs one failed call and changes nothing.
    set_property("sub-font-weight", font_weight);

    // mpv colors are #AARRGGBB; the box is black with the configured alpha.
    QString background = QStringLiteral("#");
    background += QString::number(alpha, 16).rightJustified(2, QLatin1Char('0'));
    background += QStringLiteral("000000");
    set_property("sub-back-color", background);

    // sub-back-color is only a background in the box styles; the high contrast
    // default is a tight box around the text.
    set_property("sub-border-style", QStringLiteral("background-box"));
    set_property("sub-border-size", 2.5);
    set_property("sub-shadow-offset", 0.0);

    // The configured size is a constant pixel size, while mpv scales
    // sub-font-size with the window by default.
    set_property("sub-scale-by-window", false);
}

mpv_handle* MpvItem::handle() const {
    return handle_;
}

void MpvItem::setRenderContext(void* context) {
    // Written by the renderer on the scene graph thread and read by open() on
    // the GUI thread to know whether mpv may start a file, hence the lock.
    QMutexLocker locker(&g_links_mutex);
    render_context_ = context;
}

void MpvItem::requestUpdate() {
    update();
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
                        clear_playback_position();
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
            clear_playback_position();
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
