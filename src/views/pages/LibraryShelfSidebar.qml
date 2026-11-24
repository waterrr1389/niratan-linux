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
    readonly property int toolbarHeight: Theme.sourceListToolbarHeight

    color: Theme.columnBackground
    activeFocusOnTab: true

    AlertDialog {
        id: shelfDialog

        title: qsTr("New Shelf")
        hasField: true
        selectFieldOnOpen: true
        fieldPlaceholder: qsTr("Shelf name")
        rejectText: qsTr("Cancel")

        onAboutToShow: shelfDialog.fieldText = qsTr("New Shelf")
        onAccepted: {
            const created = column.model.createShelf(shelfDialog.fieldText)
            if (created.length > 0)
                column.model.selectedShelf = "shelf:" + created
        }
    }

    AlertDialog {
        id: renameShelfDialog

        property string fromName

        title: qsTr("Rename Shelf")
        hasField: true
        selectFieldOnOpen: true
        fieldPlaceholder: qsTr("Shelf name")
        rejectText: qsTr("Cancel")

        onAboutToShow: renameShelfDialog.fieldText = renameShelfDialog.fromName
        onAccepted: column.model.renameShelf(renameShelfDialog.fromName, renameShelfDialog.fieldText)
    }

    // LibraryShelfSidebar's confirmationDialog: a destructive Delete Shelf.
    AlertDialog {
        id: deleteShelfDialog

        property string shelfName

        title: qsTr("Delete shelf “%1”?").arg(deleteShelfDialog.shelfName)
        message: qsTr("Everything on this shelf stays in the library.")
        acceptText: qsTr("Delete Shelf")
        acceptDestructive: true
        rejectText: qsTr("Cancel")

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

    SourceListTitle {
        leadingInset: column.toolbarInset
        text: column.title
    }

    Flickable {
        id: list

        anchors.top: parent.top
        anchors.topMargin: column.toolbarHeight
        anchors.bottom: footer.top
        width: parent.width
        contentHeight: rows.implicitHeight + Theme.spacingSm
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        Column {
            id: rows

            x: Theme.sourceListInset
            width: list.width - 2 * Theme.sourceListInset
            spacing: Theme.sourceListRowSpacing

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

            SourceListCaption {
                text: qsTr("Shelves")
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
        height: newShelf.implicitHeight + 2 * Theme.sourceListFooterPadding

        Button {
            id: newShelf

            x: Theme.sourceListFooterPadding
            anchors.verticalCenter: parent.verticalCenter
            flat: true
            leftPadding: Theme.spacingSm
            text: qsTr("New Shelf")
            icon.source: IconMap.source("plus")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("New Shelf")
            ToolTip.delay: Theme.tooltipDelay
            onClicked: shelfDialog.open()
        }
    }
}
