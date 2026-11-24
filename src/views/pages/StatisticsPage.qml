// SPDX-License-Identifier: GPL-3.0-or-later
//
// The statistics dashboard, following Features/Bookshelf/StatisticsDashboardView.swift
// (`body`, `header`, `dashboardLayout`, `fullWidthTrendSection`, `trendSection`,
// `todaySection`, `targetSettingsSection`, `weekSection`, `calendarSection`,
// `selectedRangeSection`, `speedSummarySection`, `bookRankingSection`,
// `shelfComparisonSection`, `emptyDashboardState`,
// `loadingDashboardPlaceholder`, `dashboardLayoutWithLoadingOverlay` and
// `isInitialSnapshotLoading`) over the aggregation in
// StatisticsDashboardModels.swift. Values arrive already aggregated from
// StatisticsViewModel; this page only formats and draws them. The cards live
// in src/views/statistics/.
//
// The page owns the shell's toolbar row: PageHeader is a fixed band above the
// ScrollView and stays at the page top while the cards scroll. Decision D1
// keeps the back control in that band instead of a macOS window toolbar.
// `toolbarInset` leaves room for the floating sidebar toggle when the main
// sidebar is collapsed.
//
// The cards follow the Swift body: Range & Trend first and full width, then
// Today, Goal, This Week, Reading Calendar, Selected Range, Speed Summary,
// Book Ranking and Shelf Comparison. Selected Range is its own card, and the
// trend card keeps only its header, controls, hint and chart. Decision D2
// keeps the two-column adaptation at >= 840 px; the default window is below
// it, so its detail page is single column.
//
// Before the first snapshot has loaded the loading card is shown alone. The
// empty state waits for a load that found no days, and a later reload keeps
// the loaded dashboard visible (`isInitialSnapshotLoading`).
//
// The calendar and shelf table scroll horizontally instead of shrinking their
// cells. Every colour is a Theme token; the data series is Theme.accent. Type,
// spacing, controls and the book details editor come from the kit and the
// dashboard tokens of the theme, the same roles other pages use.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: page

    signal backRequested()

    property real toolbarInset: 0
    property int chartStyle: 0

    readonly property int outerMargin: Theme.dashboardMargin
    readonly property int cardSpacing: Theme.dashboardCardSpacing
    readonly property bool wideLayout: page.width >= Theme.dashboardWideWidth
    readonly property var weekdayLabels: [
        qsTr("Mon"), qsTr("Tue"), qsTr("Wed"), qsTr("Thu"),
        qsTr("Fri"), qsTr("Sat"), qsTr("Sun")
    ]

    function tint(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

    function formatCharacters(value) {
        return Number(value).toLocaleString(Qt.locale(), "f", 0)
    }

    function formatDuration(seconds) {
        var minutes = Math.max(Math.round(seconds / 60), 0)
        if (minutes < 60)
            return qsTr("%1m").arg(minutes)
        var hours = Math.floor(minutes / 60)
        var rest = minutes % 60
        if (rest === 0)
            return qsTr("%1h").arg(hours)
        return qsTr("%1h %2m").arg(hours).arg(rest)
    }

    function formatSpeed(value) {
        return value < 0 ? "-" : qsTr("%1/h").arg(page.formatCharacters(value))
    }

    function fullDate(key) {
        return Qt.formatDate(new Date(key + "T12:00:00"), Qt.locale().dateFormat(Locale.LongFormat))
    }

    function dailyTargetText() {
        if (StatisticsViewModel.targetType === 1)
            return page.formatDuration(StatisticsViewModel.durationTargetMinutes * 60)
        return qsTr("%1 chars").arg(page.formatCharacters(StatisticsViewModel.characterTarget))
    }

    function rangeTitle() {
        var mode = StatisticsViewModel.rangeMode
        if (mode === 0)
            return qsTr("Recent year")
        if (mode === 1)
            return Qt.formatDate(StatisticsViewModel.rangeStart, "yyyy-M")
        if (mode === 2)
            return qsTr("%1-%2").arg(Qt.formatDate(StatisticsViewModel.rangeStart, "M/d"))
                                    .arg(Qt.formatDate(StatisticsViewModel.rangeEnd, "M/d"))
        return Qt.formatDate(StatisticsViewModel.rangeStart, "M/d")
    }

    function openBookDetails(item) {
        bookDetails.showBook(item)
    }

    StatisticsBookDetailsDialog {
        id: bookDetails

        host: page
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBackground
    }

    PageHeader {
        id: headerBand

        objectName: "statisticsHeader"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        leadingInset: page.toolbarInset
        title: qsTr("Statistics")
        subtitle: page.rangeTitle()
        onBackRequested: page.backRequested()
    }

    ScrollView {
        id: scroll

        objectName: "statisticsScroll"
        anchors.top: headerBand.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        contentWidth: content.width + page.outerMargin * 2
        contentHeight: content.height + page.outerMargin * 2
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            id: content

            x: page.outerMargin
            y: page.outerMargin
            width: Math.max(page.width - page.outerMargin * 2, Theme.dashboardMinimumWidth)
            spacing: page.cardSpacing

            StatisticsCard {
                Layout.fillWidth: true
                visible: StatisticsViewModel.hasCorruptStatistics
                StyledLabel {
                    Layout.fillWidth: true
                    text: qsTr("Some statistics are temporarily unavailable.")
                    wrapMode: Text.WordWrap
                }
            }

            StatisticsCard {
                objectName: "loadingCard"
                Layout.fillWidth: true
                visible: !StatisticsViewModel.hasLoaded
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.rowSpacing

                    StyledLabel {
                        Layout.fillWidth: true
                        text: qsTr("Loading Statistics")
                        secondary: true
                    }
                    BusyIndicator { running: !StatisticsViewModel.hasLoaded }
                }
                Repeater {
                    model: 3
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.dashboardPlaceholderHeight
                        radius: Theme.metricTileRadius
                        color: Theme.metricTileBackground
                    }
                }
            }

            StatisticsTrendCard {
                objectName: "trendCard"
                host: page
            }

            GridLayout {
                Layout.fillWidth: true
                columns: page.wideLayout ? 2 : 1
                columnSpacing: page.cardSpacing
                rowSpacing: page.cardSpacing

                StatisticsTodayCard {
                    objectName: "todayCard"
                    host: page
                }
                StatisticsGoalCard {
                    objectName: "goalCard"
                    host: page
                    visible: StatisticsViewModel.hasLoaded
                }
                StatisticsWeekCard {
                    objectName: "weekCard"
                    host: page
                }
            }

            StatisticsCalendarCard {
                objectName: "calendarCard"
                host: page
            }

            StatisticsSelectedRangeCard {
                objectName: "selectedRangeCard"
                host: page
            }

            GridLayout {
                Layout.fillWidth: true
                columns: page.wideLayout ? 2 : 1
                columnSpacing: page.cardSpacing
                rowSpacing: page.cardSpacing

                StatisticsSpeedCard {
                    objectName: "speedCard"
                    host: page
                }
                StatisticsRankingCard {
                    objectName: "rankingCard"
                    host: page
                }
            }

            StatisticsShelfCard {
                objectName: "shelfCard"
                host: page
            }

            StatisticsCard {
                objectName: "emptyStateCard"
                Layout.fillWidth: true
                visible: StatisticsViewModel.hasLoaded && !StatisticsViewModel.hasStatistics

                StatisticsSectionHeader {
                    Layout.fillWidth: true
                    title: qsTr("No Reading Records")
                }
                StyledLabel {
                    Layout.fillWidth: true
                    text: qsTr("Open a book and start reading with statistics enabled.")
                    textStyle: Theme.calloutFont
                    secondary: true
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component.onCompleted: StatisticsViewModel.refresh()
}
