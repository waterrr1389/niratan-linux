// SPDX-License-Identifier: GPL-3.0-or-later
//
// One video library list row, following
// Features/Video/VideoLibraryView.swift `VideoLibraryRowView`,
// `VideoLibraryPlaybackText.state` and `.location`,
// `VideoLibraryProgressTrack`, `VideoThumbnailPlaceholderView` and
// `VideoLibraryItemContextMenu` (local branch).
//
// The row draws the item's cached frame over its 16:9 film placeholder (the
// placeholder alone without a URL), a favorite star, the display title
// on one line with middle elision, the metadata line (source, parent folder,
// file size and modified date, with the folder and date dropped first when
// narrow), and the trailing playback text: "Watched" with a check, otherwise
// the remaining time, otherwise the position. An in-progress item gets a
// progress track over the artwork. Clicking opens; the context menu holds
// Play from Beginning, Mark as Watched, Clear Progress (disabled without a
// playback state) and Reveal in File Manager. There is no Details item.
pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: row

    property var rowData
    // The page's published file: URL for this item, from the view model's map.
    property string thumbnailUrl: ""
    property bool selected: false

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
    // VideoThumbnailImageView.task(id:): a created row asks once.
    signal thumbnailRequested()

    implicitHeight: Math.max(artwork.height, textColumn.implicitHeight) + Theme.videoRowInset

    Component.onCompleted: {
        if (row.rowID.length > 0)
            row.thumbnailRequested()
    }

    // A recycled ListView delegate asks for its new item (thumbnailTaskID).
    onRowIDChanged: {
        if (row.rowID.length > 0)
            row.thumbnailRequested()
    }

    function pad(value) {
        return value < 10 ? "0" + value : "" + value
    }

    // VideoTimeFormatter.string(from:): a whole number of seconds, hours only
    // when the duration reaches one.
    function timeText(seconds) {
        var total = Math.max(0, Math.floor(seconds))
        var hours = Math.floor(total / 3600)
        var minutes = Math.floor((total % 3600) / 60)
        var secs = total % 60
        return hours > 0 ? hours + ":" + pad(minutes) + ":" + pad(secs)
                         : minutes + ":" + pad(secs)
    }

    // ByteCountFormatter with the file style: bytes below 1000, then
    // kilobyte steps on a decimal scale.
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

    function modifiedText() {
        if (!rowData || !rowData.modifiedAt)
            return ""
        var date = new Date(rowData.modifiedAt)
        return isNaN(date.getTime()) ? "" : Qt.formatDate(date, Qt.locale(), Locale.ShortFormat)
    }

    function parentPath() {
        if (!rowData)
            return ""
        var path = rowData.path
        return path.substring(0, path.lastIndexOf("/"))
    }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: Theme.videoRowInset
        anchors.rightMargin: Theme.videoRowInset
        radius: Theme.videoRowSelectionRadius
        color: Theme.accent
        opacity: row.selected ? Theme.videoRowSelectionOpacity : 0
    }

    TapHandler {
        onTapped: row.clicked()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (eventPoint) => row.contextMenuRequested(eventPoint.position)
    }

    MediaThumbnail {
        id: artwork

        x: Theme.videoRowLeading
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.videoRowThumbnailWidth
        height: implicitHeight
        radius: Theme.videoRowThumbnailRadius
        source: row.thumbnailUrl

        ProgressTrack {
            objectName: "progressTrack"

            readonly property real progress: row.progress

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Theme.videoRowInset
            height: Theme.videoRowTrackHeight
            value: row.progress
            visible: row.hasPlayback && !row.isFinished && row.progress > 0
        }
    }

    Column {
        id: textColumn

        anchors.left: artwork.right
        anchors.leftMargin: Theme.videoRowTextSpacing
        anchors.right: stateText.left
        anchors.rightMargin: Theme.videoRowTrailing
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacingXs

        Row {
            width: parent.width
            spacing: Theme.videoMetaSpacing

            Icon {
                id: favoriteStar

                objectName: "favoriteStar"

                visible: row.isFavorite
                anchors.verticalCenter: parent.verticalCenter
                name: "star.fill"
                size: Theme.videoFavoriteGlyph
                color: Theme.popupWarning
            }

            StyledLabel {
                width: parent.width - (row.isFavorite ? favoriteStar.width + parent.spacing : 0)
                text: row.displayTitle
                textStyle: Theme.calloutFont
                weight: Theme.weightMedium
                elide: Text.ElideMiddle
            }
        }

        // ViewThatFits(in: .horizontal): the source and the size stay, the
        // folder and date drop first when the row is narrow.
        Row {
            width: parent.width
            spacing: Theme.videoMetaSpacing
            clip: true

            StyledLabel {
                text: row.rowData ? row.rowData.sourceName : ""
                textStyle: Theme.captionFont
                secondary: true
                elide: Text.ElideRight
            }
            StyledLabel {
                visible: row.width >= Theme.videoRowDetailWidth && row.rowData
                         && row.rowData.parentFolder.length > 0
                text: "\u00b7"
                textStyle: Theme.captionFont
                tertiary: true
            }
            StyledLabel {
                visible: row.width >= Theme.videoRowDetailWidth && row.rowData
                         && row.rowData.parentFolder.length > 0
                text: row.rowData ? row.rowData.parentFolder : ""
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
                text: row.rowData ? row.sizeText(row.rowData.fileSize) : ""
                textStyle: Theme.captionFont
                secondary: true
            }
            StyledLabel {
                visible: row.width >= Theme.videoRowDetailWidth && row.modifiedText().length > 0
                text: "\u00b7"
                textStyle: Theme.captionFont
                tertiary: true
            }
            StyledLabel {
                visible: row.width >= Theme.videoRowDetailWidth && row.modifiedText().length > 0
                text: row.modifiedText()
                textStyle: Theme.captionFont
                secondary: true
            }
        }
    }

    Row {
        id: stateText

        anchors.right: parent.right
        anchors.rightMargin: Theme.videoRowTrailing
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacingXs

        Icon {
            visible: row.hasPlayback && row.isFinished
            anchors.verticalCenter: parent.verticalCenter
            name: "checkmark.circle.fill"
            size: Theme.videoRowStateGlyph
            color: Theme.textSecondary
        }

        StyledLabel {
            text: row.playbackStateText()
            color: row.isFinished ? Theme.textSecondary : Theme.accent
            textStyle: Theme.captionFont
            weight: Theme.weightMedium
            monospacedDigits: true
        }
    }
}
