// SPDX-License-Identifier: GPL-3.0-or-later
//
// The main window's section sidebar, following
// NativeMac/NativeMacSidebarView.swift `body` and `sidebarBrandHeader`, and
// NativeMac/NativeMacSection.swift `allCases`, `title`, `detail` and
// `systemImage`. The brand header sits above the four sections; the selected
// row tints with the accent over the card surface, hover uses the hairline
// tone, and a row click or Up/Down on the focused list reports the section to
// the window, which owns `selected`. Manga is out of scope, so `book.pages`
// has no row.
//
// The column is 240 wide on the window background. The list content is
// indented 16 from the leading edge; each row carries the section's 16 px SF
// Symbol in a 16-wide frame, the title and the one-line caption, with a 6 px
// corner radius on its background.
//
// The brand icon is the bundled NiratanSidebarIcon PNG, drawn at 58x58. Qt
// Quick cannot round-clip an Image in the software scene graph, so the Canvas
// draws it clipped to the 13 px rounded rectangle; the hairline border is the
// same card stroke the other surfaces use, and the Swift shadow is not
// reproduced (PLAN.md).

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Rectangle {
    id: sidebar

    property int selected: 0

    signal sectionSelected(int index)

    readonly property var sections: [
        {
            title: qsTr("Bookshelf"),
            detail: qsTr("Bookshelf and sync"),
            icon: "books.vertical"
        },
        {
            title: qsTr("Dictionary"),
            detail: qsTr("Dictionary search"),
            icon: "character.book.closed"
        },
        {
            title: qsTr("Video"),
            detail: qsTr("Video learning"),
            icon: "play.rectangle"
        },
        {
            title: qsTr("Settings"),
            detail: qsTr("App settings"),
            icon: "gearshape"
        }
    ]

    color: Theme.windowBackground

    // The list takes keyboard focus like the Swift List(selection:): Up and
    // Down move the selection one section, clamped to the first and last.
    // The window owns the selection, so keys only report the new section.
    activeFocusOnTab: true
    Accessible.role: Accessible.List
    Accessible.name: qsTr("Sections")

    Keys.onUpPressed: function (event) {
        event.accepted = true
        sidebar.sectionSelected(Math.max(0, sidebar.selected - 1))
    }

    Keys.onDownPressed: function (event) {
        event.accepted = true
        sidebar.sectionSelected(Math.min(sidebar.sections.length - 1, sidebar.selected + 1))
    }

    Item {
        id: brand

        x: 18
        y: 12
        width: sidebar.width - 36
        height: 58

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            Item {
                width: 58
                height: 58

                Image {
                    anchors.fill: parent
                    source: "qrc:/qt/qml/Niratan/src/views/shell/niratan-sidebar-icon.png"
                    fillMode: Image.PreserveAspectCrop
                    sourceSize.width: 58
                    sourceSize.height: 58
                }

                // Qt Quick cannot round-clip an Image in the software scene
                // graph. The canvas covers the four corner squares outside the
                // 13 px rounded rectangle with the page background, which is
                // uniform here, so the icon appears clipped to the radius.
                Canvas {
                    id: brandCanvas

                    anchors.fill: parent

                    onPaint: {
                        const context = getContext("2d")
                        const radius = 13
                        const width = brandCanvas.width
                        const height = brandCanvas.height
                        const background = sidebar.color
                        context.clearRect(0, 0, width, height)
                        context.fillStyle = "rgba(" + Math.round(background.r * 255) + ","
                            + Math.round(background.g * 255) + ","
                            + Math.round(background.b * 255) + "," + background.a + ")"
                        context.beginPath()
                        context.moveTo(0, 0)
                        context.lineTo(radius, 0)
                        context.arc(radius, radius, radius, -Math.PI / 2, -Math.PI, true)
                        context.closePath()
                        context.fill()
                        context.beginPath()
                        context.moveTo(width, 0)
                        context.lineTo(width - radius, 0)
                        context.arc(width - radius, radius, radius, -Math.PI / 2, 0, false)
                        context.closePath()
                        context.fill()
                        context.beginPath()
                        context.moveTo(width, height)
                        context.lineTo(width, height - radius)
                        context.arc(width - radius, height - radius, radius, 0, Math.PI / 2, false)
                        context.closePath()
                        context.fill()
                        context.beginPath()
                        context.moveTo(0, height)
                        context.lineTo(0, height - radius)
                        context.arc(radius, height - radius, radius, Math.PI, Math.PI / 2, true)
                        context.closePath()
                        context.fill()
                    }

                    Component.onCompleted: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Connections {
                        target: Theme
                        function onDarkChanged() {
                            brandCanvas.requestPaint()
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 13
                    color: "transparent"
                    border.width: 0.5
                    border.color: Theme.cardStroke
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Niratan")
                color: Theme.textPrimary
                font.pixelSize: 22
                font.weight: Font.DemiBold
            }
        }
    }

    Column {
        id: sectionList

        x: 16
        y: brand.y + brand.height + 12
        width: sidebar.width - 16
        spacing: 2

        Repeater {
            model: sidebar.sections

            delegate: Item {
                id: row

                required property int index
                required property var modelData

                readonly property bool isSelected: sidebar.selected === row.index

                width: sectionList.width
                height: rowContent.implicitHeight + 12

                Accessible.role: Accessible.ListItem
                Accessible.name: row.modelData.title
                Accessible.selectable: true
                Accessible.selected: row.isSelected

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: row.isSelected
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                        : rowHover.hovered ? Theme.cardStroke : "transparent"
                    // The focus ring marks the keyboard position on the
                    // selected row while the list has focus.
                    border.width: row.isSelected && sidebar.activeFocus ? 1 : 0
                    border.color: Theme.accent
                }

                HoverHandler {
                    id: rowHover
                }

                TapHandler {
                    onTapped: {
                        sidebar.forceActiveFocus()
                        sidebar.sectionSelected(row.index)
                    }
                }

                Row {
                    id: rowContent

                    x: 8
                    y: 6
                    spacing: 10

                    Item {
                        width: 16
                        height: titleColumn.height

                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: row.modelData.icon
                            size: 16
                            color: Theme.textSecondary
                        }
                    }

                    Column {
                        id: titleColumn

                        spacing: 2
                        width: row.width - 8 * 2 - 16 - 10

                        Text {
                            width: parent.width
                            text: row.modelData.title
                            color: Theme.textPrimary
                            font.pixelSize: 14
                            wrapMode: Text.NoWrap
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: row.modelData.detail
                            color: Theme.textSecondary
                            font.pixelSize: 12
                            wrapMode: Text.NoWrap
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}
