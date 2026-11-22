// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's Appearance side panel. It hosts the reader settings the reader
// already supports rather than disconnected copies: font size and vertical
// writing are edited through the profile-backed SettingsViewModel, which saves
// them for the active profile; the open reader watches that file and reloads
// the chapter with the new layout.
//
// The full Appearance settings page belongs to the settings section; this
// inspector carries only what the reader itself changes.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: panel

    signal closeRequested()

    spacing: 0

    SettingsViewModel {
        id: appearance
    }

    ReaderInspectorHeader {
        Layout.fillWidth: true
        title: qsTr("Appearance")
        onCloseRequested: panel.closeRequested()
    }

    ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Theme.spacingMd

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.topMargin: 4
                spacing: Theme.spacingSm

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Font size")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                }

                SpinBox {
                    objectName: "appearanceFontSize"
                    from: 16
                    to: 40
                    value: appearance.fontSize
                    editable: false
                    Accessible.name: qsTr("Font size")
                    onValueModified: appearance.fontSize = value
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: Theme.spacingSm

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Vertical writing")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                }

                Switch {
                    objectName: "appearanceVerticalWriting"
                    checked: appearance.verticalWriting
                    Accessible.name: qsTr("Vertical writing")
                    onToggled: appearance.verticalWriting = checked
                }
            }
        }
    }
}
