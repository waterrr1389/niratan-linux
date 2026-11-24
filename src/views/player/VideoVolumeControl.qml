// SPDX-License-Identifier: GPL-3.0-or-later
//
// The mute button and its volume track, following
// Features/Video/VideoControlsView.swift `volumeControl` and
// `volumeSymbolName`.
//
// The glyph is speaker.slash.fill when muted or at zero volume,
// speaker.wave.1.fill below 34, speaker.wave.2.fill below 67 and
// speaker.wave.3.fill above; the track is 64 wide and sets the volume while
// dragged. Object names: volumeButton, volumeTrack.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

Row {
    id: volumeControl

    objectName: "videoVolumeControl"

    property bool isMuted: false
    property real volume: 100
    // The compact layout's overlay treatment for the volume track.
    property bool onOverlay: false

    readonly property string volumeSymbol: {
        if (volumeControl.isMuted || volumeControl.volume <= 0)
            return "speaker.slash.fill"
        if (volumeControl.volume < 34)
            return "speaker.wave.1.fill"
        if (volumeControl.volume < 67)
            return "speaker.wave.2.fill"
        return "speaker.wave.3.fill"
    }

    signal toggleMuteRequested()
    signal volumeChangeRequested(real value)

    spacing: Theme.chromeVolumeSpacing

    ChromeButton {
        id: muteButton

        objectName: "volumeButton"
        tip: volumeControl.isMuted ? qsTr("Unmute") : qsTr("Mute")
        symbol: volumeControl.volumeSymbol
        symbolSize: Theme.chromeVolumeGlyphSize
        bordered: true
        onClicked: volumeControl.toggleMuteRequested()
    }

    TrackSlider {
        id: volumeTrack

        objectName: "volumeTrack"
        onOverlay: volumeControl.onOverlay
        anchors.verticalCenter: parent.verticalCenter
        from: 0
        to: 100
        stepSize: 1
        focusPolicy: Qt.NoFocus
        value: Math.max(0, Math.min(100, volumeControl.isMuted ? 0 : volumeControl.volume))
        onMoved: volumeControl.volumeChangeRequested(volumeTrack.value)
    }
}
