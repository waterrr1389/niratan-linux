// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Niratan

// The bookshelf: shelves on the left, covers on the right. Values follow
// BookView.swift (cover 160 wide by default, 110...240, aspect 0.709, 20 pt
// spacing, titles 16 pt or 13 pt under 140) and BookshelfViewModel's sorting
// and selection rules.
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
    }

    Component.onCompleted: library.refresh()

    Connections {
        target: library
        function onBookOpened(folder) {
            page.bookOpened(folder)
        }
    }

    FileDialog {
        id: importDialog
        title: qsTr("Import EPUB")
        nameFilters: [qsTr("EPUB books (*.epub)")]
        fileMode: FileDialog.OpenFiles
        onAccepted: library.importBooks(selectedFiles)
    }

    Dialog {
        id: renameDialog
        title: qsTr("Rename Book")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string folder
        TextField {
            id: renameField
            width: 280
        }
        onAboutToShow: renameField.text = ""
        onAccepted: library.renameBook(renameDialog.folder, renameField.text)
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
        id: shelfDialog
        title: qsTr("New Shelf")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField {
            id: shelfField
            width: 240
            placeholderText: qsTr("Shelf name")
        }
        onAboutToShow: shelfField.text = qsTr("New Shelf")
        onAccepted: {
            var created = library.createShelf(shelfField.text)
            if (created.length > 0)
                library.selectedShelf = "shelf:" + created
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

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 208
            color: Theme.cardBackground
            border.width: 1
            border.color: Theme.cardStroke

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2

                Label {
                    Layout.margins: 8
                    text: "Niratan"
                    color: Theme.textPrimary
                    font.pixelSize: 20
                    font.bold: true
                }

                Repeater {
                    model: [
                        { key: "all", label: qsTr("All Books") },
                        { key: "reading", label: qsTr("Currently Reading") },
                        { key: "unshelved", label: qsTr("Unshelved") }
                    ]
                    delegate: ItemDelegate {
                        id: smartRow
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        text: smartRow.modelData.label
                        highlighted: library.selectedShelf === smartRow.modelData.key
                        onClicked: library.selectedShelf = smartRow.modelData.key
                    }
                }

                Label {
                    Layout.margins: 8
                    text: qsTr("Shelves")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                }

                Repeater {
                    model: library.shelves
                    delegate: ItemDelegate {
                        id: shelfRow
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        text: shelfRow.modelData.name + " (" + shelfRow.modelData.count + ")"
                        highlighted: library.selectedShelf === "shelf:" + shelfRow.modelData.name
                        onClicked: library.selectedShelf = "shelf:" + shelfRow.modelData.name
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: shelfMenu.popup()
                        }
                        Menu {
                            id: shelfMenu
                            MenuItem {
                                text: qsTr("Delete Shelf")
                                onClicked: library.deleteShelf(shelfRow.modelData.name)
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Button {
                    Layout.fillWidth: true
                    text: qsTr("New Shelf")
                    onClicked: shelfDialog.open()
                }
            }
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
                        Layout.fillWidth: true
                        text: library.selectedShelf === "all" ? qsTr("All Books")
                            : library.selectedShelf === "reading" ? qsTr("Currently Reading")
                            : library.selectedShelf === "unshelved" ? qsTr("Unshelved")
                            : library.selectedShelf.replace("shelf:", "")
                        color: Theme.textPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    ComboBox {
                        model: [qsTr("Recent"), qsTr("Title"), qsTr("Manual")]
                        currentIndex: library.sortOption
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
                            var folders = page.selectedList()
                            for (var i = 0; i < folders.length; ++i)
                                library.deleteBook(folders[i])
                            page.clearSelection()
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
                                anchors.bottom: parent.bottom
                                anchors.margins: 6
                                visible: cell.modelData.finished
                                text: "\u2713"
                                color: Theme.textSecondary
                                font.pixelSize: 22
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
                            visible: cell.modelData.progress > 0

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
                                if (mouse.button === Qt.RightButton) {
                                    if (!page.selecting) {
                                        bookMenu.folder = cell.modelData.folder
                                        bookMenu.title = cell.modelData.title
                                        bookMenu.popup()
                                    }
                                    return
                                }
                                if (page.selecting)
                                    page.toggleSelection(cell.modelData.folder)
                                else
                                    library.openBook(cell.modelData.folder)
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
                                onClicked: library.markRead(bookMenu.folder)
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

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.margins: 8
                visible: library.bookCount === 0
                text: qsTr("No Books. Import an EPUB using the button above to start reading.")
                color: Theme.textSecondary
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.margins: 8
                visible: library.error.length > 0
                text: library.error
                color: Theme.textSecondary
            }
        }
    }
}
