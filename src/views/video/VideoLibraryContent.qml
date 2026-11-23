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
    property var collapsedSections: ({})

    signal openRequested(var row, bool fromBeginning)
    signal markWatchedRequested(var row)
    signal clearProgressRequested(var row)
    signal revealRequested(var row)
    signal deleteCollectionRequested(string collectionId)
    signal addFolderRequested()

    readonly property bool hasSections: model && model.sections.length > 0

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
    function sectionHeaderVisible(mode, section) {
        if (content.usesCollapsibleSections(mode))
            return true
        var sections = content.model.sections
        return !(sections.length === 1 && section.id === content.duplicateHeaderID(mode))
    }

    function sectionCollapsed(sectionId) {
        return content.collapsedSections[sectionId] === true
    }

    function toggleSection(sectionId) {
        var next = Object.assign({}, content.collapsedSections)
        if (next[sectionId] === true)
            delete next[sectionId]
        else
            next[sectionId] = true
        content.collapsedSections = next
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

    Flickable {
        id: listContent

        anchors.fill: parent
        visible: content.hasSections && content.model.layoutMode === "list"
        contentHeight: listSections.implicitHeight + 28
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: listSections

            x: 10
            width: listContent.width - 20
            topPadding: 4
            bottomPadding: 24
            spacing: 2

            Repeater {
                model: content.model ? content.model.sections : []

                delegate: Column {
                    id: listBlock

                    required property var modelData

                    width: listSections.width
                    spacing: 2

                    VideoLibrarySectionHeader {
                        objectName: "sectionHeader"
                        width: listBlock.width
                        visible: content.sectionHeaderVisible(content.model.displayMode,
                                                              listBlock.modelData)
                        title: listBlock.modelData.title
                        count: listBlock.modelData.rows.length
                        collapsible: content.usesCollapsibleSections(content.model.displayMode)
                        expanded: !content.sectionCollapsed(listBlock.modelData.id)
                        hasCollection: listBlock.modelData.collectionId.length > 0
                        onClicked: content.toggleSection(listBlock.modelData.id)
                        onDeleteCollectionRequested: content.deleteCollectionRequested(
                                                         listBlock.modelData.collectionId)
                    }

                    Repeater {
                        model: content.usesCollapsibleSections(content.model.displayMode)
                               && content.sectionCollapsed(listBlock.modelData.id)
                               ? [] : listBlock.modelData.rows

                        delegate: Column {
                            id: rowBlock

                            required property var modelData
                            required property int index

                            width: listBlock.width
                            spacing: 0

                            VideoLibraryListRow {
                                objectName: "videoRow"
                                width: parent.width
                                rowData: rowBlock.modelData
                                selected: content.model.selectedItemID === rowBlock.modelData.id
                                onClicked: content.openRequested(rowBlock.modelData, false)
                                onPlayFromBeginningRequested: content.openRequested(
                                                                 rowBlock.modelData, true)
                                onMarkWatchedRequested: content.markWatchedRequested(
                                                            rowBlock.modelData)
                                onClearProgressRequested: content.clearProgressRequested(
                                                              rowBlock.modelData)
                                onRevealRequested: content.revealRequested(rowBlock.modelData)
                            }

                            // Swift List's row separator; none after the last
                            // row of a section.
                            Rectangle {
                                x: 12
                                width: parent.width - 24
                                height: 1
                                color: Theme.separator
                                visible: rowBlock.index < listBlock.modelData.rows.length - 1
                            }
                        }
                    }
                }
            }
        }
    }

    Flickable {
        id: posterContent

        anchors.fill: parent
        visible: content.hasSections && content.model.layoutMode !== "list"
        contentHeight: posterSections.implicitHeight + 28
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: posterSections

            x: 16
            width: posterContent.width - 32
            topPadding: 4
            bottomPadding: 24
            spacing: 30

            Repeater {
                model: content.model ? content.model.sections : []

                delegate: Column {
                    id: posterBlock

                    required property var modelData

                    width: posterSections.width
                    spacing: 12

                    VideoLibrarySectionHeader {
                        objectName: "sectionHeader"
                        width: posterBlock.width
                        visible: content.sectionHeaderVisible(content.model.displayMode,
                                                              posterBlock.modelData)
                        title: posterBlock.modelData.title
                        count: posterBlock.modelData.rows.length
                        collapsible: content.usesCollapsibleSections(content.model.displayMode)
                        expanded: !content.sectionCollapsed(posterBlock.modelData.id)
                        hasCollection: posterBlock.modelData.collectionId.length > 0
                        onClicked: content.toggleSection(posterBlock.modelData.id)
                        onDeleteCollectionRequested: content.deleteCollectionRequested(
                                                         posterBlock.modelData.collectionId)
                    }

                    Grid {
                        id: cardGrid

                        readonly property real cellWidth: Math.min(
                            content.model.posterWidth * 1.5,
                            Math.max(content.model.posterWidth,
                                     (width - (columns - 1) * 20) / Math.max(1, columns)))

                        width: posterBlock.width
                        columns: Math.max(1, Math.floor((width + 20)
                                                        / (content.model.posterWidth + 20)))
                        columnSpacing: 20
                        rowSpacing: 26

                        Repeater {
                            model: content.usesCollapsibleSections(content.model.displayMode)
                                   && content.sectionCollapsed(posterBlock.modelData.id)
                                   ? [] : posterBlock.modelData.rows

                            delegate: VideoLibraryPosterCard {
                                required property var modelData

                                objectName: "videoCard"
                                posterWidth: cardGrid.cellWidth
                                rowData: modelData
                                selected: content.model.selectedItemID === modelData.id
                                onClicked: content.openRequested(modelData, false)
                                onPlayFromBeginningRequested: content.openRequested(modelData, true)
                                onMarkWatchedRequested: content.markWatchedRequested(modelData)
                                onClearProgressRequested: content.clearProgressRequested(modelData)
                                onRevealRequested: content.revealRequested(modelData)
                            }
                        }
                    }
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
