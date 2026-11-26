// SPDX-License-Identifier: GPL-3.0-or-later
//
// QML access to models/video_window_aspect.h. The pure arithmetic lives in
// the model; this class only maps QML's strings and numbers onto it, so the
// Video window and the player page can share one implementation with the C++
// tests. A ratio of zero (or NaN) means that no ratio is known.
//
// QML name: VideoWindowAspect, a singleton in the Niratan module.

#pragma once

#include <QObject>
#include <QSizeF>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class VideoWindowAspectBridge : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(VideoWindowAspect)
    QML_SINGLETON

   public:
    explicit VideoWindowAspectBridge(QObject* parent = nullptr);

    // QVariant wraps an invalid QVariant when no ratio is known.
    Q_INVOKABLE QVariant videoAspectRatio(const QSizeF& displaySize, const QString& overrideValue,
                                          int rotation) const;
    Q_INVOKABLE QSizeF fittedContentSize(const QSizeF& currentContentSize, qreal videoAspectRatio,
                                         qreal sidebarWidth,
                                         const QSizeF& visibleContentFrame) const;
    Q_INVOKABLE qreal aspectFittingSidebarWidth(const QSizeF& contentSize, qreal videoAspectRatio,
                                                qreal proposedWidth, qreal minWidth,
                                                qreal maxWidth) const;
    // "width", "height" or an empty string when the delta is negligible.
    Q_INVOKABLE QString resizeDriver(const QSizeF& currentFrameSize,
                                     const QSizeF& proposedFrameSize, qreal videoAspectRatio) const;
    Q_INVOKABLE QSizeF constrainedFrameSize(const QSizeF& currentFrameSize,
                                            const QSizeF& proposedFrameSize,
                                            const QSizeF& frameDecorationSize,
                                            qreal videoAspectRatio, qreal sidebarWidth,
                                            const QSizeF& minimumFrameSize,
                                            const QString& resizeDriver) const;
};
