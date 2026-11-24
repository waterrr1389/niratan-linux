// SPDX-License-Identifier: GPL-3.0-or-later
//
// GalleryView.swift thumbnail and GalleryImagePreview: a white glyph in
// a dark circular scrim over media, with no pointer handling of its own.
import QtQuick
import Niratan

Rectangle {
    id: cue

    property string symbol
    property bool large: false

    implicitWidth: large ? Theme.galleryPreviewCueDiameter : Theme.galleryThumbnailCueDiameter
    implicitHeight: implicitWidth
    radius: width / 2
    color: Theme.mediaOverlayScrim

    Icon {
        anchors.centerIn: parent
        name: cue.symbol
        size: cue.large ? Theme.galleryPreviewCueGlyph : Theme.galleryThumbnailCueGlyph
        color: Theme.overlayForeground
    }
}
