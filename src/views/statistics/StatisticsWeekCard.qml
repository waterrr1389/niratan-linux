// SPDX-License-Identifier: GPL-3.0-or-later
//
// The This Week card of the statistics dashboard, following `weekSection` in
// Features/Bookshelf/StatisticsDashboardView.swift: the week's totals and the
// seven novelty chips. Decision D3 keeps three-letter weekday labels instead
// of Swift's narrow single-letter symbols.

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
    Layout.columnSpan: card.host && card.host.wideLayout ? 2 : 1
    visible: StatisticsViewModel.hasStatistics

    StatisticsSectionHeader {
        Layout.fillWidth: true
        title: qsTr("This Week")
        detail: qsTr("%1/7 days").arg(StatisticsViewModel.week.metTargetDays)
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 8
        rowSpacing: 8

        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Duration")
            value: card.host.formatDuration(StatisticsViewModel.week.duration)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Characters")
            value: card.host.formatCharacters(StatisticsViewModel.week.characters)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Avg Characters")
            value: card.host.formatCharacters(StatisticsViewModel.week.averageCharacters)
        }
        StatisticsMetricTile {
            Layout.fillWidth: true
            label: qsTr("Speed")
            value: card.host.formatSpeed(StatisticsViewModel.week.speed)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Repeater {
            model: StatisticsViewModel.week.days
            delegate: Item {
                id: chip
                required property int index
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 50

                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    color: chip.modelData.metTarget
                        ? card.host.tint(Theme.accent, 0.14)
                        : Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                                  Theme.textPrimary.b, 0.06)
                    border.width: chip.modelData.isToday ? 1.5 : 0
                    border.color: card.host.tint(Theme.accent, 0.5)
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: card.host.weekdayLabels[chip.index]
                        color: chip.modelData.metTarget
                            ? Theme.accent
                            : Theme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: chip.modelData.percent >= 0
                            ? chip.modelData.percent + "%"
                            : "-"
                        color: chip.modelData.metTarget
                            ? Theme.accent
                            : Theme.textSecondary
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}
