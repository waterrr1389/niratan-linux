// SPDX-License-Identifier: GPL-3.0-or-later
//
// Window aspect arithmetic for the Video window, following
// Features/Video/VideoWindowChromeController.swift
// `VideoWindowAspectLayout.videoAspectRatio`, `fittedContentSize`,
// `aspectFittingSidebarWidth`, `resizeDriver` and `constrainedFrameSize`, and
// the cases in script/test_video_window_aspect_layout.swift.
//
// The helpers are pure and take the Linux window's content size: the desktop
// title bar lives outside the QML window and is never counted as decoration.
// Sizes are logical pixels; a ratio is width / height. Every function returns
// its input unchanged (or an empty optional) when an argument is not finite or
// not positive, mirroring the guards in Swift.
//
// `video_aspect_ratio` prefers the Inspector's aspect override over the
// media's display size and inverts the result for a quarter-turn rotation.
// `fitted_content_size` keeps the current height, shrinks it to the visible
// frame and to the room left beside the study sidebar, then derives the width
// from the ratio plus that sidebar, bounded by the visible frame.
// `aspect_fitting_sidebar_width` lets the study sidebar absorb the difference
// between the window width and the aspect-correct video width when that width
// is a legal sidebar width.
// `constrained_frame_size` is the live-resize guard: one frozen driver decides
// whether the width or the height leads, the other dimension follows the
// ratio, and the minimum frame size is respected.

#pragma once

#include <QSizeF>
#include <QString>

#include <optional>

namespace niratan::video {

// VideoAspectRatio.numericValue: "-1" automatic, otherwise 16:9, 4:3, 1:1 or
// 21:9. Unknown spellings count as automatic.
std::optional<qreal> aspect_override_ratio(const QString& rawValue);

std::optional<qreal> video_aspect_ratio(const QSizeF& displaySize,
                                        std::optional<qreal> overrideRatio, int rotation);

QSizeF fitted_content_size(const QSizeF& currentContentSize, qreal videoAspectRatio,
                           qreal sidebarWidth, const QSizeF& visibleContentFrame);

qreal aspect_fitting_sidebar_width(const QSizeF& contentSize, std::optional<qreal> videoAspectRatio,
                                   qreal proposedWidth, qreal minWidth, qreal maxWidth);

enum class ResizeDriver { Width, Height };

std::optional<ResizeDriver> resize_driver(const QSizeF& currentFrameSize,
                                          const QSizeF& proposedFrameSize, qreal videoAspectRatio);

QSizeF constrained_frame_size(const QSizeF& currentFrameSize, const QSizeF& proposedFrameSize,
                              const QSizeF& frameDecorationSize, qreal videoAspectRatio,
                              qreal sidebarWidth, const QSizeF& minimumFrameSize,
                              std::optional<ResizeDriver> driver = std::nullopt);

}  // namespace niratan::video
