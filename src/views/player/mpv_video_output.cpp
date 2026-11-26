// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video output controls of MpvItem: the aspect override (VideoAspectRatio
// raw values written to mpv's video-aspect-override), the clockwise rotation,
// and the video-only screenshot capture with the save-panel copy. Splitting
// them out keeps the property-polling half of the item in mpv_item.cpp.

#include "mpv_item.h"

#include "services/player/screenshot_capture.h"

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <mpv/client.h>

#include <cmath>
#include <cstdint>

QString MpvItem::aspectRatio() const {
    return aspect_ratio_;
}

void MpvItem::setAspectRatio(const QString& aspectRatio) {
    const QString value = aspectRatio.isEmpty() ? QStringLiteral("-1") : aspectRatio;
    if (aspect_ratio_ == value) {
        return;
    }
    aspect_ratio_ = value;
    set_property("video-aspect-override", aspect_ratio_);
    emit aspectRatioChanged();
}

void MpvItem::rotateClockwise() {
    if (handle_ == nullptr) {
        return;
    }
    int rotation = static_cast<int>(std::lround(get_double("video-rotate", 0.0)));
    rotation = ((rotation % 360) + 360) % 360;
    set_property("video-rotate", (rotation + 90) % 360);
}

bool MpvItem::screenshot(const QUrl& file) {
    if (handle_ == nullptr || !file.isLocalFile() || file.toLocalFile().isEmpty()) {
        return false;
    }
    const QByteArray path = file.toLocalFile().toUtf8();
    const char* args[] = {"screenshot-to-file", path.constData(), "video", nullptr};
    // A capture can wait for the render thread; do not block the GUI thread.
    // The file exists only once mpv replies, so the reply reports completion.
    const quint64 reply = next_screenshot_reply_++;
    if (mpv_command_async(handle_, reply, args) < 0) {
        return false;
    }
    pending_screenshots_.insert(reply, file);
    return true;
}

QUrl MpvItem::temporaryScreenshotFile() const {
    return QUrl::fromLocalFile(niratan::player::temporary_screenshot_path());
}

bool MpvItem::writeScreenshotFile(const QUrl& source, const QUrl& destination) {
    if (!source.isLocalFile()) {
        return false;
    }
    if (destination.isEmpty()) {
        return niratan::player::save_screenshot_capture(source.toLocalFile(), {});
    }
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty()) {
        return false;
    }
    return niratan::player::save_screenshot_capture(source.toLocalFile(),
                                                    destination.toLocalFile());
}
