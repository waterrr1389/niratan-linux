// SPDX-License-Identifier: GPL-3.0-or-later
//
// Local verification page for the theme and the Niratan control style. Start
// with NIRATAN_THEME_PREVIEW=1 and NIRATAN_THEME=light|dark to see the
// surfaces, the text styles and every styled control in its common states,
// with a menu held open. It is not part of any user flow and draws only from
// Theme and the style.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

ApplicationWindow {
    id: window

    visible: true
    width: 960
    height: 640
    title: "Niratan theme preview"
    color: Theme.windowBackground

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 200
            color: Theme.sidebarBackground

            Column {
                anchors.fill: parent
                anchors.margins: 8
                anchors.topMargin: 16
                spacing: 2

                Label {
                    leftPadding: 8
                    bottomPadding: 4
                    text: "Sidebar"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSubheadline
                    font.weight: Font.DemiBold
                }
                ItemDelegate {
                    width: parent.width
                    text: "Highlighted row"
                    highlighted: true
                }
                ItemDelegate {
                    width: parent.width
                    text: "Plain row"
                }
                ItemDelegate {
                    width: parent.width
                    text: "Disabled row"
                    enabled: false
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: Theme.separator
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: content.implicitHeight + 40
            clip: true

            ColumnLayout {
                id: content

                x: 20
                y: 20
                width: parent.width - 40
                spacing: 14

                Label {
                    text: "Large Title"
                    font.pixelSize: Theme.fontLargeTitle
                    font.weight: Font.Bold
                }
                Label {
                    text: "Title 15 semibold · body 13 · 日本語の本文 · "
                          + "subheadline 11 in secondary"
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                }
                Label {
                    text: "Body text in the primary label colour, 読書と動画で日本語を学ぶ。"
                }
                Label {
                    text: "Secondary label · tertiary below"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSubheadline
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: cardRow.implicitHeight + 24
                    radius: Theme.cardRadius
                    color: Theme.cardBackground
                    border.width: Theme.cardBorderWidth
                    border.color: Theme.cardStroke

                    RowLayout {
                        id: cardRow

                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10

                        Button {
                            text: "Push"
                        }
                        Button {
                            text: "Default"
                            highlighted: true
                        }
                        Button {
                            text: "Flat"
                            flat: true
                        }
                        Button {
                            text: "Disabled"
                            enabled: false
                        }
                        ToolButton {
                            icon.source: "qrc:/qt/qml/Niratan/src/views/icons/phosphor/regular/plus.svg"
                        }
                        ToolButton {
                            checked: true
                            icon.source: "qrc:/qt/qml/Niratan/src/views/icons/phosphor/regular/list-checks.svg"
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }

                RowLayout {
                    spacing: 14

                    CheckBox {
                        text: "Unchecked"
                    }
                    CheckBox {
                        text: "Checked"
                        checked: true
                    }
                    CheckBox {
                        text: "Partial"
                        tristate: true
                        checkState: Qt.PartiallyChecked
                    }
                    RadioButton {
                        text: "Radio"
                    }
                    RadioButton {
                        text: "Selected"
                        checked: true
                    }
                }

                RowLayout {
                    spacing: 14

                    Switch {
                        text: "Off"
                    }
                    Switch {
                        text: "On"
                        checked: true
                    }
                    Slider {
                        Layout.preferredWidth: 160
                        value: 0.4
                    }
                    ComboBox {
                        model: ["Recent Playback", "Title", "Manual"]
                    }
                }

                RowLayout {
                    spacing: 14

                    TextField {
                        placeholderText: "Search Videos"
                    }
                    TextField {
                        text: "今日は"
                    }
                    SpinBox {
                        value: 45
                        to: 100
                    }
                }

                RowLayout {
                    spacing: 14

                    TabBar {
                        TabButton {
                            text: "Episodes"
                        }
                        TabButton {
                            text: "Video"
                        }
                        TabButton {
                            text: "Subtitles"
                        }
                    }
                    ProgressBar {
                        Layout.preferredWidth: 140
                        value: 0.35
                    }
                    BusyIndicator {}
                }

                Item {
                    Layout.preferredHeight: 120
                    Layout.fillWidth: true
                }
            }
        }
    }

    Menu {
        id: menu

        MenuItem {
            text: "Open Subtitles…"
        }
        MenuItem {
            text: "Checked item"
            checkable: true
            checked: true
        }
        MenuSeparator {}
        MenuItem {
            text: "Disabled item"
            enabled: false
        }
    }

    Component.onCompleted: menu.popup(window.width - 220, window.height - 150)
}
