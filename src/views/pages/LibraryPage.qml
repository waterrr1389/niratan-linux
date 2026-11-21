// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Niratan

// Cover geometry and badges follow BookView.swift (BookshelfLayout, ShelfCoverFrame).
Item {
    id: page
    signal bookOpened(string folder)
    signal dictionaryRequested()
    signal videoRequested()
    signal profilesRequested()
    signal statisticsRequested()
    signal settingsRequested()
    property bool selecting: false
    property var selectedFolders: ({})

    function clearSelection() {
        selecting = false
        selectedFolders = ({})
    }

    function toggleSelection(folder) {
        var next = Object.assign({}, selectedFolders)
        if (next[folder])
            delete next[folder]
        else
            next[folder] = true
        selectedFolders = next
    }

    function selectedList() {
        return Object.keys(selectedFolders)
    }

    LibraryViewModel {
        id: library
        onBookOpened: folder => page.bookOpened(folder)
    }
    Connections {
        target: library
        function onErrorChanged() {
            if (library.error.length > 0) {
                importErrorDialog.message = library.error
                importErrorDialog.open()
            }
        }
    }
    Connections {
        target: ReaderViewModel
        function onBookChanged() { library.recordBookAccess(ReaderViewModel.bookFolder) }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import EPUB")
        nameFilters: [qsTr("EPUB books (*.epub)")]
        fileMode: FileDialog.OpenFiles
        onAccepted: library.importBooks(selectedFiles)
    }

    Dialog {
        id: importErrorDialog
        property string message
        title: library.importError ? qsTr("Import Failed") : qsTr("Error")
        modal: true
        anchors.centerIn: parent
        width: Math.min(420, page.width - 32)
        standardButtons: Dialog.Ok
        contentItem: Label {
            text: importErrorDialog.message
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Theme.textPrimary
        }
        onClosed: library.clearError()
    }

    Dialog {
        id: renameDialog
        title: qsTr("Rename Book")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Save | Dialog.Cancel
        property string folder
        property string currentTitle
        TextField {
            id: renameField
            width: 280
            onAccepted: renameDialog.accept()
        }
        onAboutToShow: renameField.text = renameDialog.currentTitle
        onOpened: renameField.forceActiveFocus()
        onAccepted: library.renameBook(renameDialog.folder, renameField.text)
    }

    Dialog {
        id: markReadDialog
        property string folder
        title: qsTr("Mark as Read")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            text: qsTr("This will update your reading progress to the end of the book.")
            color: Theme.textPrimary
        }
        onAccepted: library.markRead(markReadDialog.folder)
    }

    Dialog {
        id: deleteDialog
        title: qsTr("Delete Book")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        property string folder
        property string bookTitle
        Label {
            text: qsTr("Delete \"%1\"? This cannot be undone.").arg(deleteDialog.bookTitle)
            color: Theme.textPrimary
        }
        onAccepted: library.deleteBook(deleteDialog.folder)
    }

    Dialog {
        id: bulkDeleteDialog
        property var folders: []
        title: qsTr("Delete Books")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            text: qsTr("Delete %n book(s)? This cannot be undone.", "", bulkDeleteDialog.folders.length)
            color: Theme.textPrimary
        }
        onAccepted: {
            for (const folder of bulkDeleteDialog.folders)
                library.deleteBook(folder)
            page.clearSelection()
        }
    }

    Dialog {
        id: moveDialog
        title: qsTr("Move to Shelf")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel
        property var folders: []
        ColumnLayout {
            spacing: 4
            Repeater {
                model: [qsTr("None")].concat(library.shelves.map(function (s) { return s.name }))
                delegate: Button {
                    id: shelfButton
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData
                    onClicked: {
                        var target = index === 0 ? "" : modelData
                        library.moveBooksToShelf(moveDialog.folders, target)
                        moveDialog.close()
                        page.clearSelection()
                    }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        LibraryShelfSidebar {
            Layout.fillHeight: true
            Layout.preferredWidth: 208
            model: library
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: Theme.cardBackground
                border.width: 1
                border.color: Theme.cardStroke
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8
                    Label {
                        Layout.minimumWidth: 0
                        Layout.maximumWidth: implicitWidth
                        Layout.fillWidth: true
                        text: library.selectedShelf === "all" ? qsTr("All Books")
                            : library.selectedShelf === "reading" ? qsTr("Currently Reading")
                            : library.selectedShelf === "unshelved" ? qsTr("Unshelved")
                            : library.selectedShelf.replace("shelf:", "")
                        color: Theme.textPrimary
                        font.pixelSize: 16
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: qsTr("(%1)").arg(library.bookCount)
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                    Item { Layout.fillWidth: true }
                    ComboBox {
                        model: [qsTr("Recent"), qsTr("Title"), qsTr("Manual")]
                        currentIndex: library.sortOption
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Sort Books")
                        ToolTip.delay: 700
                        onActivated: library.sortOption = currentIndex
                    }
                    Slider {
                        width: 120
                        from: 110
                        to: 240
                        stepSize: 10
                        value: library.coverWidth
                        onMoved: library.coverWidth = Math.round(value)
                    }
                    Button {
                        text: page.selecting ? qsTr("Done") : qsTr("Select")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Select Books")
                        ToolTip.delay: 700
                        onClicked: {
                            if (page.selecting)
                                page.clearSelection()
                            else
                                page.selecting = true
                        }
                    }
                    Button {
                        text: qsTr("Move")
                        visible: page.selecting
                        enabled: page.selectedList().length > 0
                        onClicked: {
                            moveDialog.folders = page.selectedList()
                            moveDialog.open()
                        }
                    }
                    Button {
                        text: qsTr("Delete")
                        visible: page.selecting
                        enabled: page.selectedList().length > 0
                        onClicked: {
                            bulkDeleteDialog.folders = page.selectedList()
                            bulkDeleteDialog.open()
                        }
                    }
                    Button {
                        text: qsTr("Dictionary")
                        onClicked: page.dictionaryRequested()
                    }
                    Button {
                        text: qsTr("Video")
                        onClicked: page.videoRequested()
                    }
                    Button {
                        text: qsTr("Profiles")
                        onClicked: page.profilesRequested()
                    }
                    Button {
                        text: qsTr("Statistics")
                        onClicked: page.statisticsRequested()
                    }
                    Button {
                        text: qsTr("Settings")
                        onClicked: page.settingsRequested()
                    }
                    Button {
                        text: qsTr("Import")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Import EPUB")
                        ToolTip.delay: 700
                        onClicked: importDialog.open()
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                GridView {
                    id: grid
                    cellWidth: library.coverWidth + 40
                    cellHeight: Math.round((library.coverWidth - 6) / 0.709) + 70
                    leftMargin: 16
                    topMargin: 16
                    model: library.books
                    boundsBehavior: Flickable.StopAtBounds
                    delegate: Item {
                        id: cell
                        required property var modelData
                        width: grid.cellWidth
                        height: grid.cellHeight
                        readonly property bool chosen: page.selectedFolders[cell.modelData.folder] === true

                        Rectangle {
                            id: coverFrame
                            x: 10
                            y: 0
                            width: library.coverWidth
                            height: Math.round((library.coverWidth - 6) / 0.709)
                            radius: 7
                            color: Theme.cardBackground
                            border.width: 1
                            border.color: cell.chosen ? Theme.accent : Theme.cardStroke
                            Image {
                                anchors.fill: parent
                                anchors.margins: 3
                                source: cell.modelData.cover
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                visible: source.toString().length > 0
                            }
                            Label {
                                anchors.centerIn: parent
                                visible: cell.modelData.cover.length === 0
                                text: cell.modelData.title
                                color: Theme.textSecondary
                                width: parent.width - 16
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                y: cell.chosen ? 6 : parent.height - height - 6
                                visible: cell.chosen || cell.modelData.finished
                                text: "\u2713"
                                color: cell.chosen ? Theme.popupBackground : Theme.textSecondary
                                width: 22
                                height: width
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: cell.chosen ? Theme.fontBody : Theme.fontLargeTitle
                                background: Rectangle {
                                    visible: cell.chosen
                                    radius: width / 2
                                    color: Theme.accent
                                }
                            }
                        }

                        Label {
                            x: 10
                            y: coverFrame.height + 6
                            width: library.coverWidth
                            text: cell.modelData.title
                            color: Theme.textPrimary
                            font.pixelSize: library.coverWidth < 140 ? 13 : 16
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            x: 10
                            y: coverFrame.height + 46
                            width: library.coverWidth
                            spacing: 4
                            Item {
                                Layout.fillWidth: true
                                height: 3
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 1.5
                                    color: Theme.cardStroke
                                }
                                Rectangle {
                                    width: parent.width * Math.min(cell.modelData.progress, 1)
                                    height: parent.height
                                    radius: 1.5
                                    color: Theme.accent
                                }
                            }
                            Label {
                                text: cell.modelData.percent + "%"
                                color: Theme.textSecondary
                                font.pixelSize: 9
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function (mouse) {
                                if (mouse.button === Qt.LeftButton) {
                                    if (page.selecting)
                                        page.toggleSelection(cell.modelData.folder)
                                    else
                                        library.openBook(cell.modelData.folder)
                                } else if (!page.selecting) {
                                    bookMenu.folder = cell.modelData.folder
                                    bookMenu.bookTitle = cell.modelData.title
                                    bookMenu.popup()
                                }
                            }
                        }

                        Menu {
                            id: bookMenu
                            property string folder
                            property string bookTitle
                            MenuItem {
                                text: qsTr("Open")
                                onClicked: library.openBook(bookMenu.folder)
                            }
                            MenuItem {
                                text: qsTr("Rename")
                                onClicked: {
                                    renameDialog.folder = bookMenu.folder
                                    renameDialog.currentTitle = bookMenu.bookTitle
                                    renameDialog.open()
                                }
                            }
                            Menu {
                                title: qsTr("Move to Shelf")
                                MenuItem {
                                    text: qsTr("None")
                                    onClicked: library.moveBookToShelf(bookMenu.folder, "")
                                }
                                Repeater {
                                    model: library.shelves
                                    delegate: MenuItem {
                                        id: moveItem
                                        required property var modelData
                                        text: moveItem.modelData.name
                                        onClicked: library.moveBookToShelf(bookMenu.folder,
                                                                           moveItem.modelData.name)
                                    }
                                }
                            }
                            MenuItem {
                                text: qsTr("Mark Read")
                                enabled: library.isLocalBook(bookMenu.folder)
                                onClicked: {
                                    markReadDialog.folder = bookMenu.folder
                                    markReadDialog.open()
                                }
                            }
                            MenuSeparator {}
                            MenuItem {
                                text: qsTr("Delete")
                                onClicked: {
                                    deleteDialog.folder = bookMenu.folder
                                    deleteDialog.bookTitle = bookMenu.bookTitle
                                    deleteDialog.open()
                                }
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                id: emptyShelf
                readonly property bool named: library.selectedShelf.startsWith("shelf:")
                Layout.alignment: Qt.AlignHCenter
                Layout.margins: 8
                visible: library.bookCount === 0
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: emptyShelf.named ? qsTr("This Shelf Is Empty")
                        : qsTr("No Books. Import an EPUB using the button above to start reading.")
                    color: emptyShelf.named ? Theme.textPrimary : Theme.textSecondary
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    visible: emptyShelf.named
                    text: qsTr("Right-click a book and choose Move to Shelf, or use Select.")
                    color: Theme.textSecondary
                }
            }
        }
    }
}
