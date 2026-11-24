// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Shelf Comparison card of the statistics dashboard, following
// `shelfComparisonSection` in Features/Bookshelf/StatisticsDashboardView.swift:
// one row per shelf with book count, total and recorded characters, duration
// and average speed. The table scrolls horizontally rather than shrinking its
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

    StatisticsSectionHeader {
        Layout.fillWidth: true
        title: qsTr("Shelf Comparison")
        detail: card.host.rangeTitle()
    }

    // Column widths of the shelf table, in table order: shelf, books, total
    // characters, recorded characters, duration and average speed.
    readonly property var columnWidths: Theme.shelfTableColumnWidths

    function cells(row) {
        return [row.name, String(row.books), card.host.formatCharacters(row.totalCharacters),
                card.host.formatCharacters(row.characters), card.host.formatDuration(row.time),
                card.host.formatSpeed(row.speed)]
    }

    Flickable {
        id: shelfScroll
        Layout.fillWidth: true
        Layout.preferredHeight: shelfColumn.height
        contentWidth: shelfColumn.width
        contentHeight: shelfColumn.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        Column {
            id: shelfColumn
            width: Math.max(Theme.shelfTableMinimumWidth, shelfScroll.width)
            spacing: 0

            TableRow {
                header: true
                texts: [qsTr("Shelf"), qsTr("Books"), qsTr("Total Characters"),
                    qsTr("Recorded Characters"), qsTr("Duration"), qsTr("Avg Speed")]
            }

            Rectangle {
                width: parent.width
                height: Theme.hairlineWidth
                color: Theme.separator
            }

            StyledLabel {
                width: parent.width
                height: Theme.dashboardEmptyHeight
                visible: StatisticsViewModel.shelfComparison.length === 0
                text: qsTr("No reading records")
                textStyle: Theme.calloutFont
                secondary: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            Repeater {
                model: StatisticsViewModel.shelfComparison
                delegate: Column {
                    id: shelfRow
                    required property int index
                    required property var modelData
                    width: shelfColumn.width
                    spacing: 0

                    TableRow {
                        texts: card.cells(shelfRow.modelData)
                    }

                    Rectangle {
                        width: parent.width
                        height: Theme.hairlineWidth
                        color: Theme.separator
                        visible: shelfRow.index
                                 < StatisticsViewModel.shelfComparison.length - 1
                    }
                }
            }
        }
    }

    // One table line: the first column leading, the numbers trailing. The
    // header is a caption semibold secondary, the rows callout tabular digits.
    component TableRow: Row {
        id: tableRow

        property var texts: []
        property bool header: false

        width: shelfColumn.width
        spacing: Theme.tableColumnSpacing

        Repeater {
            model: tableRow.texts
            delegate: StyledLabel {
                required property int index
                required property string modelData

                width: card.columnWidths[index]
                topPadding: Theme.tableCellPadding
                bottomPadding: Theme.tableCellPadding
                text: modelData
                textStyle: tableRow.header ? Theme.captionFont : Theme.calloutFont
                weight: tableRow.header ? Theme.weightSemibold : textStyle.weight
                secondary: tableRow.header
                monospacedDigits: index > 0
                elide: Text.ElideRight
                horizontalAlignment: index > 0 ? Text.AlignRight : Text.AlignLeft
            }
        }
    }
}
