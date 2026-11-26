// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_window_aspect_bridge.h"

#include "models/video_window_aspect.h"

#include <cmath>
#include <optional>

namespace {

std::optional<qreal> known_ratio(qreal ratio) {
    if (!std::isfinite(ratio) || ratio <= 0.0) {
        return std::nullopt;
    }
    return ratio;
}

}  // namespace

VideoWindowAspectBridge::VideoWindowAspectBridge(QObject* parent) : QObject(parent) {}

QVariant VideoWindowAspectBridge::videoAspectRatio(const QSizeF& displaySize,
                                                   const QString& overrideValue,
                                                   int rotation) const {
    const auto ratio = niratan::video::video_aspect_ratio(
        displaySize, niratan::video::aspect_override_ratio(overrideValue), rotation);
    if (!ratio) {
        return {};
    }
    return *ratio;
}

QSizeF VideoWindowAspectBridge::fittedContentSize(const QSizeF& currentContentSize,
                                                  qreal videoAspectRatio, qreal sidebarWidth,
                                                  const QSizeF& visibleContentFrame) const {
    return niratan::video::fitted_content_size(currentContentSize, videoAspectRatio, sidebarWidth,
                                               visibleContentFrame);
}

qreal VideoWindowAspectBridge::aspectFittingSidebarWidth(const QSizeF& contentSize,
                                                         qreal videoAspectRatio,
                                                         qreal proposedWidth, qreal minWidth,
                                                         qreal maxWidth) const {
    return niratan::video::aspect_fitting_sidebar_width(contentSize, known_ratio(videoAspectRatio),
                                                        proposedWidth, minWidth, maxWidth);
}

QString VideoWindowAspectBridge::resizeDriver(const QSizeF& currentFrameSize,
                                              const QSizeF& proposedFrameSize,
                                              qreal videoAspectRatio) const {
    const auto driver =
        niratan::video::resize_driver(currentFrameSize, proposedFrameSize, videoAspectRatio);
    if (!driver) {
        return {};
    }
    return *driver == niratan::video::ResizeDriver::Width ? QStringLiteral("width")
                                                          : QStringLiteral("height");
}

QSizeF VideoWindowAspectBridge::constrainedFrameSize(const QSizeF& currentFrameSize,
                                                     const QSizeF& proposedFrameSize,
                                                     const QSizeF& frameDecorationSize,
                                                     qreal videoAspectRatio, qreal sidebarWidth,
                                                     const QSizeF& minimumFrameSize,
                                                     const QString& driver) const {
    std::optional<niratan::video::ResizeDriver> resolved;
    if (driver == QStringLiteral("width")) {
        resolved = niratan::video::ResizeDriver::Width;
    } else if (driver == QStringLiteral("height")) {
        resolved = niratan::video::ResizeDriver::Height;
    }
    return niratan::video::constrained_frame_size(currentFrameSize, proposedFrameSize,
                                                  frameDecorationSize, videoAspectRatio,
                                                  sidebarWidth, minimumFrameSize, resolved);
}
