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
// detail the rest. The detail's top row is the window toolbar: its primary
// group holds Sort (a menu of Recent, Title and Manual), Select Books, Cover
// Size (a popover slider from 110 to 240 in steps of 10), Statistics (while
// statistics are enabled) and Import EPUB. While selecting, Done leads the row and the group holds Move
// to Shelf and Delete. Below, the selection's title in title2 semibold with
// its count, then the grid: columns exactly `coverWidth` wide, 20 apart, with
// 20 between rows and 16 of side padding, aligned to the leading edge. An
// empty library shows "No Books"; an empty selection shows the empty-shelf
// copy for its kind.
//
// Out of scope: the search field (the view model does not filter by title
// yet), Google Drive rows and refresh, Z-Library, sync actions and Export.
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
    readonly property int toolbarHeight: 52
    // Raised by the window each time the reader closes; the shelf then
    // reloads the progress the reader saved.
    property int readerCloses: 0

    onReaderClosesChanged: library.refresh()

    property bool selecting: false
    property var selectedFolders: ({})
    readonly property int selectedCount: Object.keys(page.selectedFolders).length

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

        model: library
        onBulkDeleted: page.clearSelection()
    }

    Dialog {
        id: errorDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(400, page.width - 32)
        title: library.importError ? qsTr("Import Failed") : qsTr("Error")
        modal: true
        standardButtons: Dialog.Ok

        contentItem: Label {
            text: library.error
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Theme.textSecondary
        }

        onClosed: library.clearError()
    }

    LibraryShelfSidebar {
        id: shelfColumn

        width: Math.min(230, Math.max(176, page.width * 0.24))
        height: parent.height
        model: library
        title: qsTr("Bookshelf")
        toolbarInset: page.toolbarInset
    }

    Rectangle {
        x: shelfColumn.width
        width: 1
        height: parent.height
        color: Theme.separator
    }

    Item {
        id: detail

        x: shelfColumn.width + 1
        width: page.width - x
        height: page.height

        // The toolbar row.
        Item {
            id: toolbar

            width: parent.width
            height: page.toolbarHeight

            Button {
                x: 16
                anchors.verticalCenter: parent.verticalCenter
                visible: page.selecting
                text: qsTr("Done")
                font.weight: Font.DemiBold
                onClicked: page.clearSelection()
            }

            ToolbarGroup {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: !page.selecting

                ToolbarButton {
                    id: sortButton

                    symbol: "arrow.up.arrow.down"
                    help: qsTr("Sort Books")
                    onClicked: sortMenu.popup(sortButton, 0, sortButton.height + 4)
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
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: page.selecting

                ToolbarButton {
                    id: moveButton

                    symbol: "folder"
                    help: qsTr("Move to Shelf")
                    enabled: page.selectedCount > 0
                    onClicked: moveMenu.popup(moveButton, 0, moveButton.height + 4)
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

        // LibraryCoverSizeButton's popover: a small and a large photo glyph
        // around a 180 px slider.
        Popup {
            id: coverSizePopup

            parent: coverSizeButton
            x: (coverSizeButton.width - width) / 2
            y: coverSizeButton.height + 6
            // Kept inside the window, as a popover shifts away from the edge.
            margins: 8
            padding: 16

            contentItem: Row {
                spacing: 10

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "photo"
                    size: 11
                    color: Theme.textSecondary
                }
                Slider {
                    width: 180
                    anchors.verticalCenter: parent.verticalCenter
                    from: 110
                    to: 240
                    stepSize: 10
                    value: library.coverWidth
                    onMoved: library.coverWidth = Math.round(value)
                }
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "photo"
                    size: 17
                    color: Theme.textSecondary
                }
            }
        }

        GridView {
            id: grid

            readonly property int columnSpacing: 20
            readonly property int rowSpacing: 20

            anchors.top: toolbar.bottom
            anchors.bottom: parent.bottom
            width: parent.width
            visible: (library.smartCounts["all"] ?? 0) > 0
            clip: true
            leftMargin: 16
            rightMargin: 16
            topMargin: 14
            bottomMargin: 14
            cellWidth: library.coverWidth + grid.columnSpacing
            cellHeight: Math.round((library.coverWidth - 6) / 0.709) + 19 + 46 + grid.rowSpacing
            boundsBehavior: Flickable.StopAtBounds
            model: library.books

            ScrollBar.vertical: ScrollBar {}

            // LibraryShelfDetailHeader, 14 above the grid, and under it
            // LibraryShelfEmptyView, at least 280 tall, when the selection
            // has no books.
            header: Column {
                width: grid.width - 32

                Row {
                    width: parent.width
                    height: 22 + 14
                    spacing: 8

                    Text {
                        anchors.baseline: countText.baseline
                        width: Math.min(implicitWidth, parent.width - countText.width - 8)
                        text: page.selectionTitle
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontTitle2
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        id: countText

                        y: 4
                        text: library.bookCount
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontTitle
                        font.features: { "tnum": 1 }
                    }
                }

                Item {
                    width: parent.width
                    height: 280
                    visible: library.bookCount === 0

                    EmptyMessage {
                        anchors.centerIn: parent
                        width: Math.min(360, parent.width - 16)
                        symbol: library.selectedShelf === "reading" ? "book"
                            : library.selectedShelf === "unshelved" ? "tray" : "folder"
                        title: qsTr("This Shelf Is Empty")
                        message: library.selectedShelf === "reading"
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

                LibraryBookCard {
                    coverWidth: library.coverWidth
                    title: cell.modelData.title
                    cover: cell.modelData.cover
                    progress: cell.modelData.progress
                    percent: cell.modelData.percent
                    finished: cell.modelData.finished
                    chosen: page.selecting && page.selectedFolders[cell.modelData.folder] === true
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

        EmptyMessage {
            anchors.centerIn: grid
            width: Math.min(360, detail.width - 48)
            visible: !grid.visible
            symbol: "books.vertical"
            title: qsTr("No Books")
            message: qsTr("Import an EPUB using the toolbar button to start reading.")
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
