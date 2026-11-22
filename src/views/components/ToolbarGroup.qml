// SPDX-License-Identifier: GPL-3.0-or-later
//
// One ToolbarItemGroup of a Mac Niratan window toolbar: its buttons side by
// side on a glass capsule (NativeMac/NativeGlassSurface.swift
// nativeGlassCapsuleSurface), drawn solid with a hairline.
import QtQuick
import Niratan

Item {
    id: group

    default property alias buttons: row.data

    implicitWidth: row.implicitWidth + 8
    implicitHeight: row.implicitHeight + 8

    GlassCapsule {
        anchors.fill: parent
    }

    Row {
        id: row

        anchors.centerIn: parent
        spacing: 2
    }
}
