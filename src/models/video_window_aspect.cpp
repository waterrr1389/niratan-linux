// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/video_window_aspect.h"

#include <algorithm>
#include <cmath>

namespace niratan::video {
namespace {

bool is_quarter_turn(int rotation) {
    const int normalized = ((rotation % 360) + 360) % 360;
    return normalized == 90 || normalized == 270;
}

bool all_finite(const QSizeF& size) {
    return std::isfinite(size.width()) && std::isfinite(size.height());
}

bool positive(const QSizeF& size) {
    return size.width() > 0.0 && size.height() > 0.0;
}

constexpr qreal kTolerance = 0.5;

}  // namespace

std::optional<qreal> aspect_override_ratio(const QString& rawValue) {
    if (rawValue == QStringLiteral("16:9")) {
        return 16.0 / 9.0;
    }
    if (rawValue == QStringLiteral("4:3")) {
        return 4.0 / 3.0;
    }
    if (rawValue == QStringLiteral("1:1")) {
        return 1.0;
    }
    if (rawValue == QStringLiteral("21:9")) {
        return 21.0 / 9.0;
    }
    return std::nullopt;
}

std::optional<qreal> video_aspect_ratio(const QSizeF& displaySize,
                                        std::optional<qreal> overrideRatio, int rotation) {
    std::optional<qreal> base;
    if (overrideRatio) {
        base = overrideRatio;
    } else if (displaySize.width() > 0.0 && displaySize.height() > 0.0) {
        base = displaySize.width() / displaySize.height();
    }

    if (!base || !std::isfinite(*base) || *base <= 0.0) {
        return std::nullopt;
    }
    return is_quarter_turn(rotation) ? 1.0 / *base : *base;
}

QSizeF fitted_content_size(const QSizeF& currentContentSize, qreal videoAspectRatio,
                           qreal sidebarWidth, const QSizeF& visibleContentFrame) {
    if (!std::isfinite(videoAspectRatio) || videoAspectRatio <= 0.0) {
        return currentContentSize;
    }

    sidebarWidth = std::max(sidebarWidth, 0.0);
    const qreal maxWidth = std::max(visibleContentFrame.width(), 1.0);
    const qreal maxHeight = std::max(visibleContentFrame.height(), 1.0);
    const qreal availableVideoWidth = std::max(maxWidth - sidebarWidth, 1.0);
    qreal height = std::max(currentContentSize.height(), 1.0);

    if (height > maxHeight) {
        height = maxHeight;
    }
    if (height * videoAspectRatio > availableVideoWidth) {
        height = std::max(availableVideoWidth / videoAspectRatio, 1.0);
    }

    const qreal width = std::min(height * videoAspectRatio + sidebarWidth, maxWidth);
    return QSizeF(std::max(width, 1.0), std::max(height, 1.0));
}

qreal aspect_fitting_sidebar_width(const QSizeF& contentSize, std::optional<qreal> videoAspectRatio,
                                   qreal proposedWidth, qreal minWidth, qreal maxWidth) {
    const qreal clampedProposedWidth = std::min(std::max(proposedWidth, minWidth), maxWidth);
    if (!videoAspectRatio || !std::isfinite(*videoAspectRatio) || *videoAspectRatio <= 0.0 ||
        !std::isfinite(contentSize.width()) || !std::isfinite(contentSize.height()) ||
        contentSize.width() <= 0.0 || contentSize.height() <= 0.0) {
        return clampedProposedWidth;
    }

    const qreal fittedWidth = contentSize.width() - contentSize.height() * *videoAspectRatio;
    if (!std::isfinite(fittedWidth) || fittedWidth < minWidth || fittedWidth > maxWidth) {
        return clampedProposedWidth;
    }
    return fittedWidth;
}

std::optional<ResizeDriver> resize_driver(const QSizeF& currentFrameSize,
                                          const QSizeF& proposedFrameSize, qreal videoAspectRatio) {
    if (!all_finite(currentFrameSize) || !all_finite(proposedFrameSize) ||
        !std::isfinite(videoAspectRatio) || !positive(currentFrameSize) ||
        !positive(proposedFrameSize) || videoAspectRatio <= 0.0) {
        return std::nullopt;
    }

    const qreal widthDelta = proposedFrameSize.width() - currentFrameSize.width();
    const qreal heightDelta = proposedFrameSize.height() - currentFrameSize.height();
    if (std::abs(widthDelta) <= kTolerance && std::abs(heightDelta) <= kTolerance) {
        return std::nullopt;
    }
    if (std::abs(heightDelta) <= kTolerance) {
        return ResizeDriver::Width;
    }
    if (std::abs(widthDelta) <= kTolerance) {
        return ResizeDriver::Height;
    }
    return std::abs(widthDelta / videoAspectRatio) >= std::abs(heightDelta) ? ResizeDriver::Width
                                                                            : ResizeDriver::Height;
}

QSizeF constrained_frame_size(const QSizeF& currentFrameSize, const QSizeF& proposedFrameSize,
                              const QSizeF& frameDecorationSize, qreal videoAspectRatio,
                              qreal sidebarWidth, const QSizeF& minimumFrameSize,
                              std::optional<ResizeDriver> driver) {
    if (!all_finite(currentFrameSize) || !all_finite(proposedFrameSize) ||
        !all_finite(frameDecorationSize) || !std::isfinite(videoAspectRatio) ||
        !std::isfinite(sidebarWidth) || !all_finite(minimumFrameSize) ||
        !positive(currentFrameSize) || !positive(proposedFrameSize) || videoAspectRatio <= 0.0) {
        return proposedFrameSize;
    }

    const qreal decorationWidth = std::max(frameDecorationSize.width(), 0.0);
    const qreal decorationHeight = std::max(frameDecorationSize.height(), 0.0);
    sidebarWidth = std::max(sidebarWidth, 0.0);
    const qreal currentContentWidth = std::max(currentFrameSize.width() - decorationWidth, 1.0);
    const qreal currentContentHeight = std::max(currentFrameSize.height() - decorationHeight, 1.0);
    const qreal proposedContentWidth = std::max(proposedFrameSize.width() - decorationWidth, 1.0);
    const qreal proposedContentHeight =
        std::max(proposedFrameSize.height() - decorationHeight, 1.0);
    const qreal widthDelta = proposedContentWidth - currentContentWidth;
    const qreal heightDelta = proposedContentHeight - currentContentHeight;

    if (std::abs(widthDelta) <= kTolerance && std::abs(heightDelta) <= kTolerance) {
        return proposedFrameSize;
    }

    ResizeDriver resolvedDriver;
    if (driver) {
        resolvedDriver = *driver;
    } else if (std::abs(heightDelta) <= kTolerance) {
        resolvedDriver = ResizeDriver::Width;
    } else if (std::abs(widthDelta) <= kTolerance) {
        resolvedDriver = ResizeDriver::Height;
    } else if (std::abs(widthDelta / videoAspectRatio) >= std::abs(heightDelta)) {
        resolvedDriver = ResizeDriver::Width;
    } else {
        resolvedDriver = ResizeDriver::Height;
    }

    qreal contentHeight = resolvedDriver == ResizeDriver::Width
        ? (proposedContentWidth - sidebarWidth) / videoAspectRatio
        : proposedContentHeight;
    const qreal minimumContentHeight = std::max(
        std::max(minimumFrameSize.height() - decorationHeight,
                 (minimumFrameSize.width() - decorationWidth - sidebarWidth) / videoAspectRatio),
        1.0);
    contentHeight = std::max(contentHeight, minimumContentHeight);

    const QSizeF result(contentHeight * videoAspectRatio + sidebarWidth + decorationWidth,
                        contentHeight + decorationHeight);
    if (!std::isfinite(result.width()) || !std::isfinite(result.height()) ||
        result.width() <= 0.0 || result.height() <= 0.0) {
        return proposedFrameSize;
    }
    return result;
}

}  // namespace niratan::video
