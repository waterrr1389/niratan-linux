// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Button {
    id: row
    required property string marker
    required property string title
    property string detail: ""
    property bool selected: false
    text: row.title
    focusPolicy: Qt.TabFocus
    padding: Theme.spacingSm
    contentItem: RowLayout {
        spacing: Theme.spacingSm
        Label {
            Layout.minimumWidth: 48
            Layout.alignment: Qt.AlignTop
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            text: row.marker
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            Label {
                Layout.fillWidth: true
                font.pixelSize: Theme.fontBody
                font.bold: row.selected
                color: Theme.textPrimary
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: row.title
            }
            Label {
                visible: row.detail.length > 0
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                text: row.detail
            }
        }
    }
    background: Rectangle {
        color: row.selected || row.hovered || row.down ? Theme.cardBackground : Theme.windowBackground
        border.width: row.activeFocus ? Theme.cardBorderWidth : 0
        border.color: Theme.accent
        Rectangle {
            height: parent.height
            width: 2
            visible: row.selected
            color: Theme.accent
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: Theme.cardBorderWidth
            color: Theme.cardStroke
        }
    }
}
