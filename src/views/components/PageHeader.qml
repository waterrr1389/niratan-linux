// SPDX-License-Identifier: GPL-3.0-or-later
//
// The band every inner page puts above its content: a leading back control, the
// title with an optional subtitle, and a trailing slot for page actions.
//
// It follows the title bar SwiftUI gives these pages rather than drawing one.
// NativeMac/HoshiNativeMacApp.swift hosts the window toolbar,
// Features/Bookshelf/BookshelfView.swift `toolbarContent` keeps navigation
// leading and page actions trailing, Features/Bookshelf/StatisticsDashboardView.swift
// `header` stacks the title over its secondary line,
// Features/Settings/ProfilesView.swift sets the single title through
// navigationTitle, and NativeMac/NativeGlassSurface.swift supplies the card
// tint and stroke the band is filled with. The toolbar's glass, blur and title
// centering are not reproduced: the band is a solid surface with a hairline
// bottom edge, and the title stays left-aligned next to the back control.
//
// The root already fills the width of a layout and is 48 high, so a page only
// places it in its column and reacts to backRequested().
//
// Everything else declared on an instance goes to the trailing slot, which
// stays empty (and takes no width) unless the page adds controls.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

Item {
    id: header

    signal backRequested()

    property string title
    property string subtitle
    property color subtitleColor: Theme.textSecondary
    property bool showBack: true
    default property alias trailing: trailingRow.data

    implicitHeight: 48
    Layout.fillWidth: true

    Rectangle {
        anchors.fill: parent
        color: Theme.cardBackground
    }

    // The hairline separates the band from the page below without boxing it in.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.cardStroke
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingSm

        Item {
            id: back

            visible: header.showBack
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignVCenter

            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Back")
            Accessible.focusable: true
            Accessible.onPressAction: header.backRequested()

            readonly property bool hovered: backHover.hovered
            readonly property bool pressed: backTap.pressed

            Keys.onPressed: function (event) {
                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                        || event.key === Qt.Key_Enter) {
                    event.accepted = true
                    header.backRequested()
                }
            }

            HoverHandler {
                id: backHover
            }

            TapHandler {
                id: backTap
                onTapped: header.backRequested()
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.controlRadius
                color: back.pressed
                    ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                              Theme.textPrimary.b, 0.16)
                    : back.hovered ? Theme.cardStroke : "transparent"
                border.width: back.activeFocus ? 1 : 0
                border.color: Theme.accent
            }

            Canvas {
                id: chevron
                anchors.centerIn: parent
                width: 16
                height: 16

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, chevron.width, chevron.height)
                    ctx.strokeStyle = Theme.textPrimary
                    ctx.lineWidth = 2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(10.5, 2.5)
                    ctx.lineTo(5.5, 8)
                    ctx.lineTo(10.5, 13.5)
                    ctx.stroke()
                }
            }

            Connections {
                target: Theme
                function onDarkChanged() {
                    chevron.requestPaint()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 1

            Text {
                Layout.fillWidth: true
                text: header.title
                color: Theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: header.subtitle.length > 0
                text: header.subtitle
                color: header.subtitleColor
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }
        }

        RowLayout {
            id: trailingRow
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spacingSm
        }
    }
}
