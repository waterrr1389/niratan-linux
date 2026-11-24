// SPDX-License-Identifier: GPL-3.0-or-later
//
// The playback header of the Sasayaki inspector, following
// Features/Sasayaki/SasayakiSheet.swift (playbackHeader, audiobookCover,
// audioControls, formatTime): the audiobook cover beside the title and artist,
// a scrubber over elapsed and remaining time, and the transport row of skip,
// previous cue, play/pause, next cue and skip controls. It is shown only while
// audio is loaded; the panel owns the tab content and keeps playback state
// across tab switches.
//
// The cover falls back to the book cover and then to a waveform glyph. PLAN.md
// keeps glass flat, so the Swift cover shadow is not reproduced.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: header

    objectName: "sasayakiPlaybackHeader"

    required property SasayakiViewModel player

    property real scrubTime: -1

    function formatTime(seconds) {
        const total = Math.max(0, Math.floor(seconds));
        const hours = Math.floor(total / 3600);
        const minutes = Math.floor((total % 3600) / 60);
        const rest = total % 60;
        return (hours < 10 ? "0" : "") + hours + ":" + (minutes < 10 ? "0" : "") + minutes
               + ":" + (rest < 10 ? "0" : "") + rest;
    }

    spacing: Theme.spacingMd

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingMd

        Rectangle {
            implicitWidth: Theme.audiobookCoverSize
            implicitHeight: Theme.audiobookCoverSize
            radius: Theme.audiobookCoverRadius
            color: Theme.audiobookCoverBackground
            clip: true

            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                source: header.player.audiobookArtworkSource.length > 0
                        ? header.player.audiobookArtworkSource : ReaderViewModel.coverUrl
                visible: source.toString().length > 0
            }

            Icon {
                anchors.centerIn: parent
                visible: header.player.audiobookArtworkSource.length === 0
                         && ReaderViewModel.coverUrl.length === 0
                name: "waveform"
                size: Theme.audiobookCoverGlyphSize
                color: Theme.textSecondary
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.panelHeaderTitleSpacing

            StyledLabel {
                Layout.fillWidth: true
                text: header.player.audiobookTitle.length > 0
                      ? header.player.audiobookTitle : ReaderViewModel.title
                textStyle: Theme.headlineFont
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            StyledLabel {
                Layout.fillWidth: true
                visible: header.player.audiobookArtist.length > 0
                text: header.player.audiobookArtist
                secondary: true
                textStyle: Theme.captionFont
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.readerInspectorTimeSpacing

        Slider {
            Layout.fillWidth: true
            from: 0
            to: Math.max(header.player.duration, 1)
            enabled: header.player.duration > 0
            value: header.scrubTime >= 0 ? header.scrubTime : header.player.position
            onMoved: header.scrubTime = value
            onPressedChanged: {
                if (!pressed && header.scrubTime >= 0) {
                    header.player.seek(header.scrubTime);
                    header.scrubTime = -1;
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            StyledLabel {
                text: header.formatTime(header.scrubTime >= 0 ? header.scrubTime
                                                              : header.player.position)
                secondary: true
                textStyle: Theme.captionFont
                monospacedDigits: true
            }

            Item { Layout.fillWidth: true }

            StyledLabel {
                text: "-" + header.formatTime(
                      Math.max(header.player.duration
                               - (header.scrubTime >= 0 ? header.scrubTime
                                                        : header.player.position), 0))
                secondary: true
                textStyle: Theme.captionFont
                monospacedDigits: true
            }
        }
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: Theme.audiobookTransportSpacing

        IconButton {
            icon: "15.arrow.trianglehead.counterclockwise"
            diameter: Theme.audiobookTransportDiameter
            iconSize: Theme.audiobookSkipGlyphSize
            accessibleName: qsTr("Back 15 seconds")
            onClicked: header.player.skipForward(false)
        }

        IconButton {
            icon: "backward.fill"
            diameter: Theme.audiobookTransportDiameter
            iconSize: Theme.audiobookCueGlyphSize
            accessibleName: qsTr("Previous cue")
            onClicked: header.player.previousCue()
        }

        IconButton {
            prominent: true
            diameter: Theme.audiobookPlayDiameter
            iconSize: Theme.audiobookPlayGlyphSize
            icon: header.player.playing ? "pause.fill" : "play.fill"
            accessibleName: header.player.playing ? qsTr("Pause") : qsTr("Play")
            onClicked: header.player.togglePlayback()
        }

        IconButton {
            icon: "forward.fill"
            diameter: Theme.audiobookTransportDiameter
            iconSize: Theme.audiobookCueGlyphSize
            accessibleName: qsTr("Next cue")
            onClicked: header.player.nextCue()
        }

        IconButton {
            icon: "15.arrow.trianglehead.clockwise"
            diameter: Theme.audiobookTransportDiameter
            iconSize: Theme.audiobookSkipGlyphSize
            accessibleName: qsTr("Forward 15 seconds")
            onClicked: header.player.skipForward(true)
        }
    }
}
