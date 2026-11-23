// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Reading Calendar card of the statistics dashboard, following
// `calendarSection` in Features/Bookshelf/StatisticsDashboardView.swift: one
// cell per day of the recent-year window, the selected day outlined and the
// current range tinted. The weeks scroll horizontally instead of shrinking the
// cells.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

StatisticsCard {
    id: card

    property var host
    Layout.fillWidth: true
    visible: StatisticsViewModel.hasStatistics

    readonly property string selectedKey: card.dateKey(StatisticsViewModel.selectedDate)
    readonly property string rangeStartKey: card.dateKey(StatisticsViewModel.rangeStart)
    readonly property string rangeEndKey: card.dateKey(StatisticsViewModel.rangeEnd)
    readonly property bool rangeHighlight: StatisticsViewModel.rangeMode !== 0

    function dateKey(date) {
        return date ? Qt.formatDate(date, "yyyy-MM-dd") : ""
    }

    function weekSlice(weekIndex) {
        return StatisticsViewModel.calendarDays.slice(weekIndex * 7, weekIndex * 7 + 7)
    }

    function scrollCalendarToSelection() {
        var index = StatisticsViewModel.calendarDays.findIndex(
            day => day.dateKey === card.selectedKey)
        if (index >= 0) {
            const x = Math.floor(index / 7) * (12 + weeksRow.spacing)
            calendarScroll.contentX = Math.min(
                Math.max(0, calendarScroll.contentWidth - calendarScroll.width),
                Math.max(0, x - calendarScroll.width / 2))
        }
    }

    onSelectedKeyChanged: Qt.callLater(card.scrollCalendarToSelection)
    Component.onCompleted: Qt.callLater(card.scrollCalendarToSelection)

    Connections {
        target: StatisticsViewModel
        function onDataChanged() {
            Qt.callLater(card.scrollCalendarToSelection)
        }
        function onViewChanged() {
            Qt.callLater(card.scrollCalendarToSelection)
        }
    }

    StatisticsSectionHeader {
        Layout.fillWidth: true
        title: qsTr("Reading Calendar")
        detail: card.host.rangeTitle()
    }

    Flickable {
        id: calendarScroll
        Layout.fillWidth: true
        Layout.preferredHeight: 7 * 12 + 6 * 4 + 18
        contentWidth: weeksRow.width
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        Row {
            id: weeksRow
            spacing: 4

            Repeater {
                model: Math.ceil(StatisticsViewModel.calendarDays.length / 7)
                delegate: Column {
                    id: weekColumn
                    required property int index
                    spacing: 4

                    Repeater {
                        model: card.weekSlice(weekColumn.index)
                        delegate: Rectangle {
                            id: dayCell
                            required property var modelData
                            width: 12
                            height: 12
                            radius: 3
                            color: dayCell.modelData.factor > 0
                                ? card.host.tint(Theme.accent, dayCell.modelData.factor)
                                : Theme.cardStroke
                            border.width: dayCell.selected ? 2
                                : dayCell.inRange ? 1 : 0
                            border.color: dayCell.selected
                                ? Theme.accent
                                : card.host.tint(Theme.accent, 0.45)

                            readonly property bool selected:
                                dayCell.modelData.dateKey === card.selectedKey
                            readonly property bool inRange:
                                card.rangeHighlight
                                && dayCell.modelData.dateKey >= card.rangeStartKey
                                && dayCell.modelData.dateKey <= card.rangeEndKey
                            readonly property string summary: qsTr("%1: %2")
                                .arg(card.host.fullDate(dayCell.modelData.dateKey))
                                .arg(card.host.formatCharacters(dayCell.modelData.characters))
                            Accessible.role: Accessible.Button
                            Accessible.name: dayCell.summary
                            Accessible.onPressAction: StatisticsViewModel.selectCalendarDay(
                                dayCell.modelData.dateKey)
                            ToolTip.visible: dayMouse.containsMouse
                            ToolTip.delay: 400
                            ToolTip.text: dayCell.summary

                            MouseArea {
                                id: dayMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: StatisticsViewModel.selectCalendarDay(
                                    dayCell.modelData.dateKey)
                            }
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Label {
            Layout.fillWidth: true
            text: qsTr("Selected: %1").arg(card.host.fullDate(card.selectedKey))
            color: Theme.accent
            font.bold: true
        }
        Label {
            visible: !StatisticsViewModel.selectedCalendarDay.hasRecords
            text: qsTr("No reading records")
            color: Theme.textSecondary
        }
    }

    RowLayout {
        Layout.fillWidth: true
        StatisticsMetricTile {
            label: qsTr("Characters")
            value: card.host.formatCharacters(StatisticsViewModel.selectedCalendarDay.characters)
            Layout.fillWidth: true
        }
        StatisticsMetricTile {
            label: qsTr("Duration")
            value: card.host.formatDuration(StatisticsViewModel.selectedCalendarDay.duration)
            Layout.fillWidth: true
        }
        StatisticsMetricTile {
            label: qsTr("Books")
            value: card.host.formatCharacters(StatisticsViewModel.selectedCalendarDay.books)
            Layout.fillWidth: true
        }
    }
}
