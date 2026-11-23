// SPDX-License-Identifier: GPL-3.0-or-later
//
// The compact segmented control used by the statistics dashboard, following
// the range, time grain, metric, style, target type and ranking pickers in
// Features/Bookshelf/StatisticsDashboardView.swift.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

RowLayout {
    id: segments

    property var labels: []
    property int currentIndex: 0
    signal activated(int index)
    spacing: 3

    Repeater {
        model: segments.labels
        delegate: Button {
            id: segment
            required property int index
            required property var modelData
            text: String(modelData)
            leftPadding: 10
            rightPadding: 10
            topPadding: 5
            bottomPadding: 5
            onClicked: segments.activated(segment.index)
            background: Rectangle {
                radius: 8
                color: segments.currentIndex === segment.index
                    ? Theme.accent
                    : Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                              Theme.textPrimary.b, 0.06)
                border.width: 1
                border.color: segments.currentIndex === segment.index
                    ? Theme.accent
                    : Theme.cardStroke
            }
            contentItem: Text {
                text: segment.text
                color: segments.currentIndex === segment.index
                    ? Theme.popupBackground
                    : Theme.textPrimary
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }
}
