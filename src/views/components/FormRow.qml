// SPDX-License-Identifier: GPL-3.0-or-later
//
// One form row: an optional glyph in a fixed column, a label with an optional
// subtitle, and one trailing column of controls. Follows
// NativeMac/NativeReuseViews.swift NativeSettingsRow and
// NativeSettingsSubtitledLabel (cards) and Features/Video/VideoInspectorView.swift
// labeledRow and rowLabel (compact).
//
// The density follows the enclosing FormSection unless `compact` is set.
// Cards: a body medium label, minimum height 48, padding 16. Compact: a
// callout label of up to two lines, minimum height 42, padding 12. The
// trailing items form one column whose right edge is the row's right padding,
// centred vertically; the label takes the remaining width and wraps rather
// than pushing that column out, and the row grows to keep the label clear of
// its edges. Swift lets a wrapped inspector label reach the row's edges; here
// the label keeps the subtitled label's 8 px vertical padding so text never
// touches a divider.
//
// Kit rows built on this one set `accessoryItem` to the visible control the
// row aligns; `fillsRow` lets the trailing items span the row instead, for a
// segmented picker that fills its card; `stacked` keeps the label on its own
// line above trailing items that span the row.
import QtQuick
import Niratan

Item {
    id: row

    property string label
    property string subtitle
    // An SF Symbol name drawn by Icon.
    property string glyph
    property color glyphColor: Theme.textSecondary
    property bool compact: section ? (section as FormSection).compact : false
    default property alias trailing: trailingColumn.data

    // The visible control the row aligns on its trailing column.
    readonly property Item trailingItem: accessoryItem ? accessoryItem : trailingColumn
    readonly property Item labelItem: labelText

    // Set by rows built on this one: the control trailingItem names.
    property Item accessoryItem: null
    // The trailing items span the row with the fill-row padding, with no label.
    property bool fillsRow: false
    // The label keeps its own line and the trailing items span the row below
    // it with the fill-row padding, for a control too wide to share the line.
    property bool stacked: false
    // The whole row is clickable; tapped is emitted on a click anywhere.
    property bool clickable: false

    signal tapped()

    readonly property Item section: {
        for (let item = parent; item; item = item.parent) {
            if (item instanceof FormSection)
                return item
        }
        return null
    }

    readonly property real padding: compact ? Theme.compactRowPadding : Theme.rowPadding
    readonly property real spacing: compact ? Theme.compactRowSpacing : Theme.rowSpacing
    readonly property real minimumGap: compact ? Theme.compactRowMinGap : Theme.rowMinGap
    readonly property real minimumHeight: compact ? Theme.compactRowMinHeight : Theme.rowMinHeight
    readonly property real labelX: padding + (glyph !== "" ? Theme.glyphColumnWidth + Theme.glyphSpacing : 0)
    readonly property real trailingWidth: trailingColumn.children.length > 0 && !stacked
                                          ? trailingColumn.width : 0
    readonly property real stackedLabelHeight: Math.max(minimumHeight,
                                                        labelBlock.implicitHeight
                                                        + 2 * Theme.rowWrappedPadding)

    width: parent ? parent.width : implicitWidth
    implicitWidth: labelX + labelText.implicitWidth + spacing + minimumGap + trailingWidth + padding
    implicitHeight: fillsRow
        ? trailingColumn.implicitHeight + 2 * Theme.fillRowPaddingVertical
        : stacked
        ? stackedLabelHeight + trailingColumn.implicitHeight + Theme.fillRowPaddingVertical
        : Math.max(minimumHeight, labelBlock.implicitHeight + 2 * Theme.rowWrappedPadding,
                   trailingColumn.implicitHeight)
    opacity: enabled ? 1 : Theme.disabledOpacity

    // A clickable row is a button for the keyboard and assistive tools too:
    // Tab focuses it, Return, Enter or Space activate it.
    activeFocusOnTab: clickable
    Accessible.focusable: clickable
    Accessible.onPressAction: {
        if (row.clickable && row.enabled)
            row.tapped()
    }
    Keys.onPressed: event => {
        if (row.clickable && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                              || event.key === Qt.Key_Space)) {
            event.accepted = true
            row.tapped()
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: row.clickable && row.activeFocus
        radius: Theme.controlRadius
        color: "transparent"
        border.width: Theme.focusRingWidth
        border.color: Theme.accent
    }

    TapHandler {
        enabled: row.clickable
        onTapped: row.tapped()
    }

    HoverHandler {
        enabled: row.clickable
        cursorShape: Qt.PointingHandCursor
    }

    Item {
        visible: row.glyph !== "" && !row.fillsRow
        x: row.padding
        width: Theme.glyphColumnWidth
        height: row.height

        Icon {
            anchors.centerIn: parent
            name: row.glyph
            size: Theme.glyphSize
            color: row.glyphColor
        }
    }

    Column {
        id: labelBlock

        visible: !row.fillsRow && row.label !== ""
        x: row.labelX
        y: ((row.stacked ? row.stackedLabelHeight : row.height) - height) / 2
        width: Math.max(0, (row.trailingWidth > 0 ? trailingColumn.x - row.spacing - row.minimumGap
                                                  : row.width - row.padding) - x)
        spacing: Theme.rowSubtitleSpacing

        StyledLabel {
            id: labelText

            width: labelBlock.width
            text: row.label
            textStyle: row.compact ? Theme.calloutFont : Theme.bodyFont
            weight: row.compact ? Theme.weightRegular : Theme.weightMedium
            wrapMode: Text.Wrap
            maximumLineCount: row.compact ? Theme.compactLabelMaximumLines : undefined
            elide: row.compact ? Text.ElideRight : Text.ElideNone
        }

        StyledLabel {
            visible: row.subtitle !== ""
            width: labelBlock.width
            text: row.subtitle
            textStyle: Theme.captionFont
            secondary: true
            wrapMode: Text.Wrap
        }
    }

    Row {
        id: trailingColumn

        x: row.fillsRow || row.stacked ? Theme.fillRowPaddingHorizontal
                                       : row.width - row.padding - width
        y: row.stacked ? row.stackedLabelHeight : (row.height - height) / 2
        spacing: row.spacing
    }
}
