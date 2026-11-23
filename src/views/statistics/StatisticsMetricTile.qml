// SPDX-License-Identifier: GPL-3.0-or-later
//
// One label-over-value tile of the statistics dashboard, following the metric
// tiles in Features/Bookshelf/StatisticsDashboardView.swift. Every tile shares
// the object name `metricTile`, so a test can count a card's metrics without
// matching their text against the segment labels.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Rectangle {
    id: tile

    objectName: "metricTile"
    property string label
    property string value
    implicitWidth: 96
    implicitHeight: tileLayout.implicitHeight + 22
    radius: 12
    color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.05)

    ColumnLayout {
        id: tileLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 11
        spacing: 3

        Label {
            Layout.fillWidth: true
            text: tile.label
            color: Theme.textSecondary
            font.pixelSize: 11
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: tile.value
            color: Theme.textPrimary
            font.pixelSize: 17
            font.bold: true
            elide: Text.ElideRight
        }
    }
}
