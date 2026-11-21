// SPDX-License-Identifier: GPL-3.0-or-later
//
// One field mapping row of Features/Settings/AnkiView.swift (body, the ForEach
// over noteType.fields), following NativeMac/NativeReuseViews.swift
// NativeSettingsEditableMenuField: the field name as a secondary caption above
// a free-text mapping and its menu. The menu lists "-" (which clears the
// mapping), a divider, then the handlebar options in order.
//
// The row only reports edits; the page wires `edited` and `optionChosen` to the
// view model's setFieldMapping, which saves every change.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: fieldRow

    required property string field
    required property string mapping
    required property var options

    signal edited(string text)
    signal optionChosen(string option)

    spacing: 6

    Label {
        Layout.fillWidth: true
        text: fieldRow.field
        color: Theme.textSecondary
        font.pixelSize: Theme.fontCaption
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingXs

        TextField {
            id: mappingInput

            Layout.fillWidth: true
            placeholderText: qsTr("None")
            selectByMouse: true
            text: fieldRow.mapping
            onTextEdited: fieldRow.edited(mappingInput.text)
        }

        Button {
            id: menuButton

            implicitWidth: 32
            implicitHeight: 28
            padding: 0
            text: "\u25BE"

            Accessible.name: qsTr("Fields")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Fields")
            onClicked: optionMenu.popup(menuButton)

            Menu {
                id: optionMenu

                MenuItem {
                    text: "-"
                    onTriggered: fieldRow.optionChosen("")
                }

                MenuSeparator {
                }

                Repeater {
                    model: fieldRow.options

                    delegate: MenuItem {
                        required property string modelData

                        text: modelData
                        onTriggered: fieldRow.optionChosen(modelData)
                    }
                }
            }
        }
    }
}
