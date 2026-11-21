// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

RowLayout {
    id: settingRow

    required property string label
    default property alias controls: field.data

    Layout.fillWidth: true
    spacing: Theme.spacingMd

    Label {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: settingRow.label
        color: Theme.textPrimary
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
    }

    RowLayout {
        id: field
        spacing: Theme.spacingSm
    }
}
