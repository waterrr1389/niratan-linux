// SPDX-License-Identifier: GPL-3.0-or-later
//
// One label-over-value tile of the statistics dashboard, following the metric
// tiles in Features/Bookshelf/StatisticsDashboardView.swift: a caption label
// over a semibold value with tabular digits on a tinted rounded surface. The
// value shrinks to fit rather than being cut. Every tile shares the object
// name `metricTile`, so a test can count a card's metrics without matching
// their text against the segment labels.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

Rectangle {
    id: tile

    objectName: "metricTile"
    property string label
    property string value

    implicitWidth: Theme.metricTileMinimumWidth
    implicitHeight: tileLayout.implicitHeight + 2 * Theme.metricTilePadding
    radius: Theme.metricTileRadius
    color: Theme.metricTileBackground

    ColumnLayout {
        id: tileLayout

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.metricTilePadding
        spacing: Theme.dashboardHeaderSpacing

        StyledLabel {
            Layout.fillWidth: true
            text: tile.label
            textStyle: Theme.captionFont
            secondary: true
            elide: Text.ElideRight
        }

        StyledLabel {
            Layout.fillWidth: true
            text: tile.value
            textStyle: Theme.title3Font
            weight: Theme.weightSemibold
            monospacedDigits: true
            maximumLineCount: 1
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Math.round(Theme.title3Font.pixelSize * Theme.metricMinimumScale)
        }
    }
}
