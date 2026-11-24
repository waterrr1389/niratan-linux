// SPDX-License-Identifier: GPL-3.0-or-later
//
// A row whose trailing control is a MenuPicker. Follows
// NativeMac/NativeReuseViews.swift NativeGlassMenuPicker as a NativeSettingsRow
// accessory (Features/Settings/AppearanceView.swift Font row). activated is
// forwarded from the picker on a user choice only.
import QtQuick
import Niratan

FormRow {
    id: row

    property var model: []
    property int currentIndex: -1
    property real minimumWidth: Theme.pickerMinimumWidth
    property alias placeholderText: picker.placeholderText
    property alias constrainMenuHeight: picker.constrainMenuHeight
    property alias largeList: picker.largeList

    // The object name of the picker, for tests that drive the control.
    property alias controlName: picker.objectName

    signal activated(int index)

    accessoryItem: picker

    MenuPicker {
        id: picker

        // A long value elides inside the picker instead of crowding out the
        // label: the picker takes at most half the row.
        width: Math.min(implicitWidth, Math.max(minimumWidth, row.width / 2))
        model: row.model
        currentIndex: row.currentIndex
        minimumWidth: row.minimumWidth
        Accessible.name: row.label
        onActivated: index => row.activated(index)
    }
}
