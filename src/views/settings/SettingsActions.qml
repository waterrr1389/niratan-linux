// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsButtonRow and NativeSettingsActionButtonStyle.

import QtQuick
import QtQuick.Layouts
import Niratan

RowLayout {
    default property alias buttons: actions.data

    Layout.fillWidth: true
    Layout.minimumHeight: 46
    Layout.leftMargin: 16
    Layout.rightMargin: 16
    spacing: 0

    RowLayout {
        id: actions
        spacing: Theme.spacingSm
    }

    Item {
        Layout.fillWidth: true
    }
}
