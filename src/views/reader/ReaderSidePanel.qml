// SPDX-License-Identifier: GPL-3.0-or-later
//
// The floating side panel of the reader, following
// NativeMac/NativeReaderView.swift `NativeReaderSidePanel`: a 440 px wide card
// that spans the available height, has a 20 px radius and a 0.7 px primary
// hairline, and never take space from the chapter. The page positions it 12 px
// from its edge and animates it in; this file only draws the surface and hosts
// the panel content.
//
// PLAN.md keeps glass a solid surface, so the Swift shadow is not reproduced.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Rectangle {
    id: panel

    default property alias content: contentItem.data

    // Qt.LeftEdge or Qt.RightEdge, for the rounded edge that meets the page.
    property int edge: Qt.LeftEdge

    radius: 20
    color: Theme.cardBackground
    border.width: 0.7
    border.color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b,
                          Theme.dark ? 0.16 : 0.10)

    Item {
        id: contentItem
        anchors.fill: parent
    }
}
