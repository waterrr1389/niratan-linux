// SPDX-License-Identifier: GPL-3.0-or-later
//
// One poster card in the video library grid, following
// Features/Video/VideoLibraryView.swift `VideoLibraryPosterCardView`,
// `VideoLibraryPosterArtworkView`, `VideoLibraryProgressTrack`,
// `VideoLibraryPlaybackText.state` and `.location`, `VideoThumbnailImageView`
// and `VideoLibraryItemContextMenu` (local branch).
//
// The card draws the 16:9 film placeholder with the favorite star, the
// playback overlay (a check and "Watched", otherwise the remaining time or
// position and a progress track), a play glyph on hover, an accent ring when
// selected, the two-line display title and the "parentFolder · fileSize" line.
// Clicking opens; the context menu matches the list row's.
pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: card

    property var rowData
    property bool selected: false
    property real posterWidth: 160

    readonly property string rowID: rowData ? rowData.id : ""
    readonly property string displayTitle: rowData ? rowData.displayTitle : ""
    readonly property var playback: rowData && rowData.playback ? rowData.playback : null
    readonly property bool hasPlayback: playback !== null
    readonly property bool isFinished: playback ? playback.finished === true : false
    readonly property real progress: playback && playback.progress !== undefined
                                     ? playback.progress : 0
    readonly property bool isFavorite: rowData && rowData.metadata
                                       ? rowData.metadata.isFavorite === true : false

    signal clicked()
    signal contextMenuRequested(point position)

    width: posterWidth
    implicitHeight: artwork.height + 9 + titleText.height + 3 + locationText.height

    function pad(value) {
        return value < 10 ? "0" + value : "" + value
    }

    // VideoTimeFormatter.string(from:).
    function timeText(seconds) {
        var total = Math.max(0, Math.floor(seconds))
        var hours = Math.floor(total / 3600)
        var minutes = Math.floor((total % 3600) / 60)
        var secs = total % 60
        return hours > 0 ? hours + ":" + pad(minutes) + ":" + pad(secs)
                         : minutes + ":" + pad(secs)
    }

    // ByteCountFormatter with the file style.
    function sizeText(bytes) {
        if (bytes < 1000)
            return qsTr("%1 bytes").arg(bytes)
        var units = ["KB", "MB", "GB", "TB"]
        var value = bytes / 1000
        var unit = 0
        while (value >= 1000 && unit < units.length - 1) {
            value /= 1000
            ++unit
        }
        var rounded = value >= 10 ? Math.round(value) : Math.round(value * 10) / 10
        return rounded + " " + units[unit]
    }

    function playbackStateText() {
        if (!hasPlayback)
            return ""
        if (isFinished)
            return qsTr("Watched")
        if (playback.remaining !== undefined)
            return qsTr("%1 left").arg(timeText(playback.remaining))
        return timeText(playback.position)
    }

    HoverHandler {
        id: hover
    }

    TapHandler {
        onTapped: card.clicked()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (eventPoint) => card.contextMenuRequested(eventPoint.position)
    }

    Item {
        id: artwork

        width: parent.width
        height: Math.round(width * 9 / 16)

        Rectangle {
            anchors.fill: parent
            radius: 10
            color: Theme.inactiveSelection
        }

        Icon {
            anchors.centerIn: parent
            name: "film"
            size: 26
            color: Theme.textTertiary
        }

        Rectangle {
            anchors.fill: parent
            radius: 10
            color: "transparent"
            border.width: 1
            border.color: Theme.cardStroke
        }

        // The playback scrim keeps the overlay legible on the placeholder;
        // VideoLibraryPosterArtworkView's LinearGradient runs clear to black.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.55
            visible: card.hasPlayback
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Theme.mediaOverlayScrim }
            }
        }

        Item {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 8
            width: 22
            height: 22
            visible: card.isFavorite

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: Theme.mediaOverlayScrim
            }

            Icon {
                objectName: "favoriteStar"

                anchors.centerIn: parent
                name: "star.fill"
                size: 10
                color: Theme.popupWarning
            }
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 9
            anchors.rightMargin: 9
            anchors.bottomMargin: 8
            spacing: 6
            visible: card.hasPlayback

            Row {
                anchors.right: parent.right
                spacing: 4

                Icon {
                    visible: card.isFinished
                    anchors.verticalCenter: parent.verticalCenter
                    name: "checkmark"
                    size: 9
                    color: Theme.overlayForeground
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: card.playbackStateText()
                    color: Theme.overlayForeground
                    font.pixelSize: Theme.fontCaption
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                }
            }

            Item {
                objectName: "progressTrack"

                readonly property real progress: card.progress

                width: parent.width
                height: 4
                visible: card.hasPlayback && !card.isFinished && card.progress > 0

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: Theme.overlayForeground
                    opacity: 0.32
                }

                Rectangle {
                    width: parent.width * Math.min(1, Math.max(0, card.progress))
                    height: parent.height
                    radius: height / 2
                    color: Theme.accent
                }
            }
        }

        // The play glyph appears on hover, as VideoLibraryPosterCardView does.
        Item {
            anchors.centerIn: parent
            width: 46
            height: 46
            visible: hover.hovered

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: Theme.mediaOverlayScrim
            }

            Icon {
                anchors.centerIn: parent
                name: "play.fill"
                size: 18
                color: Theme.overlayForeground
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: 14
            color: "transparent"
            border.width: 2.5
            border.color: Theme.accent
            visible: card.selected
        }
    }

    Text {
        id: titleText

        anchors.top: artwork.bottom
        anchors.topMargin: 9
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        text: card.displayTitle
        color: Theme.textPrimary
        font.pixelSize: 13
        font.weight: Font.DemiBold
        wrapMode: Text.Wrap
        maximumLineCount: 2
    }

    Row {
        id: locationText

        anchors.top: titleText.bottom
        anchors.topMargin: 3
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 2
        anchors.rightMargin: 2
        spacing: 5
        clip: true

        Text {
            text: card.rowData ? card.rowData.parentFolder : ""
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }

        Text {
            text: "\u00b7"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontCaption
        }

        Text {
            text: card.rowData ? card.sizeText(card.rowData.fileSize) : ""
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
    }
}
