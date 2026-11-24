// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Today card of the statistics dashboard, following `todaySection` in
// Features/Bookshelf/StatisticsDashboardView.swift: the goal ring beside the
// day's duration, characters, speed and streak. The values are
// StatisticsViewModel.today.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

StatisticsCard {
    id: card

    property var host
    Layout.fillWidth: true
    Layout.alignment: Qt.AlignTop
    visible: StatisticsViewModel.hasStatistics

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingLg

        GoalRing {
            Layout.preferredWidth: Theme.goalRingSize
            Layout.preferredHeight: Theme.goalRingSize
            percent: StatisticsViewModel.today.targetPercent
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.dashboardCardContentSpacing

            StatisticsSectionHeader {
                Layout.fillWidth: true
                title: qsTr("Today")
                detail: qsTr("Goal: %1").arg(card.host.dailyTargetText())
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.dashboardTileSpacing
                rowSpacing: Theme.dashboardTileSpacing

                StatisticsMetricTile {
                    Layout.fillWidth: true
                    label: qsTr("Duration")
                    value: card.host.formatDuration(StatisticsViewModel.today.duration)
                }
                StatisticsMetricTile {
                    Layout.fillWidth: true
                    label: qsTr("Characters")
                    value: card.host.formatCharacters(StatisticsViewModel.today.characters)
                }
                StatisticsMetricTile {
                    Layout.fillWidth: true
                    label: qsTr("Speed")
                    value: card.host.formatSpeed(StatisticsViewModel.today.speed)
                }
                StatisticsMetricTile {
                    Layout.fillWidth: true
                    label: qsTr("Streak")
                    value: qsTr("%1 days").arg(StatisticsViewModel.today.streak)
                }
            }
        }
    }

    component GoalRing: Item {
        id: ring
        property int percent: 0
        property color accent: Theme.accent
        property color track: Theme.trackBackground

        onPercentChanged: ringCanvas.requestPaint()
        onWidthChanged: ringCanvas.requestPaint()
        onHeightChanged: ringCanvas.requestPaint()

        Canvas {
            id: ringCanvas
            anchors.fill: parent

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, ringCanvas.width, ringCanvas.height)
                var lineWidth = Theme.goalRingLineWidth
                var center = Math.min(ringCanvas.width, ringCanvas.height) / 2
                var radius = Math.max(center - lineWidth / 2 - 1, 0)
                ctx.lineWidth = lineWidth
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.arc(center, center, radius, 0, Math.PI * 2)
                ctx.strokeStyle = ring.track
                ctx.stroke()

                var ratio = Math.max(0, Math.min(ring.percent / 100, 1))
                if (ratio > 0) {
                    ctx.beginPath()
                    ctx.arc(center, center, radius, -Math.PI / 2,
                            -Math.PI / 2 + Math.PI * 2 * ratio)
                    ctx.strokeStyle = ring.accent
                    ctx.stroke()
                }
            }
        }

        Connections {
            target: Theme
            function onDarkChanged() {
                ringCanvas.requestPaint()
            }
            function onAccentChanged() {
                ringCanvas.requestPaint()
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: Theme.metricSpacing

            StyledLabel {
                anchors.horizontalCenter: parent.horizontalCenter
                text: ring.percent + "%"
                textStyle: Theme.title2Font
                weight: Theme.weightBold
                monospacedDigits: true
            }
            StyledLabel {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Goal")
                textStyle: Theme.caption2Font
                secondary: true
            }
        }
    }
}
