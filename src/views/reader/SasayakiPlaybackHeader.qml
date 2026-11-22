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

    spacing: 12

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        Rectangle {
            implicitWidth: 64
            implicitHeight: 64
            radius: 10
            color: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g,
                           Theme.textSecondary.b, 0.10)
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
                size: 22
                color: Theme.textSecondary
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: header.player.audiobookTitle.length > 0
                      ? header.player.audiobookTitle : ReaderViewModel.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                font.bold: true
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: header.player.audiobookArtist.length > 0
                text: header.player.audiobookArtist
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

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

            Text {
                text: header.formatTime(header.scrubTime >= 0 ? header.scrubTime
                                                              : header.player.position)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                font.features: ({ "tnum": 1 })
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "-" + header.formatTime(
                      Math.max(header.player.duration
                               - (header.scrubTime >= 0 ? header.scrubTime
                                                        : header.player.position), 0))
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                font.features: ({ "tnum": 1 })
            }
        }
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: 10

        IconButton {
            icon: "15.arrow.trianglehead.counterclockwise"
            diameter: 34
            iconSize: 14
            accessibleName: qsTr("Back 15 seconds")
            onClicked: header.player.skipForward(false)
        }

        IconButton {
            icon: "backward.fill"
            diameter: 34
            iconSize: 13
            accessibleName: qsTr("Previous cue")
            onClicked: header.player.previousCue()
        }

        Rectangle {
            implicitWidth: 48
            implicitHeight: 48
            radius: 24
            color: Theme.accent

            Icon {
                anchors.centerIn: parent
                name: header.player.playing ? "pause.fill" : "play.fill"
                size: 20
                color: Theme.popupText
            }

            TapHandler {
                onTapped: header.player.togglePlayback()
            }
        }

        IconButton {
            icon: "forward.fill"
            diameter: 34
            iconSize: 13
            accessibleName: qsTr("Next cue")
            onClicked: header.player.nextCue()
        }

        IconButton {
            icon: "15.arrow.trianglehead.clockwise"
            diameter: 34
            iconSize: 14
            accessibleName: qsTr("Forward 15 seconds")
            onClicked: header.player.skipForward(true)
        }
    }
}
