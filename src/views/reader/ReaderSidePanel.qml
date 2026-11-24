// SPDX-License-Identifier: GPL-3.0-or-later
//
// The floating reader side-panel surface follows
// NativeMac/NativeReaderView.swift NativeReaderSidePanel. ReaderPage keeps the
// leading panel 440 px wide and sizes the trailing panel through
// Theme.readerSidePanelWidth. The card spans the available height, has a 20 px
// radius and a 0.7 px primary hairline, and never takes space from the
// chapter. The page positions it 12 px from its edge and animates it in; this
// file only draws the surface and hosts the panel content.
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

    radius: Theme.readerSidePanelRadius
    color: Theme.cardBackground
    border.width: Theme.readerSidePanelStrokeWidth
    border.color: Theme.readerSidePanelStroke

    Item {
        id: contentItem
        anchors.fill: parent
    }
}
