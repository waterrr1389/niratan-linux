// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bookshelf section, following NativeMac/NativeReuseViews.swift
// `NativeBookshelfReuseView` (`bookshelfContent`, `shelfDetail`,
// `toolbarContent`, `shelfTitle`), Features/Bookshelf/LibraryShelfSidebar.swift
// (`LibraryShelfLayout`, `LibraryShelfDetailHeader`, `LibraryShelfEmptyView`,
// `LibraryCoverSizeButton`), Features/Bookshelf/ShelfView.swift (the adaptive
// grid) and Features/Bookshelf/BookCell.swift (the context menu, rename sheet
// and confirmations).
//
// The shelf column takes 24% of the width, clamped to 176..230, and the
// detail the rest. The detail's top row is the window toolbar: the search
// field (prompt "Search Books") at its leading edge, then the primary group
// with Sort (a menu of Recent, Title and Manual), Select Books, Cover Size (a
// popover slider from 110 to 240 in steps of 10), Statistics (while statistics
// are enabled) and Import EPUB. While selecting, Done leads the row and the
// group holds Move to Shelf and Delete; the search field hides but keeps its
// query and filter, so Done returns to them. Below, the selection's title in
// title2 semibold with its count, then the grid: columns exactly `coverWidth`
// wide, 20 apart, with 20 between rows and 16 of side padding, aligned to the
// leading edge. An empty library shows "No Books"; an empty selection shows
// the empty-shelf copy for its kind.
//
// Search follows NativeReuseViews.displayedSection: the query is trimmed and
// matched case-insensitively against the display title and the stored title,
// after the shelf's sort, so matches keep the sorted order. A filtered shelf
// allows no cover reordering, and no matches replace the shelf's empty message
// with "No Results" naming the query.
//
// BookshelfFileDropTarget wraps the shelf: a drag with at least one EPUB is
// accepted as a copy and its files are imported, while other drags are
// refused. importBooksProgress follows BookshelfViewModel.importBooks: a
// blocking overlay shows "Importing k / n..." during a multi-file batch.
//
// Out of scope: Google Drive rows and refresh, Z-Library and sync actions.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: page

    signal bookOpened(string folder)
    signal statisticsRequested()

    // Room the window asks for at the toolbar row's leading edge, where its
    // sidebar toggle floats while the sidebar is collapsed.
    property real toolbarInset: 0
    readonly property int toolbarHeight: Theme.sourceListToolbarHeight
    // Raised by the window each time the reader closes; the shelf then
    // reloads the progress the reader saved.
    property int readerCloses: 0

    onReaderClosesChanged: library.refresh()

    property bool selecting: false
    property var selectedFolders: ({})
    readonly property int selectedCount: Object.keys(page.selectedFolders).length

    // NativeReuseViews.displayedSection: the query is trimmed, an empty one
    // shows the shelf as sorted, and a match is a case-insensitive substring of
    // the display or the stored title. Filtering keeps the shelf's order.
    property string searchQuery: ""
    readonly property string trimmedQuery: searchQuery.trim()
    readonly property bool isFiltered: trimmedQuery.length > 0
    readonly property var displayedBooks: {
        if (!page.isFiltered)
            return library.books
        const query = page.trimmedQuery.toLocaleLowerCase()
        const matches = []
        for (let index = 0; index < library.books.length; ++index) {
            const book = library.books[index]
            const title = String(book.title ?? "").toLocaleLowerCase()
            const stored = String(book.storedTitle ?? "").toLocaleLowerCase()
            if (title.includes(query) || stored.includes(query))
                matches.push(book)
        }
        return matches
    }

    readonly property string selectionTitle: {
        const shelf = library.selectedShelf
        if (shelf === "all")
            return qsTr("All Books")
        if (shelf === "reading")
            return qsTr("Currently Reading")
        if (shelf === "unshelved")
            return qsTr("Unshelved")
        return shelf.replace("shelf:", "")
    }

    function clearSelection() {
        page.selecting = false
        page.selectedFolders = ({})
    }

    function toggleSelection(folder) {
        const next = Object.assign({}, page.selectedFolders)
        if (next[folder])
            delete next[folder]
        else
            next[folder] = true
        page.selectedFolders = next
    }

    LibraryViewModel {
        id: library

        onBookOpened: folder => page.bookOpened(folder)
    }

    // The global Enable Statistics setting, read when the page is created;
    // the window recreates the page each time the section is shown.
    SettingsViewModel {
        id: settings
    }

    Connections {
        target: library

        function onErrorChanged() {
            if (library.error.length > 0)
                errorDialog.open()
        }
    }

    Connections {
        target: ReaderViewModel

        function onBookChanged() {
            library.recordBookAccess(ReaderViewModel.bookFolder)
        }
    }

    FileDialog {
        id: importDialog

        title: qsTr("Import EPUB")
        nameFilters: [qsTr("EPUB books (*.epub)")]
        fileMode: FileDialog.OpenFiles
        onAccepted: library.importBooks(selectedFiles)
    }

    LibraryDialogs {
        id: dialogs

        objectName: "libraryDialogs"
        model: library
        onBulkDeleted: page.clearSelection()
    }

    AlertDialog {
        id: errorDialog

        title: library.importError ? qsTr("Import Failed") : qsTr("Error")
        message: library.error

        onClosed: library.clearError()
    }

    LibraryShelfSidebar {
        id: shelfColumn

        width: Math.min(Theme.shelfColumnMaximumWidth,
                       Math.max(Theme.shelfColumnMinimumWidth, page.width * Theme.shelfColumnShare))
        height: parent.height
        model: library
        title: qsTr("Bookshelf")
        toolbarInset: page.toolbarInset
    }

    Rectangle {
        x: shelfColumn.width
        width: Theme.hairlineWidth
        height: parent.height
        color: Theme.separator
    }

    Item {
        id: detail

        x: shelfColumn.width + Theme.hairlineWidth
        width: page.width - x
        height: page.height

        // The toolbar row.
        Item {
            id: toolbar

            width: parent.width
            height: page.toolbarHeight

            FormButton {
                x: Theme.spacingLg
                anchors.verticalCenter: parent.verticalCenter
                visible: page.selecting
                text: qsTr("Done")
                small: true
                sizeToLabel: true
                onClicked: page.clearSelection()
            }

            // .searchable(placement: .toolbar): the query stays while selecting
            // even though the field is not part of the selection toolbar.
            SearchBar {
                id: searchBar

                objectName: "librarySearchBar"
                fieldObjectName: "librarySearchField"
                x: Theme.spacingLg
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(Theme.librarySearchMaximumWidth,
                                Math.max(Theme.librarySearchMinimumWidth,
                                         toolbar.width * Theme.librarySearchShare))
                visible: !page.selecting
                placeholderText: qsTr("Search Books")
                text: page.searchQuery
                onTextChanged: {
                    if (page.searchQuery !== text)
                        page.searchQuery = text
                }
            }

            ToolbarGroup {
                anchors.right: parent.right
                anchors.rightMargin: Theme.libraryToolbarInset
                anchors.verticalCenter: parent.verticalCenter
                visible: !page.selecting

                ToolbarButton {
                    id: sortButton

                    symbol: "arrow.up.arrow.down"
                    help: qsTr("Sort Books")
                    onClicked: sortMenu.popup(sortButton, 0, sortButton.height + Theme.spacingXs)
                }
                ToolbarButton {
                    symbol: "checklist"
                    help: qsTr("Select Books")
                    onClicked: page.selecting = true
                }
                ToolbarButton {
                    id: coverSizeButton

                    symbol: "square.resize"
                    help: qsTr("Cover Size")
                    onClicked: coverSizePopup.open()
                }
                ToolbarButton {
                    visible: settings.enableStatistics
                    symbol: "chart.xyaxis.line"
                    help: qsTr("Statistics")
                    onClicked: page.statisticsRequested()
                }
                ToolbarButton {
                    symbol: "plus"
                    help: qsTr("Import EPUB")
                    onClicked: importDialog.open()
                }
            }

            ToolbarGroup {
                anchors.right: parent.right
                anchors.rightMargin: Theme.libraryToolbarInset
                anchors.verticalCenter: parent.verticalCenter
                visible: page.selecting

                ToolbarButton {
                    id: moveButton

                    symbol: "folder"
                    help: qsTr("Move to Shelf")
                    enabled: page.selectedCount > 0
                    onClicked: moveMenu.popup(moveButton, 0, moveButton.height + Theme.spacingXs)
                }
                ToolbarButton {
                    symbol: "trash"
                    help: qsTr("Delete")
                    enabled: page.selectedCount > 0
                    onClicked: dialogs.confirmBulkDelete(Object.keys(page.selectedFolders))
                }
            }
        }

        Menu {
            id: sortMenu

            Repeater {
                model: [
                    { title: qsTr("Recent"), symbol: "clock" },
                    { title: qsTr("Title"), symbol: "textformat.size.larger.ja" },
                    { title: qsTr("Manual"), symbol: "line.3.horizontal" }
                ]

                delegate: MenuItem {
                    required property int index
                    required property var modelData

                    text: modelData.title
                    icon.source: IconMap.source(modelData.symbol)
                    checkable: true
                    checked: library.sortOption === index
                    onTriggered: library.sortOption = index
                }
            }
        }

        Menu {
            id: moveMenu

            MenuItem {
                text: qsTr("None")
                icon.source: IconMap.source("tray")
                onTriggered: {
                    library.moveBooksToShelf(Object.keys(page.selectedFolders), "")
                    page.clearSelection()
                }
            }

            Repeater {
                model: library.shelves

                delegate: MenuItem {
                    required property var modelData

                    text: modelData.name
                    icon.source: IconMap.source("folder")
                    onTriggered: {
                        library.moveBooksToShelf(Object.keys(page.selectedFolders),
                                                 modelData.name)
                        page.clearSelection()
                    }
                }
            }
        }

        // LibraryCoverSizeButton's popover, the kit's CoverSizeSlider.
        Popup {
            id: coverSizePopup

            parent: coverSizeButton
            x: (coverSizeButton.width - width) / 2
            y: coverSizeButton.height + Theme.popoverOffset
            // Kept inside the window, as a popover shifts away from the edge.
            margins: Theme.spacingSm
            padding: Theme.popoverPadding

            contentItem: CoverSizeSlider {
                value: library.coverWidth
                onMoved: function (value) { library.coverWidth = value }
            }
        }

        GridView {
            id: grid

            objectName: "libraryBookGrid"
            readonly property int columnSpacing: Theme.shelfGridSpacing
            readonly property int rowSpacing: Theme.shelfGridSpacing
            property string dragSource: ""
            property string dragTarget: ""
            readonly property bool reorderEnabled: library.selectedShelf !== "reading"
                                                  && !page.isFiltered

            function clearDrag() {
                dragSource = ""
                dragTarget = ""
            }

            function bookAt(scenePosition: point, inset: real, excluded: string): string {
                const position = mapFromItem(null, scenePosition)
                if (position.x < 0 || position.x >= width
                        || position.y < 0 || position.y >= height)
                    return ""
                const pos = contentItem.mapFromItem(grid, position)
                for (let i = 0; i < count; ++i) {
                    const item = itemAtIndex(i)
                    const folder = page.displayedBooks[i] ? page.displayedBooks[i].folder : ""
                    if (item && folder !== excluded
                            && pos.x >= item.x - inset
                            && pos.x < item.x + library.coverWidth + inset
                            && pos.y >= item.y - inset
                            && pos.y < item.y + cellHeight - rowSpacing + inset)
                        return folder
                }
                return ""
            }

            function reorderAt(position: point) {
                if (!reorderEnabled || dragSource.length === 0)
                    return
                const next = bookAt(position, Theme.bookDragHitInset, dragSource)
                if (next.length > 0 && next !== dragTarget && library.reorderBook(dragSource, next))
                    dragTarget = next
            }

            anchors.top: toolbar.bottom
            anchors.bottom: parent.bottom
            width: parent.width
            visible: (library.smartCounts["all"] ?? 0) > 0
            clip: true
            leftMargin: Theme.spacingLg
            rightMargin: Theme.spacingLg
            topMargin: Theme.shelfHeaderTopInset
            bottomMargin: Theme.shelfHeaderTopInset
            cellWidth: library.coverWidth + grid.columnSpacing
            // The card: the framed cover, its progress strip and the title box.
            cellHeight: Math.round((library.coverWidth - 2 * Theme.bookFrameInset) / Theme.bookCoverAspect)
                        + Theme.bookStripHeight + 3 * Theme.bookFrameInset
                        + Theme.bookTitleSpacing + Theme.bookTitleHeight + grid.rowSpacing
            boundsBehavior: Flickable.StopAtBounds
            model: page.displayedBooks

            ScrollBar.vertical: ScrollBar {}

            Connections {
                target: library
                function onSelectionChanged() { grid.clearDrag() }
            }

            // Keep the grab on the view: changing the order replaces its delegates.
            DragHandler {
                id: bookDrag

                parent: grid
                target: null
                enabled: grid.reorderEnabled
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                dragThreshold: Theme.bookDragHitInset
                onActiveChanged: {
                    if (active) {
                        grid.dragSource = grid.bookAt(centroid.scenePressPosition, 0, "")
                        grid.dragTarget = ""
                        grid.reorderAt(centroid.scenePosition)
                    }
                }
                onCentroidChanged: {
                    if (active)
                        grid.reorderAt(centroid.scenePosition)
                }
                onGrabChanged: (transition, point) => {
                    if (transition === PointerDevice.UngrabExclusive) {
                        grid.reorderAt(point.scenePosition)
                        grid.clearDrag()
                    } else if (transition === PointerDevice.CancelGrabExclusive) {
                        grid.clearDrag()
                    }
                }
            }

            // LibraryShelfDetailHeader, 14 above the grid, and under it
            // LibraryShelfEmptyView, at least 280 tall, when the selection
            // has no books.
            header: Column {
                width: grid.width - 2 * Theme.spacingLg

                // LibraryShelfDetailHeader, styled as the video library's
                // content header: title2 semibold and the secondary count.
                Row {
                    width: parent.width
                    height: Theme.shelfHeaderHeight
                    spacing: Theme.spacingSm

                    StyledLabel {
                        anchors.baseline: countText.baseline
                        width: Math.min(implicitWidth, parent.width - countText.width - parent.spacing)
                        text: page.selectionTitle
                        textStyle: Theme.title2Font
                        weight: Theme.weightSemibold
                        elide: Text.ElideRight
                    }
                    StyledLabel {
                        id: countText

                        anchors.verticalCenter: parent.verticalCenter
                        text: page.displayedBooks.length
                        textStyle: Theme.title3Font
                        secondary: true
                        monospacedDigits: true
                    }
                }

                Item {
                    width: parent.width
                    height: Theme.shelfEmptyMinimumHeight
                    visible: page.displayedBooks.length === 0

                    // ContentUnavailableView.search(text:) while filtered;
                    // LibraryShelfEmptyView otherwise.
                    UnavailableView {
                        objectName: "libraryShelfEmptyView"
                        anchors.centerIn: parent
                        width: Math.min(Theme.unavailableWidth, parent.width - 2 * Theme.spacingSm)
                        symbol: page.isFiltered ? "magnifyingglass"
                            : library.selectedShelf === "reading" ? "book"
                            : library.selectedShelf === "unshelved" ? "tray" : "folder"
                        title: page.isFiltered ? qsTr("No Results") : qsTr("This Shelf Is Empty")
                        message: page.isFiltered
                            ? qsTr("No results for “%1”.").arg(page.trimmedQuery)
                            : library.selectedShelf === "reading"
                            ? qsTr("Titles you've started but not finished appear here.")
                            : library.selectedShelf === "unshelved"
                            ? qsTr("Everything in the library is on a shelf.")
                            : qsTr("Right-click a cover and choose Move, or use Select to move several at once.")
                    }
                }
            }

            delegate: Item {
                id: cell

                required property var modelData

                width: grid.cellWidth
                height: grid.cellHeight
                z: bookCard.dragSource ? 2 : bookCard.dragTarget ? 1 : 0

                LibraryBookCard {
                    id: bookCard

                    objectName: "libraryBookCard"
                    coverWidth: library.coverWidth
                    title: cell.modelData.title
                    cover: cell.modelData.cover
                    progress: cell.modelData.progress
                    percent: cell.modelData.percent
                    finished: cell.modelData.finished
                    chosen: page.selecting && page.selectedFolders[cell.modelData.folder] === true
                    dragSource: grid.dragSource === cell.modelData.folder
                    dragTarget: grid.dragTarget === cell.modelData.folder
                    onClicked: {
                        if (page.selecting)
                            page.toggleSelection(cell.modelData.folder)
                        else
                            library.openBook(cell.modelData.folder)
                    }
                    onContextMenuRequested: {
                        if (!page.selecting)
                            dialogs.showBookMenu(cell.modelData.folder, cell.modelData.title)
                    }
                }
            }
        }

        UnavailableView {
            anchors.centerIn: grid
            width: Math.min(Theme.unavailableWidth, detail.width - 2 * Theme.spacingXl)
            visible: !grid.visible
            symbol: "books.vertical"
            title: qsTr("No Books")
            message: qsTr("Import an EPUB using the toolbar button to start reading.")
        }
    }

    // BookshelfFileDropTarget: at least one EPUB in the drag makes it a copy;
    // otherwise the drop is refused and no import runs.
    DropArea {
        objectName: "libraryDropArea"

        anchors.fill: parent

        onEntered: (drag) => {
            if (library.acceptsDrop(drag.urls))
                drag.accept(Qt.CopyAction)
        }
        onDropped: (drop) => {
            if (library.acceptsDrop(drop.urls)) {
                drop.accept(Qt.CopyAction)
                library.importDroppedEPUBs(drop.urls)
            }
        }
    }

    // BookshelfView's LoadingOverlay(importBooksProgress).
    LoadingOverlay {
        objectName: "libraryImportOverlay"
        messageObjectName: "libraryImportProgress"
        message: library.importProgress
        active: library.importProgress.length > 0
    }
}
