// SPDX-License-Identifier: GPL-3.0-or-later
//
// GalleryView.swift thumbnail and GalleryImagePreview: a white glyph in
// a dark circular scrim over media, with no pointer handling of its own.
// The video library sets its own diameter (implicitWidth), glyphSize and glyphColor.
import QtQuick
import Niratan

Rectangle {
    id: cue

    property string symbol
    property bool large: false
    property color glyphColor: Theme.overlayForeground
    property int glyphSize: large ? Theme.galleryPreviewCueGlyph : Theme.galleryThumbnailCueGlyph

    implicitWidth: large ? Theme.galleryPreviewCueDiameter : Theme.galleryThumbnailCueDiameter
    implicitHeight: implicitWidth
    radius: width / 2
    color: Theme.mediaOverlayScrim

    Icon {
        anchors.centerIn: parent
        name: cue.symbol
        size: cue.glyphSize
        color: cue.glyphColor
    }
}
