// SPDX-License-Identifier: GPL-3.0-or-later
//
// The render side of the video surface, following HSMpvClient.mm's render
// context setup and PlaybackEngine.swift's frame callback: an mpv OpenGL
// render context drawn into a Quick framebuffer object.
//
// Threading: the mpv handle is created and used on the GUI thread. The render
// context is created, used and freed on the scene graph thread, the only
// thread where the OpenGL context of the window is current. Qt creates the
// renderer during a scene graph sync and destroys it when the node goes away,
// in an order the item cannot observe, and keeps the renderer alive after the
// item at window teardown. Both sides therefore publish their lifetime in a
// link table keyed by the item: the handle is destroyed by whichever side
// outlives the other, and never before the render context that uses it.

#pragma once

#include <QQuickFramebufferObject>

#include <mpv/client.h>

namespace niratan::player {

// Registers a live item whose handle a renderer may draw from.
void link_render_item(const QQuickFramebufferObject* item);
// Marks the item gone. True when a renderer still holds a render context for
// the item's handle; that renderer then destroys the handle, and the item
// must not.
bool unlink_render_item(const QQuickFramebufferObject* item);
// Whether a render context exists for the item's handle, so mpv may start a
// file with vo=libmpv. Safe from the GUI thread.
bool render_context_ready(const QQuickFramebufferObject* item);

// Called from QQuickFramebufferObject::createRenderer on the scene graph
// thread. The renderer schedules item updates for new frames while the item
// is alive.
QQuickFramebufferObject::Renderer* create_mpv_renderer(QQuickFramebufferObject* item,
                                                       mpv_handle* handle);

}  // namespace niratan::player
