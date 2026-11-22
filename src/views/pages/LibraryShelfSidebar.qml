// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bookshelf's inner shelf column, following
// Features/Bookshelf/LibraryShelfSidebar.swift `LibraryShelfSidebar` and
// NativeMac/NativeReuseViews.swift `shelfSmartRows`: All Books, Currently
// Reading and Unshelved with their count badges, then the "Shelves" section
// with each shelf and its count, and a borderless New Shelf button along the
// bottom. The window toolbar's title row sits above the list, carrying the
// section title, since the column reaches the top of the window as Mac
// Niratan's inner columns do (Pictures/video.png, setting.png).
//
// A shelf row's context menu offers Rename and Delete Shelf; deleting asks
// first and keeps the books in the library. Swift creates and renames a
// shelf inline; here both go through a small name dialog. Google Drive's row
// is out of scope.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan
import "../icons/icon_map.js" as IconMap

Rectangle {
    id: column

    required property LibraryViewModel model
    property string title
    property real toolbarInset: 0
    readonly property int toolbarHeight: 52

    color: Theme.columnBackground
    activeFocusOnTab: true

    Dialog {
        id: shelfDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        title: qsTr("New Shelf")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        TextField {
            id: shelfField

            implicitWidth: 260
            placeholderText: qsTr("Shelf name")
            onAccepted: shelfDialog.accept()
        }

        onAboutToShow: shelfField.text = qsTr("New Shelf")
        onOpened: {
            shelfField.forceActiveFocus()
            shelfField.selectAll()
        }
        onAccepted: {
            const created = column.model.createShelf(shelfField.text)
            if (created.length > 0)
                column.model.selectedShelf = "shelf:" + created
        }
    }

    Dialog {
        id: renameShelfDialog

        property string fromName

        parent: Overlay.overlay
        anchors.centerIn: parent
        title: qsTr("Rename Shelf")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        TextField {
            id: renameShelfField

            implicitWidth: 260
            placeholderText: qsTr("Shelf name")
            onAccepted: renameShelfDialog.accept()
        }

        onAboutToShow: renameShelfField.text = renameShelfDialog.fromName
        onOpened: {
            renameShelfField.forceActiveFocus()
            renameShelfField.selectAll()
        }
        onAccepted: column.model.renameShelf(renameShelfDialog.fromName, renameShelfField.text)
    }

    Dialog {
        id: deleteShelfDialog

        property string shelfName

        parent: Overlay.overlay
        anchors.centerIn: parent
        title: qsTr("Delete shelf “%1”?").arg(deleteShelfDialog.shelfName)
        modal: true

        // The message wraps at a fixed 340 px inside a holder that reports
        // that width, so the dialog's size never feeds back into the text's;
        // the window is never narrower than 900.
        contentItem: Item {
            implicitWidth: 340
            implicitHeight: deleteMessage.implicitHeight

            Label {
                id: deleteMessage

                width: 340
                text: qsTr("Everything on this shelf stays in the library.")
                color: Theme.textSecondary
                wrapMode: Text.Wrap
            }
        }

        standardButtons: Dialog.Ok | Dialog.Cancel

        Component.onCompleted: deleteShelfDialog.standardButton(Dialog.Ok).text = qsTr("Delete Shelf")
        onAccepted: column.model.deleteShelf(deleteShelfDialog.shelfName)
    }

    Menu {
        id: shelfMenu

        property string shelfName

        MenuItem {
            text: qsTr("Rename")
            icon.source: IconMap.source("pencil")
            onTriggered: {
                renameShelfDialog.fromName = shelfMenu.shelfName
                renameShelfDialog.open()
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Delete Shelf")
            icon.source: IconMap.source("trash")
            onTriggered: {
                deleteShelfDialog.shelfName = shelfMenu.shelfName
                deleteShelfDialog.open()
            }
        }
    }

    Text {
        x: 16 + column.toolbarInset
        width: column.width - x - 12
        height: column.toolbarHeight
        verticalAlignment: Text.AlignVCenter
        text: column.title
        color: Theme.textPrimary
        font.pixelSize: Theme.fontTitle
        font.weight: Font.Bold
        elide: Text.ElideRight
    }

    Flickable {
        id: list

        anchors.top: parent.top
        anchors.topMargin: column.toolbarHeight
        anchors.bottom: footer.top
        width: parent.width
        contentHeight: rows.implicitHeight + 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: rows

            x: 6
            width: list.width - 12
            spacing: 2

            Repeater {
                model: [
                    { key: "all", title: qsTr("All Books"), symbol: "books.vertical" },
                    { key: "reading", title: qsTr("Currently Reading"), symbol: "book" },
                    { key: "unshelved", title: qsTr("Unshelved"), symbol: "tray" }
                ]

                delegate: SourceListRow {
                    required property var modelData

                    width: rows.width
                    symbol: modelData.symbol
                    title: modelData.title
                    badge: column.model.smartCounts[modelData.key] ?? 0
                    selected: column.model.selectedShelf === modelData.key
                    emphasized: column.activeFocus
                    onClicked: {
                        column.forceActiveFocus()
                        column.model.selectedShelf = modelData.key
                    }
                }
            }

            Text {
                leftPadding: 6
                topPadding: 14
                bottomPadding: 4
                text: qsTr("Shelves")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSubheadline
                font.weight: Font.DemiBold
            }

            Repeater {
                model: column.model.shelves

                delegate: SourceListRow {
                    required property var modelData

                    width: rows.width
                    symbol: "folder"
                    title: modelData.name
                    badge: modelData.count
                    selected: column.model.selectedShelf === "shelf:" + modelData.name
                    emphasized: column.activeFocus
                    onClicked: {
                        column.forceActiveFocus()
                        column.model.selectedShelf = "shelf:" + modelData.name
                    }
                    onContextMenuRequested: {
                        shelfMenu.shelfName = modelData.name
                        shelfMenu.popup()
                    }
                }
            }
        }
    }

    Item {
        id: footer

        anchors.bottom: parent.bottom
        width: parent.width
        height: newShelf.implicitHeight + 20

        Button {
            id: newShelf

            x: 10
            anchors.verticalCenter: parent.verticalCenter
            flat: true
            leftPadding: 8
            text: qsTr("New Shelf")
            icon.source: IconMap.source("plus")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("New Shelf")
            ToolTip.delay: 700
            onClicked: shelfDialog.open()
        }
    }
}
