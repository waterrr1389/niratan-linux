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
// first and keeps the books in the library. Rename starts an inline editor in
// the row: the label becomes a focused text field with the current name and
// placeholder "Shelf name", the folder glyph stays and the count badge hides.
// Return and focus loss commit through LibraryViewModel.renameShelf (so empty
// or unchanged names leave the shelf alone and a duplicate is rejected), and
// Escape cancels. Shelf dragging is off while the editor is open. Swift
// creates a shelf inline too; New Shelf still uses the name dialog here.
// Google Drive's row is out of scope.
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

    // LibraryShelfSidebar.editingShelfID and draftName: the shelf whose row
    // holds the inline editor, and its text.
    property string editingShelf: ""
    property string draftShelfName: ""
    readonly property bool shelfDraggingEnabled: editingShelf.length === 0

    objectName: "libraryShelfColumn"
    color: Theme.columnBackground
    activeFocusOnTab: true

    function beginRename(name) {
        if (name === editingShelf)
            return
        editingShelf = ""
        draftShelfName = name
        editingShelf = name
    }

    // commitRename and cancelRename: a stale callback from a row that already
    // stopped editing changes nothing.
    function commitRename(name) {
        if (name !== editingShelf)
            return
        const from = editingShelf
        const to = draftShelfName.trim()
        editingShelf = ""
        if (to.length === 0 || to === from)
            return
        column.model.renameShelf(from, to)
    }

    function cancelRename(name) {
        if (name === editingShelf)
            editingShelf = ""
    }

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

    // LibraryShelfSidebar's confirmationDialog: a destructive Delete Shelf.
    AlertDialog {
        id: deleteShelfDialog

        property string shelfName

        title: qsTr("Delete shelf “%1”?").arg(deleteShelfDialog.shelfName)
        message: qsTr("Everything on this shelf stays in the library.")
        acceptText: qsTr("Delete Shelf")
        acceptDestructive: true
        rejectText: qsTr("Cancel")

        onAccepted: {
            column.cancelRename(deleteShelfDialog.shelfName)
            column.model.deleteShelf(deleteShelfDialog.shelfName)
        }
    }

    Menu {
        id: shelfMenu

        property string shelfName

        MenuItem {
            objectName: "libraryRenameShelfItem"
            text: qsTr("Rename")
            icon.source: IconMap.source("pencil")
            onTriggered: column.beginRename(shelfMenu.shelfName)
        }
        MenuSeparator {}
        MenuItem {
            objectName: "libraryDeleteShelfItem"
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

        objectName: "libraryShelfList"
        property int dragSource: -1
        property int dragDestination: -1

        function shelfOffset(scenePosition: point, insertion: bool): int {
            const position = mapFromItem(null, scenePosition)
            if (position.x < 0 || position.x >= width || position.y < 0 || position.y >= height)
                return -1
            const pos = rows.mapFromItem(list, position)
            const first = customShelves.itemAt(0)
            if (!first || pos.y < first.y || pos.x < 0 || pos.x >= rows.width)
                return -1
            for (let i = 0; i < customShelves.count; ++i) {
                const row = customShelves.itemAt(i)
                if (insertion ? pos.y < row.y + row.height / 2
                              : pos.y >= row.y && pos.y < row.y + row.height)
                    return i
            }
            return insertion ? customShelves.count : -1
        }

        function clearDrag() {
            dragSource = -1
            dragDestination = -1
        }

        anchors.top: parent.top
        anchors.topMargin: column.toolbarHeight
        anchors.bottom: footer.top
        width: parent.width
        contentHeight: rows.implicitHeight + Theme.spacingSm
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        DragHandler {
            id: shelfDrag

            parent: list
            target: null
            enabled: column.shelfDraggingEnabled
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onActiveChanged: {
                if (active) {
                    list.dragSource = list.shelfOffset(centroid.scenePressPosition, false)
                    list.dragDestination = list.shelfOffset(centroid.scenePosition, true)
                }
            }
            onCentroidChanged: {
                if (active && list.dragSource >= 0)
                    list.dragDestination = list.shelfOffset(centroid.scenePosition, true)
            }
            onGrabChanged: (transition, point) => {
                if (transition === PointerDevice.UngrabExclusive) {
                    const source = list.dragSource
                    const destination = list.shelfOffset(point.scenePosition, true)
                    list.clearDrag()
                    if (enabled && source >= 0 && destination >= 0
                            && destination !== source && destination !== source + 1)
                        column.model.moveShelves([source], destination)
                } else if (transition === PointerDevice.CancelGrabExclusive) {
                    list.clearDrag()
                }
            }
        }

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

                    objectName: "librarySmartShelfRow"
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
                id: customShelves

                model: column.model.shelves

                delegate: SourceListRow {
                    id: shelfRow

                    objectName: "libraryCustomShelfRow"
                    required property int index
                    required property var modelData

                    width: rows.width
                    symbol: "folder"
                    title: modelData.name
                    badge: modelData.count
                    selected: column.model.selectedShelf === "shelf:" + modelData.name
                    emphasized: column.activeFocus
                    editing: column.editingShelf === modelData.name
                    editorObjectName: "libraryShelfNameField"
                    editorPlaceholderText: qsTr("Shelf name")
                    onClicked: {
                        column.forceActiveFocus()
                        column.model.selectedShelf = "shelf:" + modelData.name
                    }
                    onDoubleClicked: column.beginRename(modelData.name)
                    onContextMenuRequested: {
                        shelfMenu.shelfName = modelData.name
                        shelfMenu.popup()
                    }
                    onEditingChanged: {
                        if (editing)
                            editorText = column.draftShelfName
                    }
                    onEditorTextChanged: {
                        if (editing)
                            column.draftShelfName = editorText
                    }
                    onEditorAccepted: column.commitRename(modelData.name)
                    onEditorCancelled: column.cancelRename(modelData.name)
                    onEditorFocusLost: column.commitRename(modelData.name)

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        y: list.dragDestination === customShelves.count ? parent.height : 0
                        height: Theme.hairlineWidth
                        color: Theme.accent
                        visible: list.dragSource >= 0
                            && (list.dragDestination === shelfRow.index
                                || (list.dragDestination === customShelves.count
                                    && shelfRow.index === customShelves.count - 1))
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
