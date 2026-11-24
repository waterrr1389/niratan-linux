// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video library's mode column, following
// Features/Video/VideoLibraryView.swift `VideoLibrarySidebarView.body`,
// `sidebarSelection`, `sidebarRow`, `VideoLibraryDisplayMode.libraryModes`,
// `.organizationModes`, `.sidebarSystemImage`, `.titleKey` and
// `VideoLibraryViewModel.modeCounts`.
//
// The Library section lists Continue Watching, Recent, All Videos, Unwatched,
// Finished and Favorites, then Missing only while its count is above zero or
// it is the current mode. The Organization section lists Series, Folders,
// Collections and Needs Review. Each row draws its SF Symbol through Icon,
// its title and its count as a badge only above zero. Selecting a row calls
// `model.displayMode = mode`.
//
// Out of scope: the Media Servers section and Add Media Server.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

Rectangle {
    id: sidebar

    property var model
    property real toolbarInset: 0
    readonly property int toolbarHeight: Theme.sourceListToolbarHeight
    readonly property var counts: model ? model.modeCounts : ({})

    // VideoLibraryDisplayMode.libraryModes, .organizationModes and
    // .sidebarSystemImage, in Swift's order.
    readonly property var libraryModes: [
        { mode: "continueWatching", title: qsTr("Continue Watching"), symbol: "play.circle" },
        { mode: "recent", title: qsTr("Recent"), symbol: "clock" },
        { mode: "all", title: qsTr("All Videos"), symbol: "film.stack" },
        { mode: "unwatched", title: qsTr("Unwatched"), symbol: "circle.dashed" },
        { mode: "finished", title: qsTr("Finished"), symbol: "checkmark.circle" },
        { mode: "favorites", title: qsTr("Favorites"), symbol: "star" }
    ]
    readonly property var organizationModes: [
        { mode: "series", title: qsTr("Series"), symbol: "rectangle.stack" },
        { mode: "folders", title: qsTr("Folders"), symbol: "folder" },
        { mode: "collections", title: qsTr("Collections"), symbol: "square.stack" },
        { mode: "needsReview", title: qsTr("Needs Review"), symbol: "tray" }
    ]

    color: Theme.columnBackground
    activeFocusOnTab: true

    SourceListTitle {
        leadingInset: sidebar.toolbarInset
        text: qsTr("Video Library")
    }

    Flickable {
        id: list

        anchors.top: parent.top
        anchors.topMargin: sidebar.toolbarHeight
        anchors.bottom: parent.bottom
        width: parent.width
        contentHeight: rows.implicitHeight + Theme.spacingSm
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: rows

            x: Theme.sourceListInset
            width: list.width - 2 * Theme.sourceListInset
            spacing: Theme.sourceListRowSpacing

            SourceListCaption {
                first: true
                text: qsTr("Library")
            }

            Repeater {
                model: sidebar.libraryModes

                delegate: SourceListRow {
                    required property var modelData

                    property string mode: modelData.mode

                    objectName: "modeRow-" + modelData.mode
                    badgeObjectName: "modeBadge-" + modelData.mode
                    width: rows.width
                    symbol: modelData.symbol
                    title: modelData.title
                    badge: sidebar.counts[modelData.mode] ?? 0
                    selected: sidebar.model.displayMode === modelData.mode
                    emphasized: true
                    onClicked: sidebar.model.displayMode = modelData.mode
                }
            }

            SourceListRow {
                objectName: "modeRow-missing"
                badgeObjectName: "modeBadge-missing"
                property string mode: "missing"

                width: rows.width
                visible: (sidebar.counts["missing"] ?? 0) > 0
                         || sidebar.model.displayMode === "missing"
                symbol: "exclamationmark.triangle"
                title: qsTr("Missing")
                badge: sidebar.counts["missing"] ?? 0
                selected: sidebar.model.displayMode === "missing"
                emphasized: true
                onClicked: sidebar.model.displayMode = "missing"
            }

            SourceListCaption {
                first: false
                text: qsTr("Organization")
            }

            Repeater {
                model: sidebar.organizationModes

                delegate: SourceListRow {
                    required property var modelData

                    property string mode: modelData.mode

                    objectName: "modeRow-" + modelData.mode
                    badgeObjectName: "modeBadge-" + modelData.mode
                    width: rows.width
                    symbol: modelData.symbol
                    title: modelData.title
                    badge: sidebar.counts[modelData.mode] ?? 0
                    selected: sidebar.model.displayMode === modelData.mode
                    emphasized: true
                    onClicked: sidebar.model.displayMode = modelData.mode
                }
            }
        }
    }
}
