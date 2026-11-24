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

    // BookRenameSheet: the title field with Cancel and Save, as a kit alert.
    AlertDialog {
        id: renameDialog

        title: qsTr("Rename")
        hasField: true
        selectFieldOnOpen: true
        fieldPlaceholder: qsTr("Title")
        acceptText: qsTr("Save")
        rejectText: qsTr("Cancel")

        onAboutToShow: renameDialog.fieldText = dialogs.bookTitle
        onAccepted: dialogs.model.renameBook(dialogs.folder, renameDialog.fieldText.trim())
    }

    // BookCell's confirmationDialogs: Delete is destructive, Confirm is not.
    AlertDialog {
        id: deleteDialog

        title: qsTr("Delete \"%1\"?").arg(dialogs.bookTitle)
        acceptText: qsTr("Delete")
        acceptDestructive: true
        rejectText: qsTr("Cancel")
        onAccepted: dialogs.model.deleteBook(dialogs.folder)
    }

    AlertDialog {
        id: markReadDialog

        title: qsTr("Mark \"%1\" as read?").arg(dialogs.bookTitle)
        acceptText: qsTr("Confirm")
        rejectText: qsTr("Cancel")
        onAccepted: dialogs.model.markRead(dialogs.folder)
    }

    AlertDialog {
        id: bulkDeleteDialog

        property var folders: []

        title: qsTr("Delete %n book(s)?", "", bulkDeleteDialog.folders.length)
        acceptText: qsTr("Delete")
        acceptDestructive: true
        rejectText: qsTr("Cancel")
        onAccepted: {
            for (const folder of bulkDeleteDialog.folders)
                dialogs.model.deleteBook(folder)
            dialogs.bulkDeleted()
        }
    }
}
