// SPDX-License-Identifier: GPL-3.0-or-later
//
// The previous/play/next group and, when shown, the volume cluster, following
// Features/Video/VideoControlsView.swift `episodeControls` and `volumeControl`,
// and Features/Video/Playback/PlaybackEngine.swift `VideoPlaybackSpeed` for the
// pill label.
//
// Previous and next stay disabled because the port has no playlist; play is
// the filled pause while playing and the filled play while paused. The bar
// draws volume through VideoVolumeControl in the floating layout and hides the
// cluster here through `showVolume`; the compact row keeps both here.
//
// Object names: previousButton, playButton, nextButton, volumeButton,
// volumeTrack; the play glyph is playIcon. Each button carries a `tip` string
// for the tooltip and its accessibility name. `volumeLeads` orders the two
// clusters so the compact row can put the episode group first; `centered`
// centres the episode group inside a stretched transport.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

Item {
    id: transport

    objectName: "videoTransportControls"

    property bool isPlaying: false
    property bool isPaused: true
    property bool isMuted: false
    property real volume: 100
    property bool previousEnabled: false
    property bool nextEnabled: false
    property bool volumeLeads: true
    property bool showVolume: true
    property bool centered: false

    readonly property string volumeSymbol: {
        if (transport.isMuted || transport.volume <= 0)
            return "speaker.slash.fill"
        if (transport.volume < 34)
            return "speaker.wave.1.fill"
        if (transport.volume < 67)
            return "speaker.wave.2.fill"
        return "speaker.wave.3.fill"
    }

    implicitWidth: transport.centered ? 0
        : (transport.showVolume ? volumeLoader.width + episodeCluster.width + 10
                                : episodeCluster.width)
    implicitHeight: 42

    signal togglePlaybackRequested()
    signal previousRequested()
    signal nextRequested()
    signal toggleMuteRequested()
    signal volumeChangeRequested(real value)

    component TransportButton: Button {
        id: button

        property string tip
        property string symbol
        property double symbolSize: 15
        property bool active: false

        implicitWidth: 30
        implicitHeight: 30
        padding: 0
        focusPolicy: Qt.NoFocus

        ToolTip.visible: button.hovered && button.tip.length > 0
        ToolTip.text: button.tip
        ToolTip.delay: 700
        Accessible.name: button.tip

        contentItem: Icon {
            name: button.symbol
            size: button.symbolSize
            color: button.active ? Theme.accent : Theme.textPrimary
        }

        background: Rectangle {
            radius: button.height / 2
            color: button.active || button.down || button.hovered
                ? Theme.cardStroke : Theme.floatingBackground
            border.width: button.active ? Theme.cardBorderWidth : 0
            border.color: Theme.cardStroke
        }
    }

    Component {
        id: volumeComponent

        Item {
            id: volumeCluster

            width: volumeRow.implicitWidth
            height: volumeRow.implicitHeight

        Row {
            id: volumeRow

            spacing: 2

            TransportButton {
                objectName: "volumeButton"
                tip: transport.isMuted ? qsTr("Unmute") : qsTr("Mute")
                symbol: transport.volumeSymbol
                symbolSize: 14
                onClicked: transport.toggleMuteRequested()
            }

            Slider {
                id: volumeTrack

                objectName: "volumeTrack"
                width: 64
                height: 18
                anchors.verticalCenter: parent.verticalCenter
                from: 0
                to: 100
                stepSize: 1
                focusPolicy: Qt.NoFocus
                value: Math.max(0, Math.min(100, transport.isMuted ? 0 : transport.volume))
                onMoved: transport.volumeChangeRequested(volumeTrack.value)
            }
        }
    }
    }

    Loader {
        id: volumeLoader

        active: transport.showVolume
        sourceComponent: volumeComponent
        x: transport.centered ? 0
                              : (transport.volumeLeads ? 0 : episodeCluster.width + 10)
        anchors.verticalCenter: parent.verticalCenter
    }

    Item {
        id: episodeCluster

        x: transport.centered
           ? Math.max(0, (transport.width - width) / 2)
           : (transport.volumeLeads && transport.showVolume ? volumeLoader.width + 10 : 0)
        anchors.verticalCenter: parent.verticalCenter
        width: episodeRow.implicitWidth
        height: episodeRow.implicitHeight

        Row {
            id: episodeRow

            spacing: transport.volumeLeads ? 10 : 4

            TransportButton {
                objectName: "previousButton"
                tip: qsTr("Previous Episode")
                symbol: "backward.end.fill"
                enabled: transport.previousEnabled
                onClicked: transport.previousRequested()
            }

            Button {
                id: playButton

                objectName: "playButton"
                property string tip: transport.isPlaying ? qsTr("Pause") : qsTr("Play")
                implicitWidth: 42
                implicitHeight: 42
                padding: 0
                focusPolicy: Qt.NoFocus

                ToolTip.visible: playButton.hovered
                ToolTip.text: playButton.tip
                ToolTip.delay: 700
                Accessible.name: playButton.tip

                onClicked: transport.togglePlaybackRequested()

                contentItem: Icon {
                    objectName: "playIcon"
                    name: transport.isPlaying ? "pause.fill" : "play.fill"
                    size: 22
                    color: Theme.textPrimary
                }

                background: Rectangle {
                    radius: playButton.height / 2
                    color: playButton.down || playButton.hovered
                        ? Theme.cardStroke : Theme.floatingBackground
                    border.width: Theme.cardBorderWidth
                    border.color: Theme.cardStroke
                }
            }

            TransportButton {
                objectName: "nextButton"
                tip: qsTr("Next Episode")
                symbol: "forward.end.fill"
                enabled: transport.nextEnabled
                onClicked: transport.nextRequested()
            }
        }
    }
}
