// SPDX-License-Identifier: GPL-3.0-or-later
//
// The card surface of the statistics dashboard, following the
// `nativeGlassCardSurface` cards in
// Features/Bookshelf/StatisticsDashboardView.swift (StatisticsDashboardCard):
// the kit's GlassSurface holding a full-width, leading-aligned column padded
// by the dashboard's card padding on every edge.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

GlassSurface {
    id: card

    default property alias cardContent: body.data

    implicitWidth: frame.implicitWidth
    implicitHeight: frame.implicitHeight

    ColumnLayout {
        id: frame

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 0

        ColumnLayout {
            id: body

            Layout.fillWidth: true
            Layout.margins: Theme.dashboardCardPadding
            spacing: Theme.dashboardCardContentSpacing
        }
    }
}
