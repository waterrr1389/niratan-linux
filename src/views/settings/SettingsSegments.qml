// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeGlassSegmentedPicker.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

TabBar {
    id: bar

    required property list<string> labels
    required property int selectedIndex
    property int minimumSegmentWidth: 0
    signal edited(int index)

    currentIndex: count === labels.length ? selectedIndex : -1
    Keys.onLeftPressed: edited(Math.max(0, selectedIndex - 1))
    Keys.onRightPressed: edited(Math.min(labels.length - 1, selectedIndex + 1))

    Repeater {
        model: bar.labels

        TabButton {
            required property int index
            required property string modelData

            text: modelData
            implicitWidth: Math.max(bar.minimumSegmentWidth, implicitContentWidth + leftPadding + rightPadding)
            onClicked: bar.edited(index)
        }
    }
}
