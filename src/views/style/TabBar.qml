// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tab bar in the Niratan control style, after the macOS segmented control
// Mac Niratan uses for its inspector tabs (Pictures/video1.png): a capsule
// track holding equal segments.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.TabBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 2
    spacing: 2
    // Segments share the width equally, as macOS segmented controls do, so
    // the bar is as wide as its widest segment times the count.
    property int measureCount: 0

    function remeasure() {
        ++measureCount
    }

    contentWidth: {
        void measureCount
        let widest = 0
        for (let index = 0; index < count; ++index)
            widest = Math.max(widest, itemAt(index).implicitWidth)
        return widest * count + spacing * Math.max(0, count - 1)
    }

    contentItem: ListView {
        model: control.contentModel
        currentIndex: control.currentIndex
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.AutoFlickIfNeeded
        snapMode: ListView.SnapToItem
        highlightMoveDuration: 0
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: 40
        preferredHighlightEnd: width - 40
    }

    background: Rectangle {
        implicitHeight: Theme.controlHeight
        radius: height / 2
        color: Theme.inactiveSelection
    }
}
