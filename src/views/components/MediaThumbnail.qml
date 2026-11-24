// SPDX-License-Identifier: GPL-3.0-or-later
//
// A 16:9 media thumbnail, following Features/Video/VideoLibraryView.swift
// `VideoLibraryPosterArtworkView`, `VideoThumbnailPlaceholderView` and
// `VideoThumbnailImageView`: a film glyph on the inactive fill stays under the
// cached frame while it decodes, the frame fills the rounded shape cropped,
// and a hairline stroke rims it. Callers lay overlays (badges, progress) as
// children; they draw above the frame. The height follows the width.
pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: thumbnail

    property string source
    property real radius: Theme.videoPosterRadius
    // The colour around the thumbnail, which covers the image's corners.
    property color surroundColor: Theme.windowBackground
    default property alias overlays: overlayLayer.data

    implicitHeight: Math.round(width / Theme.videoThumbnailAspect)

    Item {
        objectName: "thumbnailPlaceholder"

        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            radius: thumbnail.radius
            color: Theme.inactiveSelection
        }

        Icon {
            anchors.centerIn: parent
            name: "film"
            size: Theme.videoPlaceholderGlyph
            color: Theme.textTertiary
        }
    }

    RoundedImage {
        objectName: "thumbnailImage"

        anchors.fill: parent
        visible: thumbnail.source.length > 0
        source: thumbnail.source
        fillMode: Image.PreserveAspectCrop
        radius: thumbnail.radius
        maskWidth: thumbnail.radius
        maskColor: thumbnail.surroundColor
    }

    Rectangle {
        anchors.fill: parent
        radius: thumbnail.radius
        color: "transparent"
        border.width: Theme.hairlineWidth
        border.color: Theme.cardStroke
    }

    Item {
        id: overlayLayer

        anchors.fill: parent
    }
}
