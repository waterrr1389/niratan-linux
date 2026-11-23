// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Book Ranking card of the statistics dashboard, following
// `bookRankingSection` in Features/Bookshelf/StatisticsDashboardView.swift:
// the ranking metric picker, one row per book and the More Books paging
// control. A row opens the book detail panel through the page.

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

    readonly property real maximum: {
        var rows = StatisticsViewModel.ranking
        var value = 1
        for (var index = 0; index < rows.length; ++index)
            value = Math.max(value, rows[index].value)
        return value
    }

    function valueText(row) {
        if (StatisticsViewModel.rankingMetric === 1)
            return card.host.formatDuration(row.time)
        if (StatisticsViewModel.rankingMetric === 2)
            return card.host.formatSpeed(row.speed)
        return card.host.formatCharacters(row.characters)
    }

    StatisticsSectionHeader {
        Layout.fillWidth: true
        title: qsTr("Book Ranking")
        detail: card.host.rangeTitle()
    }

    StatisticsSegments {
        labels: [qsTr("Characters"), qsTr("Duration"), qsTr("Speed")]
        currentIndex: StatisticsViewModel.rankingMetric
        onActivated: function (index) { StatisticsViewModel.rankingMetric = index }
    }

    Label {
        Layout.fillWidth: true
        Layout.preferredHeight: 100
        visible: StatisticsViewModel.ranking.length === 0
        text: qsTr("No reading records")
        color: Theme.textSecondary
        font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Repeater {
        model: StatisticsViewModel.ranking
        delegate: Item {
            id: rankRow
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: 54
            Accessible.role: Accessible.Button
            Accessible.name: rankRow.modelData.title
            Accessible.onPressAction: card.host.openBookDetails(rankRow.modelData)
            activeFocusOnTab: true
            Keys.onReturnPressed: card.host.openBookDetails(rankRow.modelData)
            Keys.onSpacePressed: card.host.openBookDetails(rankRow.modelData)
            Rectangle {
                anchors.fill: parent
                radius: 4
                color: card.host.tint(Theme.accent, 0.12)
                visible: rankRow.activeFocus || rankTap.hovered
            }
            HoverHandler { id: rankTap }
            TapHandler { onTapped: card.host.openBookDetails(rankRow.modelData) }

            RowLayout {
                anchors.fill: parent
                spacing: 12

                Item {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 48

                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: card.host.tint(Theme.textPrimary, 0.06)
                        visible: rankRow.modelData.cover.length === 0
                    }
                    Image {
                        anchors.fill: parent
                        source: rankRow.modelData.cover
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        visible: rankRow.modelData.cover.length > 0
                    }
                }

                Label {
                    Layout.preferredWidth: 120
                    text: rankRow.modelData.title
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.bold: true
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 9

                    Rectangle {
                        anchors.fill: parent
                        radius: 4.5
                        color: card.host.tint(Theme.textPrimary, 0.12)
                    }
                    Rectangle {
                        width: Math.max(parent.width
                                        * rankRow.modelData.value
                                        / card.maximum,
                                        rankRow.modelData.value > 0
                                        ? 2 : 0)
                        height: parent.height
                        radius: 4.5
                        color: card.host.tint(Theme.accent, 0.78)
                    }
                }

                Label {
                    Layout.preferredWidth: 84
                    text: card.valueText(rankRow.modelData)
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideRight
                }
            }
        }
    }

    Button {
        Layout.fillWidth: true
        visible: StatisticsViewModel.hasMoreBooks
        text: qsTr("More Books")
        onClicked: StatisticsViewModel.showMoreBooks()
    }
}
