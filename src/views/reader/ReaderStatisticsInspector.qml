// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reading Statistics side panel, following
// Features/Reader/StatisticsView.swift (ReaderStatisticsContentView.body,
// sessionHero, heroMetric, bookProgress, remainingRow, historyTable and
// historyRow) with its header from NativeMac/NativeReaderInspector.swift and
// the accessory play/pause control of NativeMac/NativeReaderView.swift
// (NativeReaderStatisticsSheet).
//
// The panel floats over the chapter without leaving the book and closes
// through its close control or Escape. ReaderPage opens it from the trailing
// controls; while it is open the reader stays a statistics tracking context.
// The header's play/pause control is the same manual tracking toggle as the T
// shortcut.
//
// Every number, duration and "—" comes from
// ReaderViewModel.statisticsInspector, which
// models/statistics/reader_statistics.h formats, so this file only lays the
// values out. The count column is named for the profile's unit: raw characters
// for Japanese, approximate words for English. The layout order is session,
// book progress, then the history table; the SF Symbol icons beside the two
// remaining-time rows are omitted because the row labels carry the meaning.
// The content scrolls in a PanelScaffold inset 18 at the sides and bottom,
// stops at its bounds and shows a scroll bar while it overflows. The hero
// count keeps one line by eliding rather than SwiftUI's minimumScaleFactor.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: inspector

    signal closeRequested()

    readonly property var stats: ReaderViewModel.statisticsInspector || ({})
    readonly property var sessionPeriod: stats.session ? stats.session : ({})
    readonly property var todayPeriod: stats.today ? stats.today : ({})
    readonly property var allTimePeriod: stats.allTime ? stats.allTime : ({})
    readonly property bool english: stats.english === true
    readonly property bool tracking: ReaderViewModel.tracking
    readonly property string countLabel: english ? qsTr("Approximate Words Read")
                                                : qsTr("Characters Read")

    // historyRow's value cells: callout tabular digits, trailing.
    component HistoryValue: StyledLabel {
        Layout.alignment: Qt.AlignRight
        textStyle: Theme.calloutFont
        monospacedDigits: true
        elide: Text.ElideRight
    }

    function text(value) {
        return value ? value : "";
    }

    spacing: 0

    PanelHeader {
        Layout.fillWidth: true
        title: qsTr("Statistics")
        onCloseRequested: inspector.closeRequested()

        IconButton {
            icon: inspector.tracking ? "pause.fill" : "play.fill"
            diameter: Theme.closeButtonDiameter
            iconSize: Theme.closeButtonGlyph
            accessibleName: inspector.tracking ? qsTr("Pause Tracking")
                                               : qsTr("Start Tracking")
            onClicked: ReaderViewModel.toggleTracking()
        }
    }

    PanelScaffold {
        Layout.fillWidth: true
        Layout.fillHeight: true
        compact: true
        persistentScrollBar: true
        topPadding: 0
        horizontalPadding: Theme.panelHeaderPaddingHorizontal
        bottomPadding: Theme.panelHeaderPaddingHorizontal
        spacing: Theme.statisticsSectionSpacing

        HeroCard {
            objectName: "sessionHero"
            active: inspector.tracking

            Item {
                implicitHeight: Math.max(status.implicitHeight, sessionLabel.implicitHeight)

                Row {
                    id: status

                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.statusDotSpacing

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Theme.statusDotSize
                        height: Theme.statusDotSize
                        radius: Theme.statusDotRadius
                        color: inspector.tracking ? Theme.popupSuccess : Theme.textSecondary
                    }

                    StyledLabel {
                        text: inspector.tracking ? qsTr("Tracking") : qsTr("Paused")
                        textStyle: Theme.captionFont
                        weight: Theme.weightSemibold
                        secondary: !inspector.tracking
                    }
                }

                StyledLabel {
                    id: sessionLabel

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Session")
                    textStyle: Theme.captionFont
                    secondary: true
                }
            }

            Column {
                StyledLabel {
                    width: parent.width
                    text: inspector.text(inspector.sessionPeriod.characters)
                    textStyle: Theme.heroCountFont
                    monospacedDigits: true
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                StyledLabel {
                    width: parent.width
                    text: inspector.countLabel
                    textStyle: Theme.captionFont
                    secondary: true
                }
            }

            RowLayout {
                spacing: 0

                MetricLabel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    value: inspector.text(inspector.sessionPeriod.speed)
                    label: qsTr("Reading Speed")
                }

                Rectangle {
                    Layout.preferredWidth: Theme.hairlineWidth
                    Layout.preferredHeight: Theme.metricDividerHeight
                    color: Theme.separator
                }

                MetricLabel {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.leftMargin: Theme.metricLeading
                    value: inspector.text(inspector.sessionPeriod.duration)
                    label: qsTr("Reading Time")
                }
            }
        }

        Column {
            spacing: Theme.statisticsGroupSpacing

            Item {
                width: parent.width
                implicitHeight: Math.max(progressTitle.implicitHeight, percent.implicitHeight)

                SectionTitle {
                    id: progressTitle

                    anchors.left: parent.left
                    anchors.right: percent.left
                    anchors.rightMargin: Theme.spacingSm
                    text: qsTr("Book Progress")
                }

                StyledLabel {
                    id: percent

                    x: parent.width - width
                    text: inspector.text(inspector.stats.bookPercent)
                    textStyle: Theme.captionFont
                    weight: Theme.weightSemibold
                    monospacedDigits: true
                }
            }

            ProgressTrack {
                width: parent.width
                value: inspector.stats.bookFraction ? inspector.stats.bookFraction : 0
            }

            ValueRow {
                width: parent.width
                label: qsTr("Time to finish Book")
                value: inspector.text(inspector.stats.bookRemaining)
            }

            ValueRow {
                width: parent.width
                label: qsTr("Time to finish Chapter")
                value: inspector.text(inspector.stats.chapterRemaining)
            }
        }

        Column {
            spacing: Theme.statisticsHistorySpacing

            SectionTitle {
                width: parent.width
                text: qsTr("History")
            }

            GridLayout {
                width: parent.width
                columns: 4
                columnSpacing: Theme.tableColumnSpacing
                rowSpacing: Theme.tableRowSpacing

                Item {
                    Layout.fillWidth: true
                }

                Repeater {
                    model: [inspector.countLabel, qsTr("Reading Speed"), qsTr("Reading Time")]

                    delegate: StyledLabel {
                        required property string modelData

                        Layout.alignment: Qt.AlignRight
                        text: modelData
                        textStyle: Theme.caption2Font
                        secondary: true
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.columnSpan: 4
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.hairlineWidth
                    color: Theme.separator
                }

                StyledLabel {
                    text: qsTr("Today")
                    textStyle: Theme.calloutFont
                    weight: Theme.weightMedium
                }
                HistoryValue { text: inspector.text(inspector.todayPeriod.characters) }
                HistoryValue { text: inspector.text(inspector.todayPeriod.speed) }
                HistoryValue { text: inspector.text(inspector.todayPeriod.duration) }
                StyledLabel {
                    text: qsTr("All Time")
                    textStyle: Theme.calloutFont
                    weight: Theme.weightMedium
                }
                HistoryValue { text: inspector.text(inspector.allTimePeriod.characters) }
                HistoryValue { text: inspector.text(inspector.allTimePeriod.speed) }
                HistoryValue { text: inspector.text(inspector.allTimePeriod.duration) }
            }
        }
    }
}
