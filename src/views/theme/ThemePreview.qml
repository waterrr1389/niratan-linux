// SPDX-License-Identifier: GPL-3.0-or-later
//
// Local verification page for the M1 tokens. Start with
// NIRATAN_THEME_PREVIEW=1 and NIRATAN_THEME=light|dark to inspect the window,
// card, floating, menu and popup surfaces side by side. It is not part of any
// user flow and draws only from Theme.

import QtQuick
import QtQuick.Controls
import Niratan

ApplicationWindow {
    visible: true
    width: 760
    height: 680
    title: "Niratan theme preview"
    color: Theme.windowBackground

    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Text {
            text: "window background"
            color: Theme.textSecondary
            font.pixelSize: 12
        }

        Rectangle {
            width: parent.width
            height: 108
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.width: Theme.cardBorderWidth
            border.color: Theme.cardStroke

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 6

                Text {
                    text: "card"
                    color: Theme.textPrimary
                    font.pixelSize: 18
                }
                Text {
                    text: "cardBackground over windowBackground, cardStroke at 1px"
                    color: Theme.textSecondary
                    font.pixelSize: 13
                }
            }
        }

        Row {
            spacing: 16

            Rectangle {
                width: 220
                height: 44
                radius: height / 2
                color: Theme.floatingBackground
                border.width: Theme.cardBorderWidth
                border.color: Theme.floatingStroke

                Text {
                    anchors.centerIn: parent
                    text: "floating capsule"
                    color: Theme.textPrimary
                    font.pixelSize: 13
                }
            }

            Rectangle {
                width: 260
                height: 44
                radius: height / 2
                color: Theme.floatingBackground
                border.width: Theme.cardBorderWidth
                border.color: Theme.floatingStroke

                Text {
                    anchors.centerIn: parent
                    text: "floating capsule"
                    color: Theme.textPrimary
                    font.pixelSize: 13
                }
            }
        }

        Row {
            spacing: 16

            Rectangle {
                width: 260
                height: 148
                radius: 8
                color: Theme.menuBackground

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text {
                        text: "menu surface"
                        color: Theme.menuText
                        font.pixelSize: 14
                    }
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.menuDivider
                    }
                    Text {
                        text: "secondary item"
                        color: Theme.popupTextSecondary
                        font.pixelSize: 13
                    }
                    Text {
                        text: "tertiary item"
                        color: Theme.popupTextTertiary
                        font.pixelSize: 13
                    }
                }
            }

            Rectangle {
                width: 300
                height: 148
                radius: 8
                color: Theme.popupBackground
                border.width: Theme.cardBorderWidth
                border.color: Theme.menuDivider

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    Text {
                        text: "popup frame"
                        color: Theme.popupText
                        font.pixelSize: 14
                    }
                    Text {
                        text: "muted text"
                        color: Theme.popupTextMuted
                        font.pixelSize: 12
                    }
                    Row {
                        spacing: 10
                        Rectangle {
                            width: 60
                            height: 18
                            color: Theme.popupDanger
                        }
                        Rectangle {
                            width: 60
                            height: 18
                            color: Theme.popupSuccess
                        }
                    }
                }
            }
        }
    }
}
