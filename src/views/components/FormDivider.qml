// SPDX-License-Identifier: GPL-3.0-or-later
//
// A hairline between two rows of a section, inset on the leading side by the
// enclosing section's row padding (16 cards, 12 compact) and running to its
// trailing edge. Follows NativeMac/NativeReuseViews.swift
// NativeSettingsSeparator and Features/Video/VideoInspectorView.swift
// rowDivider; Swift's half-point line is one pixel here. Kit rows never draw
// their own dividers.
import QtQuick
import Niratan

Rectangle {
    id: divider

    readonly property Item section: {
        for (let item = parent; item; item = item.parent) {
            if (item instanceof FormSection)
                return item
        }
        return null
    }
    readonly property bool compact: section ? (section as FormSection).compact : false

    x: compact ? Theme.compactRowPadding : Theme.rowPadding
    width: parent ? parent.width - x : 0
    implicitHeight: Theme.hairlineWidth
    color: Theme.separator
}
