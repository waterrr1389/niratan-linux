// SPDX-License-Identifier: GPL-3.0-or-later
//
// A row with a trailing ToggleSwitch and no switch label. Follows
// NativeMac/NativeReuseViews.swift NativeSettingsToggle and
// Features/Video/VideoInspectorView.swift toggleRow, which uses the small
// switch. The switch follows `checked`; toggled is emitted on a user change
// only, and the page writes the new value back.
import QtQuick
import Niratan

FormRow {
    id: row

    property bool checked: false

    signal toggled(bool checked)

    accessoryItem: control.indicator

    ToggleSwitch {
        id: control

        small: row.compact
        checked: row.checked
        Accessible.name: row.label
        onToggled: row.toggled(checked)
    }
}
