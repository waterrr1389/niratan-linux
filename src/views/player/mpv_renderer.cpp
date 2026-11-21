// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpv_renderer.h"

#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QSize>

#include <mpv/render.h>
#include <mpv/render_gl.h>

#include <unordered_map>

namespace niratan::player {
namespace {

struct Link {
    bool item_alive = true;
    bool renderer_alive = false;
};

QMutex g_links_mutex;
std::unordered_map<const QQuickFramebufferObject*, Link> g_links;

class MpvRenderer : public QQuickFramebufferObject::Renderer {
   public:
    MpvRenderer(QQuickFramebufferObject* item, mpv_handle* handle);
    ~MpvRenderer() override;

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override;
    void render() override;
    void synchronize(QQuickFramebufferObject*) override {}

   private:
    static void* resolve_gl_function(void* context, const char* name);
    static void report_update(void* data);

    QQuickFramebufferObject* item_ = nullptr;
    mpv_handle* handle_ = nullptr;
    mpv_render_context* context_ = nullptr;
    QOpenGLFunctions* gl_ = nullptr;
};

MpvRenderer::MpvRenderer(QQuickFramebufferObject* item, mpv_handle* handle)
    : item_(item), handle_(handle) {
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
        // The item cannot report this: its error has no setter the renderer
        // may call, and QML would need a queued round trip to notice.
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
            g_links.emplace(item_, Link{.item_alive = true, .renderer_alive = true});
        }
    }
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

    // The item is already gone, so its destructor left the handle for us.
    if (!item_alive && handle_ != nullptr) {
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

void link_render_item(const QQuickFramebufferObject* item) {
    QMutexLocker locker(&g_links_mutex);
    g_links[item] = Link{};
}

bool unlink_render_item(const QQuickFramebufferObject* item) {
    QMutexLocker locker(&g_links_mutex);
    const auto link = g_links.find(item);
    if (link == g_links.end()) {
        return false;
    }
    link->second.item_alive = false;
    const bool renderer_alive = link->second.renderer_alive;
    if (!renderer_alive) {
        g_links.erase(link);
    }
    return renderer_alive;
}

bool render_context_ready(const QQuickFramebufferObject* item) {
    QMutexLocker locker(&g_links_mutex);
    const auto link = g_links.find(item);
    return link != g_links.end() && link->second.renderer_alive;
}

QQuickFramebufferObject::Renderer* create_mpv_renderer(QQuickFramebufferObject* item,
                                                       mpv_handle* handle) {
    return new MpvRenderer(item, handle);
}

}  // namespace niratan::player
