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
// only shown while sync is enabled. The book's own Export entry follows
// BookCell.bookExportURL: it exists only when an EPUB file name is stored,
// sits after Rename and before Delete, and opens Niratan-win's save dialog
// (NovelLibraryPageViewModel.ExportNovelAsync).
//
// Decision: the Move submenu also checks the book's current shelf (None when
// it is on no shelf); Swift only disables that row, and the owner asked to see
// where the book is.
pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: dialogs

    required property LibraryViewModel model

    signal bulkDeleted()

    property string folder
    property string bookTitle
    // The shelf the book is on, read on every popup: the Move submenu marks
    // it and disables it, as BookCell disables the current destination.
    property string currentBookShelf

    function showBookMenu(folder, title) {
        dialogs.folder = folder
        dialogs.bookTitle = title
        dialogs.currentBookShelf = dialogs.model.shelfForBook(folder)
        dialogs.placeSyncEntry()
        dialogs.placeExportEntry()
        bookMenu.popup()
    }

    // BookCell adds the Export button only when the book stores an EPUB name,
    // between Rename and Delete. It is inserted afresh for each popup so the
    // menu's item count matches the entries Swift builds.
    property QtObject exportEntry: null

    function placeExportEntry() {
        bookMenu.currentIndex = -1
        if (dialogs.exportEntry)
            bookMenu.removeItem(dialogs.exportEntry)
        dialogs.exportEntry = null
        if (!dialogs.model.canExportBook(dialogs.folder))
            return
        const item = exportItemComponent.createObject(dialogs) as MenuItem
        bookMenu.insertItem(bookMenu.count - 1, item)
        dialogs.exportEntry = item
    }

    Component {
        id: exportItemComponent

        MenuItem {
            objectName: "bookExportItem"

            text: qsTr("Export")
            icon.source: IconMap.source("square.and.arrow.up")
            onTriggered: {
                exportDialog.folder = dialogs.folder
                exportDialog.currentFile = dialogs.exportPath(dialogs.folder)
                exportDialog.open()
            }
        }
    }

    // BookCell shows one ッツ Sync entry, chosen by syncMode, right after Move.
    // Each popup removes the previous entry and inserts a freshly created one:
    // a row detached from the menu and inserted again stays undrawn and deaf
    // to the pointer, and a hidden row inside a closed menu is not laid out
    // again when its height changes.
    property QtObject syncEntry: null

    function placeSyncEntry() {
        // The menu may still be open on the entry being replaced; drop the
        // selection so removing that row does not move it in a loop.
        bookMenu.currentIndex = -1
        if (dialogs.syncEntry instanceof Menu) {
            // Removing a submenu row does not close the submenu itself.
            const previous = dialogs.syncEntry as Menu
            previous.close()
            bookMenu.removeMenu(previous)
        } else if (dialogs.syncEntry)
            bookMenu.removeItem(dialogs.syncEntry)
        dialogs.syncEntry = null
        if (!SyncSettingsViewModel.enabled || SyncSettingsViewModel.provider !== "ttu")
            return
        if (SyncSettingsViewModel.syncMode === "Manual") {
            const menu = syncMenuComponent.createObject(dialogs) as Menu
            bookMenu.insertMenu(1, menu)
            dialogs.syncEntry = menu
        } else {
            const item = syncItemComponent.createObject(dialogs) as MenuItem
            bookMenu.insertItem(1, item)
            dialogs.syncEntry = item
        }
    }

    function confirmBulkDelete(folders) {
        bulkDeleteDialog.folders = folders
        bulkDeleteDialog.open()
    }

    // NovelLibraryPageViewModel.ExportNovelAsync: the save dialog starts in
    // Downloads with the display title's base name and the .epub extension.
    function exportPath(folder) {
        const downloads = StandardPaths.writableLocation(StandardPaths.DownloadsLocation)
        const directory = downloads.length > 0
            ? downloads
            : StandardPaths.writableLocation(StandardPaths.HomeLocation)
        return "file://" + directory + "/" + dialogs.model.exportBaseName(folder) + ".epub"
    }

    Component {
        id: syncItemComponent

        MenuItem {
            objectName: "bookSyncItem"

            text: qsTr("Sync")
            icon.source: IconMap.source("arrow.triangle.2.circlepath")
            onTriggered: SyncSettingsViewModel.syncBook(dialogs.folder, "")
        }
    }

    Component {
        id: syncMenuComponent

        Menu {
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
                checked: dialogs.currentBookShelf === ""
                enabled: !checked
                onTriggered: dialogs.model.moveBookToShelf(dialogs.folder, "")
            }

            Repeater {
                model: dialogs.model.shelves

                delegate: MenuItem {
                    required property var modelData

                    text: modelData.name
                    icon.source: IconMap.source("folder")
                    checked: dialogs.currentBookShelf === modelData.name
                    enabled: !checked
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

    FileDialog {
        id: exportDialog

        property string folder

        title: qsTr("Export EPUB")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("EPUB books (*.epub)")]
        defaultSuffix: "epub"
        onAccepted: dialogs.model.exportBook(exportDialog.folder, exportDialog.selectedFile)
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
