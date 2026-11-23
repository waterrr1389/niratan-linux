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

    spacing: 2

    Button {
        id: muteButton

        objectName: "volumeButton"
        property string tip: volumeControl.isMuted ? qsTr("Unmute") : qsTr("Mute")
        implicitWidth: 34
        implicitHeight: 34
        padding: 0
        horizontalPadding: 0
        focusPolicy: Qt.NoFocus
        onClicked: volumeControl.toggleMuteRequested()

        ToolTip.visible: muteButton.hovered
        ToolTip.text: muteButton.tip
        ToolTip.delay: 700
        Accessible.name: muteButton.tip

        contentItem: Icon {
            name: volumeControl.volumeSymbol
            size: 17
            color: Theme.textPrimary
        }

        background: Rectangle {
            radius: muteButton.height / 2
            color: muteButton.down || muteButton.hovered
                ? Theme.cardStroke : Theme.floatingBackground
            border.width: Theme.cardBorderWidth
            border.color: Theme.cardStroke
        }
    }

    TrackSlider {
        id: volumeTrack

        objectName: "volumeTrack"
        anchors.verticalCenter: parent.verticalCenter
        from: 0
        to: 100
        stepSize: 1
        focusPolicy: Qt.NoFocus
        value: Math.max(0, Math.min(100, volumeControl.isMuted ? 0 : volumeControl.volume))
        onMoved: volumeControl.volumeChangeRequested(volumeTrack.value)
    }
}
