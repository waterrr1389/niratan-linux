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

#include <QByteArray>
#include <QChar>
#include <QDebug>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QSize>
#include <QString>
#include <QUrl>

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

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

// eof-reached stays true for as long as the ended file is loaded, while
// ended() must fire once per load. Polling, open() and the destructor all run
// on the GUI thread, so no lock is needed here.
std::unordered_set<const MpvItem*> g_ended_items;

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

    poll_timer_.setInterval(kPollIntervalMs);
    connect(&poll_timer_, &QTimer::timeout, this, &MpvItem::poll);
    poll_timer_.start();
}

MpvItem::~MpvItem() {
    poll_timer_.stop();
    g_ended_items.erase(this);

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

void MpvItem::open(const QUrl& file) {
    if (handle_ == nullptr) {
        if (error_.isEmpty()) {
            error_ = QStringLiteral("The video engine is unavailable");
            emit errorChanged();
        }
        return;
    }

    const QString source = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (source.isEmpty()) {
        return;
    }

    // A-B loop and chapter belong to the previous file until mpv reports the
    // new ones, and a stale loop would immediately cut the new file.
    ab_loop_start_ = -1.0;
    chapter_index_ = -1;
    set_property("ab-loop-a", QStringLiteral("no"));
    set_property("ab-loop-b", QStringLiteral("no"));
    g_ended_items.erase(this);

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

    // mpv aborts a file whose vo=libmpv starts before a render context exists,
    // which happens when the item is asked to play before its first frame.
    bool context_ready = false;
    {
        QMutexLocker locker(&g_links_mutex);
        context_ready = render_context_ != nullptr;
    }
    if (!context_ready) {
        QTimer::singleShot(kLoadRetryMs, this, [this, file] { open(file); });
        return;
    }

    // mpv takes the path as one argument, never as a parsed command line, so a
    // path with spaces or quotes needs no escaping.
    const QByteArray path = source.toUtf8();
    const char* args[] = {"loadfile", path.constData(), "replace", nullptr};
    const int status = mpv_command(handle_, args);
    if (status < 0) {
        error_ = QString::fromUtf8(mpv_error_string(status));
        emit errorChanged();
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

void MpvItem::cycleSubtitles() {
    // Cycling needs the track list to pick the next subtitle track. Until the
    // engine owns track selection, this only shows or hides the current track.
    toggleSubtitles();
}

void MpvItem::toggleSubtitles() {
    if (handle_ == nullptr) {
        return;
    }
    set_property("sub-visibility", !get_bool("sub-visibility", true));
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

void MpvItem::poll() {
    if (handle_ == nullptr) {
        return;
    }

    const double time_pos = get_double("time-pos", position_);
    const double duration = get_double("duration", 0.0);
    const bool paused = get_bool("pause", paused_);
    const bool loaded = duration > 0.0 && error_.isEmpty();
    const double volume = get_double("volume", volume_);
    const bool muted = get_bool("mute", muted_);
    const double speed = get_double("speed", speed_);
    const double subtitle_delay = get_double("sub-delay", subtitle_delay_);
    const double audio_delay = get_double("audio-delay", audio_delay_);
    const int chapter = static_cast<int>(std::lround(get_double("chapter", -1.0)));

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
    if (loaded_changed) {
        loaded_ = loaded;
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

    if (get_bool("eof-reached", false)) {
        if (g_ended_items.insert(this).second) {
            emit ended();
        }
    } else {
        g_ended_items.erase(this);
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
