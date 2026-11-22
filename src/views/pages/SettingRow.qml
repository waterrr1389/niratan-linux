// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsRow and NativeSettingsSubtitledLabel.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

GridLayout {
    id: settingRow

    required property string label
    property color labelColor: Theme.textPrimary
    property string subtitle
    property bool stacked: false
    readonly property real labelWidth: title.implicitWidth
    default property alias controls: field.data

    columns: stacked ? 1 : 2
    Layout.fillWidth: true
    Layout.minimumHeight: 46
    Layout.leftMargin: stacked ? 12 : 16
    Layout.rightMargin: stacked ? 12 : 16
    Layout.topMargin: stacked ? Theme.spacingSm : 0
    Layout.bottomMargin: stacked ? Theme.spacingSm : 0
    columnSpacing: Theme.spacingMd
    rowSpacing: Theme.spacingXs

    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 2

        Label {
            id: title

            Layout.fillWidth: true
            text: settingRow.label
            color: settingRow.labelColor
            font.pixelSize: Theme.fontBody
            font.weight: Font.Medium
            wrapMode: Text.Wrap
        }

        SettingsNote {
            visible: settingRow.subtitle.length > 0
            text: settingRow.subtitle
        }
    }

    RowLayout {
        id: field

        Layout.fillWidth: settingRow.stacked
        spacing: Theme.spacingSm
    }
}
