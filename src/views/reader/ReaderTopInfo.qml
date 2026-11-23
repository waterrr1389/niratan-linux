// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's top information overlay, following
// NativeMac/NativeReaderView.swift `nativeTopInfoOverlay`: the book title on
// one centred line with middle truncation in the headline style, and below it
// the top progress line at 12 px with tabular digits, 2 px apart, 10 from the
// top and never closer than 96 to either side. It is text only, so it never
// takes a tap from the chapter.
//
// Swift draws bare text over a background-colour edge gradient; the Mac
// screenshots show it on a glass capsule. Without glass the text would sit
// directly on vertical body text, so it is drawn on the solid capsule surface
// used by the other reader controls.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: topInfo

    objectName: "readerTopInfo"

    readonly property bool showsProgress: ReaderViewModel.showProgressTop
                                          && ReaderViewModel.progressText.length > 0
    readonly property real maximumWidth: parent.width - 2 * 96

    anchors.top: parent.top
    anchors.topMargin: 10
    anchors.horizontalCenter: parent.horizontalCenter
    readonly property real naturalWidth: Math.max(title.visible ? title.implicitWidth : 0,
                                                  progress.visible ? progress.implicitWidth : 0)

    width: Math.min(naturalWidth + 2 * Theme.readerInfoCapsulePaddingHorizontal,
                    maximumWidth)
    height: lines.implicitHeight + 2 * Theme.readerInfoCapsulePaddingVertical
    visible: ReaderViewModel.showTitle || showsProgress

    GlassCapsule {
        anchors.fill: parent
    }

    Column {
        id: lines

        anchors.centerIn: parent
        width: topInfo.width - 2 * Theme.readerInfoCapsulePaddingHorizontal
        spacing: 2

        Text {
            id: title

            objectName: "readerTopTitle"
            width: parent.width
            visible: ReaderViewModel.showTitle
            text: ReaderViewModel.title
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
            maximumLineCount: 1
            color: ReaderViewModel.usesCustomInfoColor ? ReaderViewModel.infoColor
                                                       : Theme.textPrimary
            font.pixelSize: Theme.fontTitle
            font.bold: true
        }

        Text {
            id: progress

            objectName: "readerTopProgress"
            width: parent.width
            visible: topInfo.showsProgress
            text: ReaderViewModel.progressText
            horizontalAlignment: Text.AlignHCenter
            color: ReaderViewModel.usesCustomInfoColor ? ReaderViewModel.infoColor
                                                       : Theme.textSecondary
            font.pixelSize: 12
            font.features: ({ "tnum": 1 })
        }
    }
}
