// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's top information overlay, following
// NativeMac/NativeReaderView.swift `nativeTopInfoOverlay`: the book title on
// one centred line with middle truncation in the headline style, and below it
// the top progress line at 12 px with tabular digits. The stack is inset 96
// horizontal and 10 from the top, uses 2 px spacing and is text only, so it
// never takes a tap from the chapter.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Column {
    id: topInfo

    objectName: "readerTopInfo"

    readonly property bool showsProgress: ReaderViewModel.showProgressTop
                                          && ReaderViewModel.progressText.length > 0

    anchors.top: parent.top
    anchors.topMargin: 10
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.leftMargin: 96
    anchors.rightMargin: 96
    spacing: 2
    visible: ReaderViewModel.showTitle || showsProgress

    Text {
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
