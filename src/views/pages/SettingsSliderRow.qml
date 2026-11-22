// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsSliderRow.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: row

    required property string label
    required property real from
    required property real to
    required property real stepSize
    required property int decimals
    required property real value
    property string suffix
    signal edited(real value)

    Layout.fillWidth: true
    Layout.leftMargin: 16
    Layout.rightMargin: 16
    Layout.topMargin: 10
    Layout.bottomMargin: 10
    spacing: Theme.spacingSm

    RowLayout {
        Layout.fillWidth: true

        Label {
            Layout.fillWidth: true
            text: row.label
            font.weight: Font.Medium
        }

        Label {
            text: slider.value.toLocaleString(Qt.locale(), "f", row.decimals) + row.suffix
            font.weight: Font.DemiBold
        }
    }

    Slider {
        id: slider

        Layout.fillWidth: true
        Accessible.name: row.label
        from: row.from
        to: row.to
        stepSize: row.stepSize
        snapMode: Slider.SnapAlways
        value: row.value
        onMoved: row.edited(value)
    }
}
