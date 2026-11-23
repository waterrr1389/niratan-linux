// SPDX-License-Identifier: GPL-3.0-or-later
//
// One row of a macOS sidebar-style List, as Mac Niratan's section sidebar
// (NativeMac/NativeMacSidebarView.swift) and inner columns
// (Features/Bookshelf/LibraryShelfSidebar.swift) draw them: a 16 px glyph,
// the title, an optional caption line and an optional trailing count badge.
// The selected row is filled with the accent while its list has keyboard
// focus (`emphasized`) and with the grey inactive selection otherwise, as
// macOS draws an unfocused source list. A zero badge is hidden, as SwiftUI's
// `.badge(Int)` hides 0.
import QtQuick
import Niratan

Item {
    id: row

    property string symbol
    property string title
    property string detail
    property int badge: 0
    property bool selected: false
    property bool emphasized: false
    property color symbolColor: Theme.accent
    // Names the badge for a page test; empty leaves it anonymous.
    property string badgeObjectName

    signal clicked()
    signal contextMenuRequested()

    readonly property bool filled: row.selected && row.emphasized
    readonly property color textColor: row.filled ? Theme.textOnAccent : Theme.textPrimary

    implicitWidth: 180
    implicitHeight: row.detail.length > 0 ? 40 : 28

    Accessible.role: Accessible.ListItem
    Accessible.name: row.title
    Accessible.selectable: true
    Accessible.selected: row.selected

    Rectangle {
        anchors.fill: parent
        radius: Theme.rowRadius
        color: row.filled ? Theme.accent
                          : row.selected ? Theme.inactiveSelection
                                         : hover.hovered ? Theme.hoverTint : "transparent"
    }

    HoverHandler {
        id: hover
    }

    TapHandler {
        onTapped: row.clicked()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: row.contextMenuRequested()
    }

    // Tight insets so "Currently Reading" and a two-digit badge fit the
    // 176 px minimum of the bookshelf's shelf column.
    Icon {
        id: glyph

        x: 6
        anchors.verticalCenter: parent.verticalCenter
        size: 16
        name: row.symbol
        color: row.filled ? Theme.textOnAccent : row.symbolColor
    }

    Column {
        anchors.left: glyph.right
        anchors.leftMargin: 6
        anchors.right: badgeText.visible ? badgeText.left : parent.right
        anchors.rightMargin: 5
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        Text {
            width: parent.width
            text: row.title
            color: row.textColor
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: row.detail.length > 0
            text: row.detail
            color: row.filled ? Theme.textOnAccent : Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
    }

    Text {
        id: badgeText

        objectName: row.badgeObjectName
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        visible: row.badge > 0
        text: row.badge
        color: row.filled ? Theme.textOnAccent : Theme.textSecondary
        font.pixelSize: Theme.fontCallout
        font.features: { "tnum": 1 }
    }
}
