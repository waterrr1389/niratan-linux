// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsToggle.

import QtQuick
import QtQuick.Controls
import Niratan

SettingRow {
    id: row

    required property bool checked
    signal edited(bool value)

    Switch {
        Accessible.name: row.label
        checked: row.checked
        onToggled: row.edited(checked)
    }
}
