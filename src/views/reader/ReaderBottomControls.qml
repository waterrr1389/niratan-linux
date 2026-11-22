// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's bottom overlay, following NativeMac/NativeReaderView.swift
// `nativeBottomInfoOverlay` and `nativeBottomControls`: the statistics line
// then the progress when it is shown at the bottom, both centred at 11 px with
// tabular digits, and a trailing row of 34 px glass icon buttons with 16 px
// symbols, 8 apart, inset 32 from the trailing edge and 18 from the bottom.
//
// The order follows Swift: the tracking toggle, the lookup-aware playback
// toggle, then Go to, Appearance, Statistics, Gallery and Sasayaki, each shown
// only when its feature is enabled. History back/forward capsules and Lyrics
// Mode have no Linux backing and are not drawn.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: controls

    objectName: "readerBottomControls"

    required property SasayakiViewModel player
    required property bool sasayakiEnabled
    required property bool pausedByLookup

    signal toolRequested(string tool)
    signal sasayakiToggleRequested()

    readonly property bool showsStatistics: ReaderViewModel.statisticsText.length > 0
    readonly property bool showsProgress: !ReaderViewModel.showProgressTop
                                          && ReaderViewModel.progressText.length > 0

    Column {
        id: bottomInfo

        objectName: "readerBottomInfo"

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        spacing: 2
        visible: controls.showsStatistics || controls.showsProgress

        Text {
            objectName: "readerStatisticsLine"
            visible: controls.showsStatistics
            text: ReaderViewModel.statisticsText
            horizontalAlignment: Text.AlignHCenter
            color: ReaderViewModel.usesCustomInfoColor ? ReaderViewModel.infoColor
                                                       : Theme.textSecondary
            font.pixelSize: 11
            font.features: ({ "tnum": 1 })
        }

        Text {
            objectName: "readerBottomProgress"
            visible: controls.showsProgress
            text: ReaderViewModel.progressText
            horizontalAlignment: Text.AlignHCenter
            color: ReaderViewModel.usesCustomInfoColor ? ReaderViewModel.infoColor
                                                       : Theme.textSecondary
            font.pixelSize: 11
            font.features: ({ "tnum": 1 })
        }
    }

    RowLayout {
        anchors.right: parent.right
        anchors.rightMargin: 32
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        spacing: 8

        IconButton {
            objectName: "readerTrackingButton"
            visible: ReaderViewModel.statisticsEnabled && ReaderViewModel.showStatisticsToggle
            icon: ReaderViewModel.tracking ? "timer" : "chart.xyaxis.line"
            diameter: 34
            iconSize: 16
            accessibleName: ReaderViewModel.tracking ? qsTr("Pause Tracking")
                                                     : qsTr("Start Tracking")
            onClicked: ReaderViewModel.toggleTracking()
        }

        IconButton {
            objectName: "readerPlaybackButton"
            visible: controls.sasayakiEnabled && ReaderViewModel.showSasayakiToggle
                     && controls.player.hasAudio
            icon: controls.player.playing || controls.pausedByLookup ? "pause.fill" : "waveform"
            diameter: 34
            iconSize: 16
            accessibleName: controls.player.playing || controls.pausedByLookup
                            ? qsTr("Pause Sasayaki") : qsTr("Play Sasayaki")
            onClicked: controls.sasayakiToggleRequested()
        }

        IconButton {
            objectName: "readerGoToButton"
            icon: "list.bullet"
            diameter: 34
            iconSize: 16
            accessibleName: qsTr("Go to")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("goTo")
        }

        IconButton {
            objectName: "readerAppearanceButton"
            icon: "paintpalette"
            diameter: 34
            iconSize: 16
            accessibleName: qsTr("Appearance")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("appearance")
        }

        IconButton {
            objectName: "readerStatisticsButton"
            visible: ReaderViewModel.statisticsEnabled
            icon: "chart.bar.xaxis"
            diameter: 34
            iconSize: 16
            accessibleName: qsTr("Statistics")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("statistics")
        }

        IconButton {
            objectName: "readerGalleryButton"
            visible: ReaderViewModel.hasBook
            icon: "photo.on.rectangle"
            diameter: 34
            iconSize: 16
            accessibleName: qsTr("Gallery")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("gallery")
        }

        IconButton {
            objectName: "readerSasayakiButton"
            visible: controls.sasayakiEnabled && ReaderViewModel.hasBook
            icon: "headphones"
            diameter: 34
            iconSize: 16
            accessibleName: qsTr("Sasayaki")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("sasayaki")
        }
    }
}
