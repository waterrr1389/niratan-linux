// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Speed Summary card of the statistics dashboard, following
// `speedSummarySection` in Features/Bookshelf/StatisticsDashboardView.swift:
// the weighted, typical and recent speeds, the change against the first
// fourteen days and the fastest and slowest recorded days.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

StatisticsCard {
    id: card

    property var host
    Layout.fillWidth: true
    Layout.alignment: Qt.AlignTop
    visible: StatisticsViewModel.hasStatistics

    function formatSpeedDay(dateKey, speed) {
        if (dateKey.length === 0 || speed < 0)
            return "-"
        var parts = dateKey.split("-")
        var monthDay = parts.length === 3 ? Number(parts[1]) + "/" + Number(parts[2]) : dateKey
        return qsTr("%1 · %2").arg(monthDay).arg(card.host.formatSpeed(speed))
    }

    function formatSignedPercent(value) {
        return value > 0 ? "+" + value + "%" : value + "%"
    }

    StatisticsSectionHeader {
        Layout.fillWidth: true
        title: qsTr("Speed Summary")
        detail: card.host.rangeTitle()
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 8
        rowSpacing: 8

        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Weighted Avg")
            value: card.host.formatSpeed(StatisticsViewModel.speed.weighted)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Typical Day")
            value: card.host.formatSpeed(StatisticsViewModel.speed.typical)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Last 7 Active Days")
            value: card.host.formatSpeed(StatisticsViewModel.speed.recent)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Change vs First 14")
            value: StatisticsViewModel.speed.hasChange
                ? card.formatSignedPercent(StatisticsViewModel.speed.change)
                : "-"
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Fastest Day")
            value: card.formatSpeedDay(StatisticsViewModel.speed.bestDate,
                                       StatisticsViewModel.speed.bestSpeed)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Slowest Day")
            value: card.formatSpeedDay(StatisticsViewModel.speed.worstDate,
                                       StatisticsViewModel.speed.worstSpeed)
        }
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("Speed ignores reading samples under 1 minute.")
        color: Theme.textSecondary
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
}
