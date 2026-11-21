// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reading Statistics inspector for the reader page, following
// Features/Reader/StatisticsView.swift (ReaderStatisticsContentView.body,
// sessionHero, heroMetric, bookProgress, remainingRow, historyTable and
// historyRow) with its header and section titles from
// NativeMac/NativeReaderView.swift (NativeReaderStatisticsSheet,
// NativeReaderInspectorHeader, NativeReaderInspectorSectionTitle).
//
// The drawer opens over the chapter without leaving the book and closes
// through its close control or Escape. ReaderPage opens it from the toolbar;
// the trailing play/pause control is the same manual tracking toggle as the
// T shortcut.
//
// Every number, duration and "—" comes from
// ReaderViewModel.statisticsInspector, which
// models/statistics/reader_statistics.h formats, so this file only lays the
// values out. The count column is named for the profile's unit: raw characters
// for Japanese, approximate words for English.
//
// The SF Symbol icons beside the two remaining-time rows are omitted; the row
// labels carry the meaning. The rest keeps the source order: session, book
// progress, then the history table.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Drawer {
    id: inspector

    edge: Qt.RightEdge
    modal: true
    padding: 0
    width: Math.min(380, parent ? parent.width * 0.65 : 380)
    height: parent ? parent.height : 0

    readonly property var stats: ReaderViewModel.statisticsInspector
    readonly property var sessionPeriod: stats.session ? stats.session : ({})
    readonly property var todayPeriod: stats.today ? stats.today : ({})
    readonly property var allTimePeriod: stats.allTime ? stats.allTime : ({})
    readonly property bool english: stats.english === true
    readonly property bool tracking: ReaderViewModel.tracking
    readonly property string countLabel: english ? qsTr("Approximate Words Read")
                                                : qsTr("Characters Read")

    background: Rectangle {
        color: Theme.cardBackground
        border.width: Theme.cardBorderWidth
        border.color: Theme.cardStroke
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLg
            spacing: Theme.spacingSm

            Label {
                Layout.fillWidth: true
                text: qsTr("Statistics")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontTitle
                font.bold: true
            }
            ToolButton {
                text: inspector.tracking ? "\u23f8" : "\u25b6"
                ToolTip.visible: hovered
                ToolTip.text: inspector.tracking ? qsTr("Pause Tracking")
                                                 : qsTr("Start Tracking")
                Accessible.name: ToolTip.text
                onClicked: ReaderViewModel.toggleTracking()
            }
            ToolButton {
                text: "\u00d7"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Close")
                Accessible.name: qsTr("Close")
                onClicked: inspector.close()
            }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: scroll.availableWidth
                spacing: Theme.spacingXl

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingLg
                    Layout.rightMargin: Theme.spacingLg
                    implicitHeight: hero.implicitHeight + 2 * Theme.spacingLg
                    radius: 16
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                   inspector.tracking ? 0.12 : 0.06)

                    ColumnLayout {
                        id: hero
                        anchors.fill: parent
                        anchors.margins: Theme.spacingLg
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Rectangle {
                                Layout.preferredWidth: 7
                                Layout.preferredHeight: 7
                                radius: 3.5
                                color: inspector.tracking ? Theme.popupSuccess
                                                          : Theme.textSecondary
                            }
                            Label {
                                text: inspector.tracking ? qsTr("Tracking")
                                                         : qsTr("Paused")
                                color: inspector.tracking ? Theme.textPrimary
                                                          : Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: qsTr("Session")
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }
                        }

                        ColumnLayout {
                            spacing: 0
                            Label {
                                text: inspector.sessionPeriod.characters
                                      ? inspector.sessionPeriod.characters : ""
                                color: Theme.textPrimary
                                font.pixelSize: 40
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                text: inspector.countLabel
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 14

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Label {
                                    text: inspector.sessionPeriod.speed
                                          ? inspector.sessionPeriod.speed : ""
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontBody
                                    font.bold: true
                                }
                                Label {
                                    text: qsTr("Reading Speed")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }
                            }
                            Rectangle {
                                Layout.preferredWidth: 1
                                Layout.preferredHeight: 28
                                color: Theme.cardStroke
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Label {
                                    text: inspector.sessionPeriod.duration
                                          ? inspector.sessionPeriod.duration : ""
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontBody
                                    font.bold: true
                                }
                                Label {
                                    text: qsTr("Reading Time")
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontCaption
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingLg
                    Layout.rightMargin: Theme.spacingLg
                    spacing: Theme.spacingSm

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Book Progress")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontCaption
                            font.bold: true
                        }
                        Label {
                            text: inspector.stats.bookPercent
                                  ? inspector.stats.bookPercent : ""
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontCaption
                            font.bold: true
                        }
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 1
                        value: inspector.stats.bookFraction
                               ? inspector.stats.bookFraction : 0
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Time to finish Book")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }
                        Label {
                            text: inspector.stats.bookRemaining
                                  ? inspector.stats.bookRemaining : "\u2014"
                            color: text === "\u2014" ? Theme.textSecondary
                                                     : Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Time to finish Chapter")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }
                        Label {
                            text: inspector.stats.chapterRemaining
                                  ? inspector.stats.chapterRemaining : "\u2014"
                            color: text === "\u2014" ? Theme.textSecondary
                                                     : Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingLg
                    Layout.rightMargin: Theme.spacingLg
                    Layout.bottomMargin: Theme.spacingLg
                    spacing: 6

                    Label {
                        text: qsTr("History")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontCaption
                        font.bold: true
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        columnSpacing: 12
                        rowSpacing: 8

                        Label {
                            text: ""
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: inspector.countLabel
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("Reading Speed")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("Reading Time")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }

                        Rectangle {
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.cardStroke
                        }

                        Label {
                            text: qsTr("Today")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                            font.bold: true
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: inspector.todayPeriod.characters
                                  ? inspector.todayPeriod.characters : ""
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: inspector.todayPeriod.speed
                                  ? inspector.todayPeriod.speed : ""
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: inspector.todayPeriod.duration
                                  ? inspector.todayPeriod.duration : ""
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }

                        Label {
                            text: qsTr("All Time")
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                            font.bold: true
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: inspector.allTimePeriod.characters
                                  ? inspector.allTimePeriod.characters : ""
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: inspector.allTimePeriod.speed
                                  ? inspector.allTimePeriod.speed : ""
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: inspector.allTimePeriod.duration
                                  ? inspector.allTimePeriod.duration : ""
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }
                    }
                }
            }
        }
    }
}
