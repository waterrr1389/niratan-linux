// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video library's list, poster grid and empty states, following
// Features/Video/VideoLibraryView.swift `libraryContent`, `libraryListRow`,
// `sectionExpansionBinding`, `shouldHideSingleSectionHeader`,
// `duplicateSectionHeaderID`, `VideoLibraryPosterGridView.body`,
// `VideoLibraryPosterSectionHeader`, `VideoLibraryCollapsibleSectionHeader`,
// `VideoLibraryPosterCardView`, `VideoLibraryRowView`,
// `VideoThumbnailPlaceholderView` and `VideoLibraryViewModel.emptyTitleKey`
// and `emptyDescriptionKey`.
//
// No sources draws the film-stack state with the Add Video Folder action; an
// empty mode draws its own symbol, title and description. Otherwise the mode's
// sections are listed as rows or posted as 16:9 cards. Series, Folders and
// Collections sections collapse from their header; a single generated section
// hides its header.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: content

    property var model
    property var sections: []
    property var collapsedSections: ({})

    signal openRequested(var row, bool fromBeginning)
    signal markWatchedRequested(var row)
    signal clearProgressRequested(var row)
    signal revealRequested(var row)
    signal deleteCollectionRequested(string collectionId)
    signal addFolderRequested()

    readonly property bool hasSections: sections.length > 0
    readonly property bool listMode: model && model.layoutMode === "list"
    readonly property string mode: model ? model.displayMode : ""
    property var contextRow: null

    function usesCollapsibleSections(mode) {
        return mode === "series" || mode === "folders" || mode === "collections"
    }

    // VideoLibraryView.duplicateSectionHeaderID.
    function duplicateHeaderID(mode) {
        switch (mode) {
        case "continueWatching": return "continue"
        case "favorites": return "favorites"
        case "unwatched": return "unwatched"
        case "finished": return "finished"
        case "missing": return "missing"
        case "recent": return "recent"
        case "all": return "all"
        case "needsReview": return "needs-review"
        default: return ""
        }
    }

    // shouldHideSingleSectionHeader: one generated section draws no header.
    function sectionHeaderVisible(section) {
        return usesCollapsibleSections(mode)
               || !(sections.length === 1 && section.id === duplicateHeaderID(mode))
    }

    function sectionCollapsed(sectionId) {
        return content.collapsedSections[sectionId] === true
    }

    function toggleSection(sectionId, index) {
        var next = Object.assign({}, content.collapsedSections)
        if (next[sectionId] === true)
            delete next[sectionId]
        else
            next[sectionId] = true
        content.collapsedSections = next
        // The section keeps its index when only its own rows change.
        libraryList.positionViewAtIndex(index, ListView.Contain)
    }

    // A single viewport owns the scroll extent. Each poster delegate covers
    // one visual grid row, so even a section with hundreds of cards stays lazy.
    function entries(columns) {
        const result = []
        const collapsible = usesCollapsibleSections(mode)
        for (const section of sections) {
            const firstGap = result.length === 0 ? 0 : listMode ? 2 : 30
            const showHeader = sectionHeaderVisible(section)
            const expanded = !collapsible || !sectionCollapsed(section.id)
            if (showHeader)
                result.push({ section: section, gap: firstGap, expanded: expanded })
            if (!expanded)
                continue
            const rows = section.rows
            for (let i = 0; i < rows.length; i += columns) {
                const gap = i === 0 ? (showHeader ? (listMode ? 2 : 12) : firstGap)
                                    : (listMode ? 2 : 26)
                if (listMode)
                    result.push({ row: rows[i], gap: gap, separator: i + 1 < rows.length })
                else
                    result.push({ cards: rows.slice(i, i + columns), gap: gap })
            }
        }
        return result
    }

    function openContextMenu(row, position) {
        contextRow = row
        contextMenuLoader.active = true
        const menu = contextMenuLoader.item as Menu
        menu.popup(content, position.x, position.y)
    }

    // VideoLibraryDisplayMode.sidebarSystemImage, for the empty state.
    function modeSymbol(mode) {
        switch (mode) {
        case "continueWatching": return "play.circle"
        case "unwatched": return "circle.dashed"
        case "finished": return "checkmark.circle"
        case "missing": return "exclamationmark.triangle"
        case "recent": return "clock"
        case "all": return "film.stack"
        case "needsReview": return "tray"
        case "favorites": return "star"
        case "series": return "rectangle.stack"
        case "collections": return "square.stack"
        default: return "folder"
        }
    }

    // No sources: ContentUnavailableView with the folder action.
    Column {
        id: noSources

        anchors.centerIn: parent
        width: parent.width - 32
        visible: content.model !== null && !content.model.hasSources
        spacing: 8

        EmptyMessage {
            width: parent.width
            symbol: "film.stack"
            title: qsTr("No Video Folders")
            message: qsTr("Add a local folder to build your video bookshelf.")
        }

        Button {
            id: addFolderButton

            objectName: "emptyAddFolderButton"
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Add Video Folder")
            icon.source: IconMap.source("folder.badge.plus")
            onClicked: content.addFolderRequested()
        }
    }

    // Sources but nothing in this mode.
    EmptyMessage {
        anchors.centerIn: parent
        width: Math.min(360, content.width - 48)
        visible: content.model !== null && content.model.hasSources
                 && !content.hasSections
        symbol: content.modeSymbol(content.model ? content.model.displayMode : "")
        title: content.model ? content.model.emptyTitle : ""
        message: content.model ? content.model.emptyDescription : ""
    }

    ListView {
        id: libraryList

        readonly property int sideMargin: content.listMode ? 10 : 16
        readonly property real rowWidth: Math.max(0, width - 2 * sideMargin)
        readonly property real posterWidth: content.model ? content.model.posterWidth : 160
        readonly property int columnCount: content.listMode ? 1
                                          : Math.max(1, Math.floor((rowWidth + 20)
                                                                  / (posterWidth + 20)))
        readonly property real cellWidth: Math.min(posterWidth * 1.5,
            Math.max(posterWidth, (rowWidth - (columnCount - 1) * 20) / columnCount))

        objectName: content.listMode ? "videoLibraryList" : "videoLibraryPosterList"
        anchors.fill: parent
        visible: content.hasSections
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        topMargin: 4
        bottomMargin: 24
        currentIndex: -1
        keyNavigationEnabled: false
        cacheBuffer: 0
        model: content.entries(columnCount)
        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            id: entry

            required property var modelData
            required property int index

            width: libraryList.width
            height: body.height + modelData.gap

            Loader {
                id: body

                x: libraryList.sideMargin
                y: entry.modelData.gap
                width: libraryList.rowWidth
                sourceComponent: entry.modelData.section ? sectionHeader
                                 : entry.modelData.row ? listRow : posterRow
            }

            Component {
                id: sectionHeader

                VideoLibrarySectionHeader {
                    objectName: "sectionHeader"
                    title: entry.modelData.section.title
                    count: entry.modelData.section.rows.length
                    collapsible: content.usesCollapsibleSections(content.mode)
                    expanded: entry.modelData.expanded
                    hasCollection: entry.modelData.section.collectionId.length > 0
                    onClicked: content.toggleSection(entry.modelData.section.id, entry.index)
                    onDeleteCollectionRequested: content.deleteCollectionRequested(
                                                     entry.modelData.section.collectionId)
                }
            }

            Component {
                id: listRow

                Column {
                    VideoLibraryListRow {
                        id: videoRow

                        objectName: "videoRow"
                        width: parent.width
                        rowData: entry.modelData.row
                        selected: content.model.selectedItemID === rowData.id
                        onClicked: content.openRequested(rowData, false)
                        onContextMenuRequested: (position) => content.openContextMenu(
                            rowData, videoRow.mapToItem(content, position.x, position.y))
                    }

                    // Swift List's separator is omitted after a section's last row.
                    Rectangle {
                        x: 12
                        width: parent.width - 24
                        height: 1
                        color: Theme.separator
                        visible: entry.modelData.separator
                    }
                }
            }

            Component {
                id: posterRow

                Row {
                    spacing: 20

                    Repeater {
                        model: entry.modelData.cards

                        delegate: VideoLibraryPosterCard {
                            id: videoCard

                            required property var modelData

                            objectName: "videoCard"
                            posterWidth: libraryList.cellWidth
                            rowData: modelData
                            selected: content.model.selectedItemID === rowData.id
                            onClicked: content.openRequested(rowData, false)
                            onContextMenuRequested: (position) => content.openContextMenu(
                                rowData, videoCard.mapToItem(content, position.x, position.y))
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: contextMenuLoader

        active: false
        sourceComponent: Menu {
            id: contextMenu

            objectName: "videoContextMenu"

            MenuItem {
                text: qsTr("Play from Beginning")
                icon.source: IconMap.source("backward.end")
                onTriggered: {
                    contextMenu.close()
                    content.openRequested(content.contextRow, true)
                }
            }
            MenuItem {
                text: qsTr("Mark as Watched")
                icon.source: IconMap.source("checkmark.circle")
                onTriggered: {
                    contextMenu.close()
                    content.markWatchedRequested(content.contextRow)
                }
            }
            MenuItem {
                text: qsTr("Clear Progress")
                icon.source: IconMap.source("xmark.circle")
                enabled: !!content.contextRow && !!content.contextRow.playback
                onTriggered: {
                    contextMenu.close()
                    content.clearProgressRequested(content.contextRow)
                }
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("Reveal in File Manager")
                icon.source: IconMap.source("folder")
                onTriggered: {
                    contextMenu.close()
                    content.revealRequested(content.contextRow)
                }
            }
        }
    }

    // ContentUnavailableView: a large secondary glyph, a bold title and the
    // description, centred.
    component EmptyMessage: Column {
        property string symbol
        property string title
        property string message

        spacing: 8

        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            size: 40
            name: parent.symbol
            color: Theme.textSecondary
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: parent.title
            color: Theme.textPrimary
            font.pixelSize: Theme.fontTitle2
            font.weight: Font.Bold
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: parent.message
            color: Theme.textSecondary
        }
    }
}
