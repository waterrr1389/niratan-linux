// SPDX-License-Identifier: GPL-3.0-or-later
//
// One poster card in the video library grid, following
// Features/Video/VideoLibraryView.swift `VideoLibraryPosterCardView`,
// `VideoLibraryPosterArtworkView`, `VideoLibraryProgressTrack`,
// `VideoLibraryPlaybackText.state` and `.location`, `VideoThumbnailImageView`
// and `VideoLibraryItemContextMenu` (local branch).
//
// The card draws the item's cached frame over its 16:9 film placeholder (the
// placeholder alone without a URL) with the favorite star, the
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
    // The page's published file: URL for this item, from the view model's map.
    property string thumbnailUrl: ""
    property bool selected: false
    property real posterWidth: Theme.videoPosterDefaultWidth

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
    // VideoThumbnailImageView.task(id:): a created card asks once.
    signal thumbnailRequested()

    width: posterWidth
    implicitHeight: artwork.height + Theme.videoPosterTitleSpacing + titleText.height
                    + Theme.videoPosterLineSpacing + locationText.height

    Component.onCompleted: {
        if (card.rowID.length > 0)
            card.thumbnailRequested()
    }

    // A recycled delegate asks for its new item (thumbnailTaskID).
    onRowIDChanged: {
        if (card.rowID.length > 0)
            card.thumbnailRequested()
    }

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

    MediaThumbnail {
        id: artwork

        width: parent.width
        height: implicitHeight
        source: card.thumbnailUrl

        // The playback scrim keeps the overlay legible on the placeholder;
        // VideoLibraryPosterArtworkView's LinearGradient runs clear to black.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * Theme.videoPosterScrimShare
            visible: card.hasPlayback
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Theme.mediaOverlayScrim }
            }
        }

        MediaCue {
            objectName: "favoriteStar"

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: Theme.videoPosterOverlayInset
            implicitWidth: Theme.videoFavoriteBadgeDiameter
            visible: card.isFavorite
            symbol: "star.fill"
            glyphSize: Theme.videoFavoriteGlyph
            glyphColor: Theme.popupWarning
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.videoPosterOverlayInset
            spacing: Theme.videoMetaSpacing
            visible: card.hasPlayback

            Row {
                anchors.right: parent.right
                spacing: Theme.spacingXs

                Icon {
                    visible: card.isFinished
                    anchors.verticalCenter: parent.verticalCenter
                    name: "checkmark"
                    size: Theme.videoPlaybackGlyph
                    color: Theme.overlayForeground
                }

                StyledLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    text: card.playbackStateText()
                    color: Theme.overlayForeground
                    textStyle: Theme.captionFont
                    weight: Theme.weightSemibold
                    monospacedDigits: true
                }
            }

            ProgressTrack {
                objectName: "progressTrack"

                readonly property real progress: card.progress

                width: parent.width
                height: Theme.videoOverlayTrackHeight
                onMedia: true
                value: card.progress
                visible: card.hasPlayback && !card.isFinished && card.progress > 0
            }
        }

        // The play glyph appears on hover, as VideoLibraryPosterCardView does.
        MediaCue {
            anchors.centerIn: parent
            implicitWidth: Theme.videoPlayCueDiameter
            visible: hover.hovered
            symbol: "play.fill"
            glyphSize: Theme.videoPlayCueGlyph
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.videoPosterSelectionOutset
            radius: Theme.videoPosterRadius + Theme.videoPosterSelectionOutset
            color: "transparent"
            border.width: Theme.videoPosterSelectionWidth
            border.color: Theme.accent
            visible: card.selected
        }
    }

    StyledLabel {
        id: titleText

        anchors.top: artwork.bottom
        anchors.topMargin: Theme.videoPosterTitleSpacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.videoPosterTextInset
        anchors.rightMargin: Theme.videoPosterTextInset
        text: card.displayTitle
        textStyle: Theme.calloutFont
        weight: Theme.weightSemibold
        wrapMode: Text.Wrap
        maximumLineCount: 2
    }

    Row {
        id: locationText

        anchors.top: titleText.bottom
        anchors.topMargin: Theme.videoPosterLineSpacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.videoPosterTextInset
        anchors.rightMargin: Theme.videoPosterTextInset
        spacing: Theme.videoMetaSpacing
        clip: true

        StyledLabel {
            text: card.rowData ? card.rowData.parentFolder : ""
            textStyle: Theme.captionFont
            secondary: true
            elide: Text.ElideRight
        }

        StyledLabel {
            text: "\u00b7"
            textStyle: Theme.captionFont
            tertiary: true
        }

        StyledLabel {
            text: card.rowData ? card.sizeText(card.rowData.fileSize) : ""
            textStyle: Theme.captionFont
            secondary: true
        }
    }
}
