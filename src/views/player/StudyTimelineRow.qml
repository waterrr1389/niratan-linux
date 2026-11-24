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
        StyledLabel {
            Layout.minimumWidth: Theme.studyMarkerColumnWidth
            Layout.alignment: Qt.AlignTop
            secondary: true
            textStyle: Theme.captionFont
            text: row.marker
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            StyledLabel {
                Layout.fillWidth: true
                textStyle: Theme.bodyFont
                weight: row.selected ? Theme.weightSemibold : Theme.weightRegular
                color: Theme.textPrimary
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: row.title
            }
            StyledLabel {
                visible: row.detail.length > 0
                secondary: true
                textStyle: Theme.captionFont
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
            width: Theme.studyResizeActiveWidth
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
