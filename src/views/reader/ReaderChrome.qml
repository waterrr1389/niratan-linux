// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

// NativeReaderView.statisticsString and nativeBottomControls, in the header's
// trailing slot alongside the existing inspector buttons. The Sasayaki
// play/pause control follows NativeReaderView's enableSasayaki &&
// readerShowSasayakiToggle && hasAudio gate (B5); the page performs the toggle
// so it can clear the lookup's remembered pause first.
RowLayout {
    id: chrome
    required property SasayakiViewModel player
    required property bool sasayakiEnabled
    required property bool pausedByLookup
    signal sasayakiToggleRequested()
    spacing: Theme.spacingSm

    Label {
        visible: ReaderViewModel.showReadingSpeed && ReaderViewModel.tracking
        color: ReaderViewModel.infoColor
        font.pixelSize: Theme.fontCaption
        text: {
            const speed = ReaderViewModel.statisticsSessionSpeed
            return ReaderViewModel.englishContent
                ? qsTr("%1 words/h").arg(Math.ceil(speed / 5).toLocaleString(Qt.locale(), "f", 0))
                : qsTr("%1 \u5b57/h").arg(speed.toLocaleString(Qt.locale(), "f", 0))
        }
    }

    Label {
        visible: ReaderViewModel.showReadingTime && ReaderViewModel.tracking
        color: ReaderViewModel.infoColor
        font.pixelSize: Theme.fontCaption
        text: {
            const minutes = ReaderViewModel.statisticsSessionMinutes
            return qsTr("%1:%2").arg(Math.floor(minutes / 60)).arg(String(minutes % 60).padStart(2, "0"))
        }
    }

    ToolButton {
        visible: ReaderViewModel.showStatisticsToggle && ReaderViewModel.statisticsEnabled
        text: ReaderViewModel.tracking ? "\u275A\u275A" : "\u23F1"
        Accessible.name: ReaderViewModel.tracking ? qsTr("Pause Tracking") : qsTr("Start Tracking")
        ToolTip.text: Accessible.name
        ToolTip.visible: hovered
        onClicked: ReaderViewModel.toggleTracking()
    }

    ToolButton {
        visible: chrome.sasayakiEnabled && ReaderViewModel.showSasayakiToggle && chrome.player.hasAudio
        text: chrome.player.playing || chrome.pausedByLookup ? "\u275A\u275A" : "\u25B6"
        Accessible.name: chrome.player.playing || chrome.pausedByLookup ? qsTr("Pause Sasayaki") : qsTr("Play Sasayaki")
        ToolTip.text: Accessible.name
        ToolTip.visible: hovered
        onClicked: chrome.sasayakiToggleRequested()
    }
}
