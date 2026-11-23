// SPDX-License-Identifier: GPL-3.0-or-later
//
// A row of leading action buttons, 8 apart. Follows
// NativeMac/NativeReuseViews.swift NativeSettingsButtonRow: at least 48 high
// with padding 16 in cards, 36 high without padding in the compact density,
// which follows the enclosing FormSection unless set.
import QtQuick
import Niratan

Item {
    id: row

    property bool compact: section ? (section as FormSection).compact : false
    default property alias buttons: content.data

    readonly property Item section: {
        for (let item = parent; item; item = item.parent) {
            if (item instanceof FormSection)
                return item
        }
        return null
    }

    width: parent ? parent.width : implicitWidth
    implicitWidth: content.implicitWidth + 2 * content.x
    implicitHeight: Math.max(compact ? Theme.compactButtonRowMinHeight : Theme.buttonRowMinHeight,
                             content.implicitHeight)
    opacity: enabled ? 1 : Theme.disabledOpacity

    Row {
        id: content

        x: row.compact ? 0 : Theme.rowPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.capsuleSpacing
    }
}
