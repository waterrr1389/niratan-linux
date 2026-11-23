// SPDX-License-Identifier: GPL-3.0-or-later
//
// A selectable list row with a title, an optional detail line and a trailing
// checkmark while selected, following Features/Video/VideoInspectorView.swift
// `VideoInspectorChoiceRow`: callout title (semibold when selected, middle
// truncation), caption secondary detail, the accent checkmark, and the hover
// and press tints of `VideoInspectorRowButtonStyle`. Padding and minimum
// height follow the compact FormRow.
//
// `rowType` and `rowId` identify the choice for the page; clicked is emitted
// on a tap.

import QtQuick
import Niratan

Item {
    id: row

    property string rowType: ""
    property int rowId: 0
    property string title: ""
    property string detail: ""
    property bool selected: false
    readonly property bool hovered: hover.hovered

    signal clicked()

    width: parent ? parent.width : implicitWidth
    implicitWidth: content.implicitWidth + 2 * Theme.compactRowPadding
    implicitHeight: Math.max(Theme.compactRowMinHeight, content.implicitHeight + 2 * Theme.rowWrappedPadding)

    Rectangle {
        anchors.fill: parent
        color: tap.pressed ? Theme.pressTint : row.hovered ? Theme.hoverTint : "transparent"
    }

    HoverHandler {
        id: hover

        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        id: tap

        onTapped: row.clicked()
    }

    Column {
        id: content

        x: Theme.compactRowPadding
        width: check.x - Theme.compactRowSpacing - x
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.rowSubtitleSpacing

        StyledLabel {
            width: parent.width
            text: row.title
            textStyle: Theme.calloutFont
            weight: row.selected ? Theme.weightSemibold : Theme.weightRegular
            elide: Text.ElideMiddle
        }

        StyledLabel {
            width: parent.width
            visible: row.detail.length > 0
            text: row.detail
            textStyle: Theme.captionFont
            secondary: true
            elide: Text.ElideRight
        }
    }

    Icon {
        id: check

        x: row.width - Theme.compactRowPadding - width
        anchors.verticalCenter: parent.verticalCenter
        visible: row.selected
        name: "checkmark"
        size: Theme.glyphSize
        color: Theme.accent
    }
}
