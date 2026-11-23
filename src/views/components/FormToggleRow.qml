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

    // The object name of the switch, for tests that drive the control.
    property alias controlName: control.objectName

    signal toggled(bool checked)

    accessoryItem: control.indicator

    ToggleSwitch {
        id: control

        small: row.compact
        checked: row.checked
        Accessible.name: row.label
        // The switch stays bound to `checked`, so a change the page does not
        // store springs back.
        onToggled: {
            row.toggled(checked)
            checked = Qt.binding(() => row.checked)
        }
    }
}
