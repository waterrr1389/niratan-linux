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
    // Transcript search ranges into `title`, in UTF-16 positions and order.
    property var highlights: []
    readonly property bool hasHighlights: row.highlights !== undefined
        && row.highlights !== null && row.highlights.length > 0
    text: row.title
    focusPolicy: Qt.TabFocus
    padding: Theme.spacingSm

    function escapeMarkup(value) {
        return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
            .replace(/\n/g, "<br/>")
    }

    // VideoStudySearch.highlighted: the original characters with the matched
    // ranges tinted, left to right.
    function highlightedTitle() {
        const tint = Theme.videoSearchMatch
        const background = "rgba(" + Math.round(tint.r * 255) + "," + Math.round(tint.g * 255)
            + "," + Math.round(tint.b * 255) + "," + tint.a + ")"
        let markup = ""
        let cursor = 0
        for (const range of row.highlights) {
            const start = Math.min(Math.max(range.start, 0), row.title.length)
            const end = Math.min(start + Math.max(range.length, 0), row.title.length)
            markup += row.escapeMarkup(row.title.slice(cursor, start))
            markup += "<span style=\"background-color:" + background + "\">"
                + row.escapeMarkup(row.title.slice(start, end)) + "</span>"
            cursor = end
        }
        return markup + row.escapeMarkup(row.title.slice(cursor))
    }

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
                textFormat: row.hasHighlights ? Text.RichText : Text.PlainText
                text: row.hasHighlights ? row.highlightedTitle() : row.title
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
