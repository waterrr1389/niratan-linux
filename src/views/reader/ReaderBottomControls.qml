// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's bottom overlay, following NativeMac/NativeReaderView.swift
// `nativeBottomInfoOverlay` and `nativeBottomControls`: the statistics line
// then the progress when it is shown at the bottom, both centred in caption
// with tabular digits, and a trailing row of 34 px glass icon buttons with 16 px
// symbols, 8 apart, inset 32 from the trailing edge and 18 from the bottom.
//
// The order follows Swift: Back on the leading side, then on the trailing side
// Forward before the tracking toggle, the lookup-aware playback toggle, Go to,
// Appearance, Statistics, Gallery and Sasayaki. Each history capsule appears
// only when its target resolves; Lyrics Mode has no Linux backing and is not
// drawn.

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
        anchors.bottomMargin: Theme.readerToolInsetBottom
        spacing: Theme.readerInfoLineSpacing
        visible: controls.showsStatistics || controls.showsProgress

        StyledLabel {
            objectName: "readerStatisticsLine"
            visible: controls.showsStatistics
            text: ReaderViewModel.statisticsText
            horizontalAlignment: Text.AlignHCenter
            color: ReaderViewModel.usesCustomInfoColor ? ReaderViewModel.infoColor
                                                       : Theme.textSecondary
            textStyle: Theme.captionFont
            monospacedDigits: true
        }

        StyledLabel {
            objectName: "readerBottomProgress"
            visible: controls.showsProgress
            text: ReaderViewModel.progressText
            horizontalAlignment: Text.AlignHCenter
            color: ReaderViewModel.usesCustomInfoColor ? ReaderViewModel.infoColor
                                                       : Theme.textSecondary
            textStyle: Theme.captionFont
            monospacedDigits: true
        }
    }

    RowLayout {
        anchors.left: parent.left
        anchors.leftMargin: Theme.readerToolInsetTrailing
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.readerToolInsetBottom
        spacing: Theme.readerToolSpacing

        ChipButton {
            objectName: "readerBackButton"
            visible: ReaderViewModel.backTarget >= 0
            Layout.preferredHeight: Theme.readerToolDiameter
            glyph: "arrow.counterclockwise"
            text: ReaderViewModel.backTargetText
            onClicked: ReaderViewModel.navigateBackwards()
        }
    }

    RowLayout {
        anchors.right: parent.right
        anchors.rightMargin: Theme.readerToolInsetTrailing
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.readerToolInsetBottom
        spacing: Theme.readerToolSpacing

        ChipButton {
            objectName: "readerForwardButton"
            visible: ReaderViewModel.forwardTarget >= 0
            Layout.preferredHeight: Theme.readerToolDiameter
            glyph: "arrow.clockwise"
            glyphAfterText: true
            text: ReaderViewModel.forwardTargetText
            onClicked: ReaderViewModel.navigateForwards()
        }

        IconButton {
            objectName: "readerTrackingButton"
            visible: ReaderViewModel.statisticsEnabled && ReaderViewModel.showStatisticsToggle
            icon: ReaderViewModel.tracking ? "timer" : "chart.xyaxis.line"
            diameter: Theme.readerToolDiameter
            iconSize: Theme.readerToolGlyph
            accessibleName: ReaderViewModel.tracking ? qsTr("Pause Tracking")
                                                     : qsTr("Start Tracking")
            onClicked: ReaderViewModel.toggleTracking()
        }

        IconButton {
            objectName: "readerPlaybackButton"
            visible: controls.sasayakiEnabled && ReaderViewModel.showSasayakiToggle
                     && controls.player.hasAudio
            icon: controls.player.playing || controls.pausedByLookup ? "pause.fill" : "waveform"
            diameter: Theme.readerToolDiameter
            iconSize: Theme.readerToolGlyph
            accessibleName: controls.player.playing || controls.pausedByLookup
                            ? qsTr("Pause Sasayaki") : qsTr("Play Sasayaki")
            onClicked: controls.sasayakiToggleRequested()
        }

        IconButton {
            objectName: "readerGoToButton"
            icon: "list.bullet"
            diameter: Theme.readerToolDiameter
            iconSize: Theme.readerToolGlyph
            accessibleName: qsTr("Go to")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("goTo")
        }

        IconButton {
            objectName: "readerAppearanceButton"
            icon: "paintpalette"
            diameter: Theme.readerToolDiameter
            iconSize: Theme.readerToolGlyph
            accessibleName: qsTr("Appearance")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("appearance")
        }

        IconButton {
            objectName: "readerStatisticsButton"
            visible: ReaderViewModel.statisticsEnabled
            icon: "chart.bar.xaxis"
            diameter: Theme.readerToolDiameter
            iconSize: Theme.readerToolGlyph
            accessibleName: qsTr("Statistics")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("statistics")
        }

        IconButton {
            objectName: "readerGalleryButton"
            visible: ReaderViewModel.hasBook
            icon: "photo.on.rectangle"
            diameter: Theme.readerToolDiameter
            iconSize: Theme.readerToolGlyph
            accessibleName: qsTr("Gallery")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("gallery")
        }

        IconButton {
            objectName: "readerSasayakiButton"
            visible: controls.sasayakiEnabled && ReaderViewModel.hasBook
            icon: "headphones"
            diameter: Theme.readerToolDiameter
            iconSize: Theme.readerToolGlyph
            accessibleName: qsTr("Sasayaki")
            ToolTip.visible: hovered
            ToolTip.text: accessibleName
            onClicked: controls.toolRequested("sasayaki")
        }
    }
}
