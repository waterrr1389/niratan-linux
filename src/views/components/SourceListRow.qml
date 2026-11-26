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
//
// `editing` replaces the title with a plain text field for an inline rename,
// as LibraryShelfSidebar.shelfRow draws its TextField: the glyph stays, the
// badge hides, and the field takes focus when it appears. Accepted, cancelled
// and focus-lost are reported to the owner, which decides what to commit.
import QtQuick
import QtQuick.Controls
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
    // The inline editor's state and text; see the comment above.
    property bool editing: false
    property alias editorText: editor.text
    property alias editorPlaceholderText: editor.placeholderText
    property alias editorObjectName: editor.objectName

    signal clicked()
    signal contextMenuRequested()
    signal doubleClicked()
    signal editorAccepted()
    signal editorCancelled()
    signal editorFocusLost()

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

    // LibraryShelfSidebar's primaryAction: a double click starts the rename
    // while the single-click selection above still happens first.
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onDoubleTapped: row.doubleClicked()
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
            visible: !row.editing
            text: row.title
            color: row.textColor
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
        }

        TextField {
            id: editor

            width: parent.width
            visible: row.editing
            background: null
            padding: 0
            leftPadding: 0
            rightPadding: 0
            topPadding: 0
            bottomPadding: 0
            color: row.textColor
            font.pixelSize: Theme.fontBody
            selectByMouse: true
            activeFocusOnTab: false

            onVisibleChanged: {
                if (visible)
                    Qt.callLater(forceActiveFocus)
            }
            onActiveFocusChanged: {
                if (!activeFocus && visible)
                    row.editorFocusLost()
            }
            onAccepted: row.editorAccepted()
            Keys.onEscapePressed: event => {
                row.editorCancelled()
                event.accepted = true
            }
        }

        Text {
            width: parent.width
            visible: row.detail.length > 0 && !row.editing
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
        visible: row.badge > 0 && !row.editing
        text: row.badge
        color: row.filled ? Theme.textOnAccent : Theme.textSecondary
        font.pixelSize: Theme.fontCallout
        font.features: { "tnum": 1 }
    }
}
