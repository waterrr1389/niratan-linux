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
            width: Math.max(680, shelfScroll.width)
            spacing: 0

            Row {
                width: parent.width
                spacing: 10

                TableCell {
                    cellWidth: 160
                    text: qsTr("Shelf")
                    color: Theme.textSecondary
                    font.bold: true
                    font.pixelSize: 11
                }
                TableCell {
                    cellWidth: 56
                    rightAligned: true
                    text: qsTr("Books")
                    color: Theme.textSecondary
                    font.bold: true
                    font.pixelSize: 11
                }
                TableCell {
                    cellWidth: 112
                    rightAligned: true
                    text: qsTr("Total Characters")
                    color: Theme.textSecondary
                    font.bold: true
                    font.pixelSize: 11
                }
                TableCell {
                    cellWidth: 118
                    rightAligned: true
                    text: qsTr("Recorded Characters")
                    color: Theme.textSecondary
                    font.bold: true
                    font.pixelSize: 11
                }
                TableCell {
                    cellWidth: 82
                    rightAligned: true
                    text: qsTr("Duration")
                    color: Theme.textSecondary
                    font.bold: true
                    font.pixelSize: 11
                }
                TableCell {
                    cellWidth: 92
                    rightAligned: true
                    text: qsTr("Avg Speed")
                    color: Theme.textSecondary
                    font.bold: true
                    font.pixelSize: 11
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.cardStroke
            }

            Label {
                width: parent.width
                height: 96
                visible: StatisticsViewModel.shelfComparison.length === 0
                text: qsTr("No reading records")
                color: Theme.textSecondary
                font.pixelSize: 13
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

                    Row {
                        width: parent.width
                        spacing: 10

                        TableCell {
                            cellWidth: 160
                            text: shelfRow.modelData.name
                        }
                        TableCell {
                            cellWidth: 56
                            rightAligned: true
                            text: String(shelfRow.modelData.books)
                        }
                        TableCell {
                            cellWidth: 112
                            rightAligned: true
                            text: card.host.formatCharacters(
                                shelfRow.modelData.totalCharacters)
                        }
                        TableCell {
                            cellWidth: 118
                            rightAligned: true
                            text: card.host.formatCharacters(
                                shelfRow.modelData.characters)
                        }
                        TableCell {
                            cellWidth: 82
                            rightAligned: true
                            text: card.host.formatDuration(shelfRow.modelData.time)
                        }
                        TableCell {
                            cellWidth: 92
                            rightAligned: true
                            text: card.host.formatSpeed(shelfRow.modelData.speed)
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.cardStroke
                        visible: shelfRow.index
                                 < StatisticsViewModel.shelfComparison.length - 1
                    }
                }
            }
        }
    }

    component TableCell: Label {
        property int cellWidth: 100
        property bool rightAligned: false
        width: cellWidth
        topPadding: 6
        bottomPadding: 6
        color: Theme.textPrimary
        font.pixelSize: 12
        elide: Text.ElideRight
        horizontalAlignment: rightAligned ? Text.AlignRight : Text.AlignLeft
    }
}
