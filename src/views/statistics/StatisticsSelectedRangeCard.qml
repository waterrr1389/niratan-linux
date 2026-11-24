// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Selected Range card of the statistics dashboard, following
// `selectedRangeSection` in Features/Bookshelf/StatisticsDashboardView.swift:
// its own card between the calendar and the speed summary, carrying the range
// title and the duration, characters, speed and progress metrics.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

StatisticsCard {
    id: card

    property var host
    Layout.fillWidth: true
    visible: StatisticsViewModel.hasStatistics

    StatisticsSectionHeader {
        Layout.fillWidth: true
        title: qsTr("Selected Range")
        detail: card.host.rangeTitle()
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: Theme.dashboardTileSpacing
        rowSpacing: Theme.dashboardTileSpacing

        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Duration")
            value: card.host.formatDuration(StatisticsViewModel.selectedRange.duration)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Characters")
            value: card.host.formatCharacters(StatisticsViewModel.selectedRange.characters)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Speed")
            value: card.host.formatSpeed(StatisticsViewModel.selectedRange.speed)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: StatisticsViewModel.rangeMode === 3
                ? qsTr("Goal Progress") : qsTr("Days Met")
            value: StatisticsViewModel.rangeMode === 3
                ? StatisticsViewModel.selectedRange.targetProgressPercent + "%"
                : qsTr("%1 days").arg(StatisticsViewModel.selectedRange.targetDays)
        }
    }
}
