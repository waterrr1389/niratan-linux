// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bookshelf's book menu and its confirmations, following
// Features/Bookshelf/BookCell.swift (the context menu: Move, the ッツ Sync
// entry, Mark Read, Rename and Delete, with their glyphs; `BookRenameSheet`;
// the delete and mark-read confirmation dialogs), NativeMac/NativeReuseViews.swift
// (the bulk delete alert of `bookshelfContent`) and Features/Bookshelf/
// BookshelfView.swift with BookshelfViewModel.syncBook and handleSyncResult
// (the Syncing... overlay, the error alert and the result alert). Sync is a
// single item in Auto mode and an Import/Export submenu in Manual mode, and
// only shown while sync is enabled. The Export entry is out of scope.
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
        dialogs.placeSyncEntry()
        bookMenu.popup()
    }

    // BookCell shows one ッツ Sync entry, chosen by syncMode, right after Move.
    // take*() detaches the entries without destroying them, so they can be
    // inserted again on the next popup.
    function placeSyncEntry() {
        for (let i = bookMenu.count - 1; i >= 0; --i) {
            const row = bookMenu.itemAt(i)
            if (row === syncItem)
                bookMenu.takeItem(i)
            else if (row && row.subMenu === syncMenu)
                bookMenu.takeMenu(i)
        }
        if (!SyncSettingsViewModel.enabled || SyncSettingsViewModel.provider !== "ttu")
            return
        if (SyncSettingsViewModel.syncMode === "Manual")
            bookMenu.insertMenu(1, syncMenu)
        else
            bookMenu.insertItem(1, syncItem)
    }

    function confirmBulkDelete(folders) {
        bulkDeleteDialog.folders = folders
        bulkDeleteDialog.open()
    }

    MenuItem {
        id: syncItem

        objectName: "bookSyncItem"

        text: qsTr("Sync")
        icon.source: IconMap.source("arrow.triangle.2.circlepath")
        onTriggered: SyncSettingsViewModel.syncBook(dialogs.folder, "")
    }

    Menu {
        id: syncMenu

        objectName: "bookSyncMenu"

        title: qsTr("Sync")
        icon.source: IconMap.source("arrow.triangle.2.circlepath")

        MenuItem {
            text: qsTr("Import")
            icon.source: IconMap.source("arrow.down")
            onTriggered: SyncSettingsViewModel.syncBook(dialogs.folder, "import")
        }
        MenuItem {
            text: qsTr("Export")
            icon.source: IconMap.source("arrow.up")
            onTriggered: SyncSettingsViewModel.syncBook(dialogs.folder, "export")
        }
    }

    Menu {
        id: bookMenu

        objectName: "bookMenu"

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

    LoadingOverlay {
        message: qsTr("Syncing...")
        active: SyncSettingsViewModel.syncing
    }

    AlertDialog {
        id: syncErrorDialog

        title: qsTr("Error")
        message: qsTr("Sync failed: %1").arg(SyncSettingsViewModel.syncError)
        onClosed: SyncSettingsViewModel.clearSyncError()
    }

    // handleSyncResult: a skipped result shows nothing.
    AlertDialog {
        id: syncResultDialog
    }

    Connections {
        target: SyncSettingsViewModel

        function onSyncErrorChanged() {
            if (SyncSettingsViewModel.syncError !== "")
                syncErrorDialog.open()
        }

        function onSyncFinished(kind, title, characterCount) {
            const messages = [
                qsTr("%1 is already synced").arg(title),
                qsTr("Synced %1 from ッツ\n%2 characters").arg(title).arg(characterCount),
                qsTr("Synced %1 to ッツ\n%2 characters").arg(title).arg(characterCount)
            ]
            if (kind < 0 || kind >= messages.length)
                return
            // handleSyncResult reloads progress after an import.
            if (kind === 1)
                dialogs.model.refresh()
            syncResultDialog.message = messages[kind]
            syncResultDialog.open()
        }
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
