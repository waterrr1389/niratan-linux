// SPDX-License-Identifier: GPL-3.0-or-later
//
// One video library list row, following
// Features/Video/VideoLibraryView.swift `VideoLibraryRowView`,
// `VideoLibraryPlaybackText.state` and `.location`,
// `VideoLibraryProgressTrack`, `VideoThumbnailPlaceholderView` and
// `VideoLibraryItemContextMenu` (local branch).
//
// The row draws the 16:9 film placeholder, a favorite star, the display title
// on one line with middle elision, the metadata line (source, parent folder,
// file size and modified date, with the folder and date dropped first when
// narrow), and the trailing playback text: "Watched" with a check, otherwise
// the remaining time, otherwise the position. An in-progress item gets a
// progress track over the artwork. Clicking opens; the context menu holds
// Play from Beginning, Mark as Watched, Clear Progress (disabled without a
// playback state) and Reveal in File Manager. There is no Details item.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: row

    property var rowData
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
    signal playFromBeginningRequested()
    signal markWatchedRequested()
    signal clearProgressRequested()
    signal revealRequested()


    implicitHeight: Math.max(artwork.height, textColumn.implicitHeight) + 6

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
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        radius: 10
        color: row.selected ? Theme.accent : "transparent"
        opacity: row.selected ? 0.16 : 0
    }

    TapHandler {
        onTapped: row.clicked()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: contextMenu.popup()
    }

    Item {
        id: artwork

        x: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 120
        height: 67.5

        Rectangle {
            anchors.fill: parent
            radius: 7
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
            radius: 7
            color: "transparent"
            border.width: 1
            border.color: Theme.cardStroke
        }

        Item {
            objectName: "progressTrack"

            readonly property real progress: row.progress

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 6
            height: 3
            visible: row.hasPlayback && !row.isFinished && row.progress > 0

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: Theme.shelfProgressTrack
            }

            Rectangle {
                width: parent.width * Math.min(1, Math.max(0, row.progress))
                height: parent.height
                radius: height / 2
                color: Theme.accent
            }
        }
    }

    Column {
        id: textColumn

        anchors.left: artwork.right
        anchors.leftMargin: 14
        anchors.right: stateText.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Row {
            width: parent.width
            spacing: 5

            Icon {
                objectName: "favoriteStar"

                visible: row.isFavorite
                anchors.verticalCenter: parent.verticalCenter
                name: "star.fill"
                size: 10
                color: Theme.popupWarning
            }

            Text {
                width: parent.width - (row.isFavorite ? 15 : 0)
                text: row.displayTitle
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                font.weight: Font.Medium
                elide: Text.ElideMiddle
            }
        }

        // ViewThatFits(in: .horizontal): the source and the size stay, the
        // folder and date drop first when the row is narrow.
        Row {
            width: parent.width
            spacing: 5
            clip: true

            Text {
                text: row.rowData ? row.rowData.sourceName : ""
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }

            Text {
                visible: row.width >= 520 && row.rowData
                         && row.rowData.parentFolder.length > 0
                text: "\u00b7"
                color: Theme.textTertiary
                font.pixelSize: Theme.fontCaption
            }

            Text {
                visible: row.width >= 520 && row.rowData
                         && row.rowData.parentFolder.length > 0
                text: row.rowData ? row.rowData.parentFolder : ""
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
                text: row.rowData ? row.sizeText(row.rowData.fileSize) : ""
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }

            Text {
                visible: row.width >= 520 && row.modifiedText().length > 0
                text: "\u00b7"
                color: Theme.textTertiary
                font.pixelSize: Theme.fontCaption
            }

            Text {
                visible: row.width >= 520 && row.modifiedText().length > 0
                text: row.modifiedText()
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
        }
    }

    Row {
        id: stateText

        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Icon {
            visible: row.hasPlayback && row.isFinished
            anchors.verticalCenter: parent.verticalCenter
            name: "checkmark.circle.fill"
            size: 12
            color: Theme.textSecondary
        }

        Text {
            text: row.playbackStateText()
            color: row.isFinished ? Theme.textSecondary : Theme.accent
            font.pixelSize: Theme.fontCaption
            font.weight: Font.Medium
            font.features: { "tnum": 1 }
        }
    }

    Menu {
        id: contextMenu

        MenuItem {
            text: qsTr("Play from Beginning")
            icon.source: IconMap.source("backward.end")
            onTriggered: row.playFromBeginningRequested()
        }
        MenuItem {
            text: qsTr("Mark as Watched")
            icon.source: IconMap.source("checkmark.circle")
            onTriggered: row.markWatchedRequested()
        }
        MenuItem {
            text: qsTr("Clear Progress")
            icon.source: IconMap.source("xmark.circle")
            enabled: row.hasPlayback
            onTriggered: row.clearProgressRequested()
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Reveal in File Manager")
            icon.source: IconMap.source("folder")
            onTriggered: row.revealRequested()
        }
    }
}
