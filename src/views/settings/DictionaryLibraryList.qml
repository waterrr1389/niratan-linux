// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary rows of Features/Settings/DictionaryView.swift: each row
// leads with dictionaryReorderHandle and carries the category picker, the
// enabled toggle and the row menu. dictionaryReorderGesture: a drag that
// starts on a row's handle and has passed the drag distance moves the
// dictionary onto each other visible row the pointer enters, as
// updateDictionaryDrag does, and the release point counts once more, as
// endDictionaryDrag does. One area over the list tracks the drag, because
// every move rebuilds the rows; a press outside a handle passes through to
// the row's controls.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

Item {
    id: list

    required property DictionarySettingsViewModel libraryModel
    // The page's scroll area, which clips the rows a drag may target.
    required property Flickable flickable
    required property list<string> categories

    height: rowsColumn.height

    Column {
        id: rowsColumn

        width: parent.width

        Repeater {
            id: rowsRepeater

            model: list.libraryModel.rows

            delegate: dictionaryRow
        }
    }

    MouseArea {
        id: reorder

        property string source
        property string target
        property bool dragging: false
        // Scene points, so a wheel scroll under a still pointer
        // retargets the drag.
        property point start
        property point pointer

        anchors.fill: parent
        preventStealing: true
        cursorShape: reorder.dragging ? Qt.ClosedHandCursor : undefined

        // The row under a point of the list, if the point is visible.
        function rowAt(x, y) {
            if (!reorder.contains(Qt.point(x, y))
                    || !list.flickable.contains(reorder.mapToItem(list.flickable, x, y)))
                return null
            for (let index = 0; index < rowsRepeater.count; ++index) {
                const row = rowsRepeater.itemAt(index)
                if (row && y >= row.y && y < row.y + row.height)
                    return row
            }
            return null
        }

        // updateDictionaryDrag: once past the drag distance, entering
        // another row moves the dragged dictionary onto it.
        function track() {
            if (!reorder.dragging)
                return
            const local = reorder.mapFromItem(null, reorder.pointer.x, reorder.pointer.y)
            const row = reorder.rowAt(local.x, local.y)
            if (!row)
                return
            const title = row.modelData.title
            if (title === reorder.source || title === reorder.target)
                return
            list.libraryModel.move(reorder.source, title)
            reorder.target = title
        }

        function follow(mouse) {
            reorder.pointer = reorder.mapToItem(null, mouse.x, mouse.y)
            if (!reorder.dragging && Math.hypot(reorder.pointer.x - reorder.start.x,
                                                reorder.pointer.y - reorder.start.y)
                    >= Application.styleHints.startDragDistance)
                reorder.dragging = true
            reorder.track()
        }

        function finish() {
            reorder.source = ""
            reorder.target = ""
            reorder.dragging = false
        }

        onPressed: mouse => {
            const row = reorder.rowAt(mouse.x, mouse.y)
            const handle = row ? row.handle : null
            const local = handle ? reorder.mapToItem(handle, mouse.x, mouse.y) : null
            if (!local || !handle.contains(local)) {
                mouse.accepted = false
                return
            }
            reorder.source = row.modelData.title
            reorder.target = ""
            reorder.dragging = false
            reorder.pointer = reorder.mapToItem(null, mouse.x, mouse.y)
            reorder.start = reorder.pointer
        }
        onPositionChanged: mouse => {
            if (reorder.source !== "")
                reorder.follow(mouse)
        }
        // endDictionaryDrag: the release point counts once more.
        onReleased: mouse => {
            if (reorder.source !== "")
                reorder.follow(mouse)
            reorder.finish()
        }
        onCanceled: reorder.finish()

        Connections {
            target: list.flickable
            enabled: reorder.dragging

            function onContentYChanged() {
                reorder.track()
            }
        }
    }

    Component {
        id: dictionaryRow

        Item {
            id: entry

            required property var modelData
            required property int index
            readonly property Item handle: handleArea

            width: rowsColumn.width
            height: entryColumn.height

            Column {
                id: entryColumn

                width: parent.width

                FormDivider { visible: entry.index > 0 }

                FormRow {
                    id: entryRow

                    label: entry.modelData.title
                    glyph: "line.3.horizontal"
                    glyphColor: Theme.textTertiary

                    MenuPicker {
                        visible: list.libraryModel.dictionaryType === "Term"
                        Accessible.name: qsTr("Category")
                        model: [qsTr("No Category"), qsTr("Monolingual"), qsTr("Bilingual"), qsTr("Exclude")]
                        currentIndex: list.categories.indexOf(entry.modelData.category)
                        onActivated: index => list.libraryModel.setCategory(entry.modelData.title, list.categories[index])
                    }

                    ToggleSwitch {
                        objectName: "dictionaryEnabledSwitch"
                        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                        Accessible.name: entry.modelData.title
                        checked: entry.modelData.enabled
                        onToggled: list.libraryModel.setEnabled(entry.modelData.title, checked)
                    }

                    GlyphButton {
                        id: actions

                        glyph: "ellipsis"
                        filled: false
                        diameter: Theme.smallGlyphButtonDiameter
                        Accessible.name: qsTr("Dictionary Actions")
                        onClicked: actionsMenu.popup(actions)

                        Menu {
                            id: actionsMenu

                            MenuItem {
                                text: qsTr("Move Up")
                                enabled: entry.index > 0
                                onTriggered: list.libraryModel.move(entry.modelData.title, list.libraryModel.rows[entry.index - 1].title)
                            }

                            MenuItem {
                                text: qsTr("Move Down")
                                enabled: entry.index + 1 < list.libraryModel.rows.length
                                onTriggered: list.libraryModel.move(entry.modelData.title, list.libraryModel.rows[entry.index + 1].title)
                            }

                            MenuSeparator {}

                            MenuItem {
                                text: qsTr("Delete")
                                onTriggered: list.libraryModel.remove(entry.modelData.title)
                            }
                        }
                    }
                }
            }

            // dictionaryReorderHandle: the glyph column of the row, where a
            // reorder drag starts.
            Item {
                id: handleArea

                objectName: "dictionaryReorderHandle"
                x: entryRow.padding
                y: entryRow.y
                width: Theme.glyphColumnWidth
                height: entryRow.height

                HoverHandler {
                    cursorShape: Qt.OpenHandCursor
                }
            }
        }
    }
}
