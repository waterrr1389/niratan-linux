// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bookshelf's book menu and its confirmations, following
// Features/Bookshelf/BookCell.swift (the context menu: Move, Mark Read,
// Rename and Delete, with their glyphs; `BookRenameSheet`; the delete and mark-read
// confirmation dialogs) and NativeMac/NativeReuseViews.swift (the bulk delete
// alert of `bookshelfContent`). Sync and Export entries are out of scope.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: dialogs

    required property LibraryViewModel model

    signal bulkDeleted()

    property string folder
    property string bookTitle

    function showBookMenu(folder, title) {
        dialogs.folder = folder
        dialogs.bookTitle = title
        bookMenu.popup()
    }

    function confirmBulkDelete(folders) {
        bulkDeleteDialog.folders = folders
        bulkDeleteDialog.open()
    }

    Menu {
        id: bookMenu

        Menu {
            title: qsTr("Move")
            icon.source: IconMap.source("folder")

            MenuItem {
                text: qsTr("None")
                icon.source: IconMap.source("tray")
                onTriggered: dialogs.model.moveBookToShelf(dialogs.folder, "")
            }

            Repeater {
                model: dialogs.model.shelves

                delegate: MenuItem {
                    required property var modelData

                    text: modelData.name
                    icon.source: IconMap.source("folder")
                    onTriggered: dialogs.model.moveBookToShelf(dialogs.folder, modelData.name)
                }
            }
        }
        MenuItem {
            text: qsTr("Mark Read")
            icon.source: IconMap.source("checkmark")
            enabled: dialogs.model.isLocalBook(dialogs.folder)
            onTriggered: markReadDialog.open()
        }
        MenuItem {
            text: qsTr("Rename")
            icon.source: IconMap.source("character.cursor.ibeam.ja")
            onTriggered: renameDialog.open()
        }
        MenuItem {
            text: qsTr("Delete")
            icon.source: IconMap.source("trash")
            onTriggered: deleteDialog.open()
        }
    }

    // BookRenameSheet: a title2 bold heading, the title field and Cancel and
    // Save, 420 wide with 24 of padding.
    Dialog {
        id: renameDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent ? parent.width - 32 : 420)
        modal: true
        padding: 24
        topPadding: 24

        header: null

        contentItem: Column {
            spacing: 18

            Label {
                text: qsTr("Rename")
                font.pixelSize: Theme.fontTitle2
                font.weight: Font.Bold
            }
            TextField {
                id: renameField

                width: parent.width
                placeholderText: qsTr("Title")
                onAccepted: renameDialog.accept()
            }
        }

        footer: DialogButtonBox {
            topPadding: 0
            padding: 24

            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                text: qsTr("Save")
                prominent: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onAboutToShow: renameField.text = dialogs.bookTitle
        onOpened: {
            renameField.forceActiveFocus()
            renameField.selectAll()
        }
        onAccepted: dialogs.model.renameBook(dialogs.folder, renameField.text.trim())
    }

    ConfirmDialog {
        id: deleteDialog

        title: qsTr("Delete \"%1\"?").arg(dialogs.bookTitle)
        confirmText: qsTr("Delete")
        onAccepted: dialogs.model.deleteBook(dialogs.folder)
    }

    ConfirmDialog {
        id: markReadDialog

        title: qsTr("Mark \"%1\" as read?").arg(dialogs.bookTitle)
        confirmText: qsTr("Confirm")
        onAccepted: dialogs.model.markRead(dialogs.folder)
    }

    ConfirmDialog {
        id: bulkDeleteDialog

        property var folders: []

        title: qsTr("Delete %n book(s)?", "", bulkDeleteDialog.folders.length)
        confirmText: qsTr("Delete")
        onAccepted: {
            for (const folder of bulkDeleteDialog.folders)
                dialogs.model.deleteBook(folder)
            dialogs.bulkDeleted()
        }
    }

    // A confirmation: the question as the title, Cancel, and the action as
    // the default button.
    component ConfirmDialog: Dialog {
        id: confirm

        property string confirmText

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(360, parent ? parent.width - 32 : 360)
        modal: true

        contentItem: Item {
            implicitHeight: 0
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                text: confirm.confirmText
                prominent: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
    }
}
