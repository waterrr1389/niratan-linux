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

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
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

    spacing: 0

    ReaderInspectorHeader {
        Layout.fillWidth: true
        title: qsTr("Statistics")
        onCloseRequested: inspector.closeRequested()

        IconButton {
            icon: inspector.tracking ? "pause.fill" : "play.fill"
            diameter: 28
            iconSize: 11
            accessibleName: inspector.tracking ? qsTr("Pause Tracking")
                                               : qsTr("Start Tracking")
            onClicked: ReaderViewModel.toggleTracking()
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
                objectName: "sessionHero"
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
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
                        Text {
                            text: inspector.tracking ? qsTr("Tracking") : qsTr("Paused")
                            color: inspector.tracking ? Theme.textPrimary
                                                      : Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: qsTr("Session")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                    }

                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: inspector.sessionPeriod.characters
                                  ? inspector.sessionPeriod.characters : ""
                            color: Theme.textPrimary
                            font.pixelSize: 40
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
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
                            Text {
                                text: inspector.sessionPeriod.speed
                                      ? inspector.sessionPeriod.speed : ""
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                                font.bold: true
                            }
                            Text {
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
                            Text {
                                text: inspector.sessionPeriod.duration
                                      ? inspector.sessionPeriod.duration : ""
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                                font.bold: true
                            }
                            Text {
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
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                spacing: Theme.spacingSm

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Book Progress")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontCaption
                        font.bold: true
                    }
                    Text {
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
                    value: inspector.stats.bookFraction ? inspector.stats.bookFraction : 0
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Time to finish Book")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        text: inspector.stats.bookRemaining
                              ? inspector.stats.bookRemaining : "\u2014"
                        color: text === "\u2014" ? Theme.textSecondary : Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Time to finish Chapter")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        text: inspector.stats.chapterRemaining
                              ? inspector.stats.chapterRemaining : "\u2014"
                        color: text === "\u2014" ? Theme.textSecondary : Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.bottomMargin: 18
                spacing: 6

                Text {
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

                    Text { text: "" }
                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: inspector.countLabel
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Reading Speed")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                    Text {
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

                    Text {
                        text: qsTr("Today")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                        font.bold: true
                    }
                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: inspector.todayPeriod.characters
                              ? inspector.todayPeriod.characters : ""
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: inspector.todayPeriod.speed ? inspector.todayPeriod.speed : ""
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: inspector.todayPeriod.duration
                              ? inspector.todayPeriod.duration : ""
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }

                    Text {
                        text: qsTr("All Time")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                        font.bold: true
                    }
                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: inspector.allTimePeriod.characters
                              ? inspector.allTimePeriod.characters : ""
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        Layout.alignment: Qt.AlignRight
                        text: inspector.allTimePeriod.speed ? inspector.allTimePeriod.speed : ""
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
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
