// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/AppearanceView.swift: the Theme card's ColorPicker rows
// (Background Color, Text Color, Info Color). A NativeSettingsRow whose
// accessory is the swatch of a labels-hidden ColorPicker; picking a colour
// emits the row's edited signal with the QColor the dialog accepted.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import Niratan

SettingRow {
    id: row

    required property color value
    signal edited(color value)

    Rectangle {
        objectName: row.objectName + "Swatch"

        Layout.preferredWidth: 24
        Layout.preferredHeight: 24
        radius: 6
        color: row.value
        border.width: 1
        border.color: Theme.cardStroke
        Accessible.name: row.label
        Accessible.role: Accessible.Button

        TapHandler {
            onTapped: {
                picker.selectedColor = row.value;
                picker.open();
            }
        }
    }

    ColorDialog {
        id: picker

        title: row.label
        options: ColorDialog.ShowAlphaChannel
        onAccepted: row.edited(selectedColor)
    }
}
