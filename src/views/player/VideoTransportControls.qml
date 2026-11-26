// SPDX-License-Identifier: GPL-3.0-or-later
//
// The previous/play/next group and, when shown, the volume cluster, following
// Features/Video/VideoControlsView.swift `episodeControls` and `volumeControl`,
// and Features/Video/Playback/PlaybackEngine.swift `VideoPlaybackSpeed` for the
// pill label.
//
// Previous and next follow `previousEnabled` and `nextEnabled` from the bar;
// they are enabled only when the episode list has a neighbour in that
// direction. Play is the filled pause while playing and the filled play while
// paused. The bar draws volume through VideoVolumeControl in the floating
// layout and hides the cluster here through `showVolume`; the compact row
// keeps both here.
//
// Object names: previousButton, playButton, nextButton, volumeButton,
// volumeTrack; the play glyph is playIcon. Each button carries a `tip` string
// for the tooltip and its accessibility name. `volumeLeads` orders the two
// clusters so the compact row can put the episode group first.
pragma ComponentBehavior: Bound

import QtQuick
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
    // The compact layout's overlay treatment for the volume track.
    property bool onOverlay: false

    readonly property string volumeSymbol: {
        if (transport.isMuted || transport.volume <= 0)
            return "speaker.slash.fill"
        if (transport.volume < 34)
            return "speaker.wave.1.fill"
        if (transport.volume < 67)
            return "speaker.wave.2.fill"
        return "speaker.wave.3.fill"
    }

    implicitWidth: transport.showVolume
        ? volumeLoader.width + episodeCluster.width + Theme.chromeClusterSpacing
        : episodeCluster.width
    implicitHeight: Theme.chromePlayDiameter

    signal togglePlaybackRequested()
    signal previousRequested()
    signal nextRequested()
    signal toggleMuteRequested()
    signal volumeChangeRequested(real value)

    Component {
        id: volumeComponent

        Item {
            id: volumeCluster

            width: volumeRow.implicitWidth
            height: volumeRow.implicitHeight

        Row {
            id: volumeRow

            spacing: Theme.chromeVolumeSpacing

            ChromeButton {
                objectName: "volumeButton"
                tip: transport.isMuted ? qsTr("Unmute") : qsTr("Mute")
                symbol: transport.volumeSymbol
                symbolSize: Theme.chromeVolumeGlyphSize
                onClicked: transport.toggleMuteRequested()
            }

            TrackSlider {
                id: volumeTrack

                objectName: "volumeTrack"
                onOverlay: transport.onOverlay
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
        x: transport.volumeLeads ? 0 : episodeCluster.width + Theme.chromeClusterSpacing
        anchors.verticalCenter: parent.verticalCenter
    }

    Item {
        id: episodeCluster

        x: transport.volumeLeads && transport.showVolume
            ? volumeLoader.width + Theme.chromeClusterSpacing : 0
        anchors.verticalCenter: parent.verticalCenter
        width: episodeRow.implicitWidth
        height: episodeRow.implicitHeight

        Row {
            id: episodeRow

            spacing: transport.volumeLeads ? Theme.chromeClusterSpacing : Theme.spacingXs

            ChromeButton {
                objectName: "previousButton"
                anchors.verticalCenter: parent.verticalCenter
                tip: qsTr("Previous Episode")
                symbol: "backward.end.fill"
                enabled: transport.previousEnabled
                onClicked: transport.previousRequested()
            }

            ChromeButton {
                id: playButton

                objectName: "playButton"
                tip: transport.isPlaying ? qsTr("Pause") : qsTr("Play")
                symbol: transport.isPlaying ? "pause.fill" : "play.fill"
                symbolSize: Theme.chromePlayGlyphSize
                diameter: Theme.chromePlayDiameter
                glyphName: "playIcon"
                bordered: true
                onClicked: transport.togglePlaybackRequested()
            }

            ChromeButton {
                objectName: "nextButton"
                anchors.verticalCenter: parent.verticalCenter
                tip: qsTr("Next Episode")
                symbol: "forward.end.fill"
                enabled: transport.nextEnabled
                onClicked: transport.nextRequested()
            }
        }
    }
}
