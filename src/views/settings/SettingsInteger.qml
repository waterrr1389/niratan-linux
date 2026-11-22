// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Settings/AppearanceView.swift: nativeAppearanceContent's numeric steppers.

import QtQuick
import QtQuick.Controls
import Niratan

SettingRow {
    id: row

    required property int from
    required property int to
    required property int value
    property string suffix
    signal edited(int value)

    SpinBox {
        Accessible.name: row.label
        from: row.from
        to: row.to
        value: row.value
        editable: row.suffix.length === 0
        textFromValue: (value, locale) => Number(value).toLocaleString(locale, "f", 0) + row.suffix
        onValueModified: row.edited(value)
    }
}
