// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

SettingRow {
    id: sliderRow

    required property real from
    required property real to
    required property real stepSize
    required property int decimals
    required property real value
    signal edited(real value)

    Slider {
        id: slider

        Accessible.name: sliderRow.label
        Layout.preferredWidth: 8 * Theme.spacingLg
        Layout.minimumWidth: 4 * Theme.spacingLg
        from: sliderRow.from
        to: sliderRow.to
        stepSize: sliderRow.stepSize
        snapMode: Slider.SnapAlways
        value: sliderRow.value
        onMoved: sliderRow.edited(slider.value)
    }

    Label {
        Layout.minimumWidth: 3 * Theme.spacingMd
        text: slider.value.toLocaleString(Qt.locale(), "f", sliderRow.decimals)
        color: Theme.textSecondary
        horizontalAlignment: Text.AlignRight
    }
}
