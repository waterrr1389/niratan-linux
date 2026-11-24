// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Book Ranking card of the statistics dashboard, following
// `bookRankingSection` in Features/Bookshelf/StatisticsDashboardView.swift:
// the ranking metric picker, one row per book and the More Books paging
// control. A row opens the book detail panel through the page.

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

    StyledLabel {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.dashboardEmptyHeight
        visible: StatisticsViewModel.ranking.length === 0
        text: qsTr("No reading records")
        textStyle: Theme.calloutFont
        secondary: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Repeater {
        model: StatisticsViewModel.ranking
        delegate: Item {
            id: rankRow
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: Theme.rankingRowHeight
            Accessible.role: Accessible.Button
            Accessible.name: rankRow.modelData.title
            Accessible.onPressAction: card.host.openBookDetails(rankRow.modelData)
            activeFocusOnTab: true
            Keys.onReturnPressed: card.host.openBookDetails(rankRow.modelData)
            Keys.onSpacePressed: card.host.openBookDetails(rankRow.modelData)
            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: -Theme.spacingXs
                anchors.rightMargin: -Theme.spacingXs
                radius: Theme.controlRadius
                color: rankTap.pressed ? Theme.pressTint : Theme.hoverTint
                visible: rankRow.activeFocus || rankHover.hovered
            }
            HoverHandler {
                id: rankHover
                cursorShape: Qt.PointingHandCursor
            }
            TapHandler {
                id: rankTap
                onTapped: card.host.openBookDetails(rankRow.modelData)
            }

            RowLayout {
                anchors.fill: parent
                spacing: Theme.rowSpacing

                Item {
                    Layout.preferredWidth: Theme.rankingCoverWidth
                    Layout.preferredHeight: Theme.rankingCoverHeight

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.summaryCoverRadius
                        color: Theme.coverPlaceholder
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

                StyledLabel {
                    Layout.preferredWidth: Theme.rankingTitleWidth
                    text: rankRow.modelData.title
                    textStyle: Theme.calloutFont
                    weight: Theme.weightSemibold
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                }

                ProgressTrack {
                    Layout.fillWidth: true
                    value: rankRow.modelData.value / card.maximum
                }

                StyledLabel {
                    Layout.preferredWidth: Theme.rankingValueWidth
                    text: card.valueText(rankRow.modelData)
                    textStyle: Theme.calloutFont
                    secondary: true
                    monospacedDigits: true
                    horizontalAlignment: Text.AlignRight
                    maximumLineCount: 1
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: Math.round(Theme.calloutFont.pixelSize
                                                 * Theme.metricMinimumScale)
                }
            }
        }
    }

    FormButton {
        Layout.fillWidth: true
        visible: StatisticsViewModel.hasMoreBooks
        text: qsTr("More Books")
        onClicked: StatisticsViewModel.showMoreBooks()
    }
}
