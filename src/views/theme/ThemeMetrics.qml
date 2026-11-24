// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plain geometry tokens of the statistics surfaces that Theme extends: the
// dashboard of Features/Bookshelf/StatisticsDashboardView.swift (breakpoints,
// goal ring, week chips, ranking rows, shelf table, reading calendar, trend
// chart and the book details editor) and the minimumScaleFactor values of
// Features/Reader/StatisticsView.swift and the dashboard metrics. Views never
// use this type directly; they read every token through the Theme singleton.
// Tokens derived from colours or from the shared spacing scale stay in
// Theme.qml.

import QtQml

QtObject {
    // Dashboard layout: the two-column breakpoint, the narrowest content
    // column, metric tiles, the goal ring, week chips and empty placeholders.
    readonly property int dashboardWideWidth: 840
    readonly property int dashboardMinimumWidth: 240
    readonly property int metricTileMinimumWidth: 96
    readonly property int goalRingSize: 108
    readonly property int goalRingLineWidth: 13
    readonly property int weekChipHeight: 50
    readonly property int weekChipSpacing: 6
    readonly property int weekChipRadius: 10
    readonly property real weekChipTodayStroke: 1.5
    readonly property real weekChipMetOpacity: 0.14
    readonly property real weekChipTodayOpacity: 0.5
    readonly property int dashboardPlaceholderHeight: 46
    readonly property int dashboardEmptyHeight: 100

    // Book ranking rows: cover, title and value columns.
    readonly property int rankingRowHeight: 54
    readonly property int rankingCoverWidth: 36
    readonly property int rankingCoverHeight: 48
    readonly property int rankingTitleWidth: 120
    readonly property int rankingValueWidth: 84

    // The book details editor: its size, two-column breakpoint, lists and
    // session fields.
    readonly property int bookDetailsWidth: 780
    readonly property int bookDetailsHeight: 560
    readonly property int bookDetailsTwoColumnWidth: 620
    readonly property int bookDetailsListWidth: 270
    readonly property int bookDetailsListHeight: 160
    readonly property int bookDetailsSessionsHeight: 120
    readonly property int bookDetailsCountFieldWidth: 120
    readonly property int bookDetailsTimeFieldWidth: 70

    // The trend chart: bar, gap and axis label widths per time grain (day,
    // week, month), then the chart's own heights, radii and marks.
    readonly property var trendBarWidths: [12, 22, 28]
    readonly property var trendBarSpacings: [5, 8, 10]
    readonly property var trendLabelWidths: [42, 66, 58]
    readonly property int trendChartHeight: 224
    readonly property int trendAxisWidth: 56
    readonly property int trendLabelHeight: 22
    readonly property int trendEmptyHeight: 150
    readonly property int trendBarRadius: 5
    readonly property real trendBarOpacity: 0.86
    readonly property int trendMinimumBarHeight: 6
    readonly property int trendEmptyBarHeight: 2
    readonly property int trendLineWidth: 2
    readonly property int trendPointSize: 4

    // The shelf comparison table, in column order: shelf, books, total
    // characters, recorded characters, duration and average speed.
    readonly property var shelfTableColumnWidths: [160, 56, 112, 118, 82, 92]
    readonly property int shelfTableMinimumWidth: 680
    readonly property int tableCellPadding: 6

    // The reading calendar's day cells.
    readonly property int calendarCellSize: 12
    readonly property int calendarCellSpacing: 4
    readonly property int calendarCellRadius: 3
    readonly property int calendarSelectedStroke: 2
    readonly property real calendarRangeOpacity: 0.45
    readonly property int calendarScrollBarRoom: 18

    // StatisticsView.swift minimumScaleFactor: the session count, the hero
    // metrics and the history cells shrink to fit before they would be cut.
    readonly property real statisticsHeroMinimumScale: 0.6
    readonly property real metricMinimumScale: 0.7
    readonly property real statisticsHistoryMinimumScale: 0.75
}
