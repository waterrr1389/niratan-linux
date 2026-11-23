// SPDX-License-Identifier: GPL-3.0-or-later
//
// A titled slider: label and formatted value on one line, the slider below.
// Follows NativeMac/NativeReuseViews.swift NativeSettingsSliderRow (cards: a
// body medium label, the value semibold primary, 8 between line and slider)
// and Features/Video/VideoInspectorView.swift sliderRow and sliderContent
// (compact: a glyph and callout label, the value in callout tabular digits
// secondary, 6 between line and slider, the small slider). Both pad 10
// vertically; the value sits on the label's baseline and ends, like the
// slider, on the trailing column.
//
// The density follows the enclosing FormSection unless set. The slider
// follows `value`; moved is emitted on a user drag only, snapped to
// stepSize, and the page writes the new value back. `valueText` is formatted
// by the page.
import QtQuick
import QtQuick.Controls
import Niratan

Item {
    id: row

    property string label
    property string glyph
    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 0
    property string valueText
    property bool compact: section ? (section as FormSection).compact : false

    readonly property Item labelItem: labelText
    readonly property Item valueLabel: valueText
    readonly property Item slider: control
    // The slider is the control the row aligns on the trailing column.
    readonly property Item trailingItem: slider

    signal moved(real value)

    readonly property Item section: {
        for (let item = parent; item; item = item.parent) {
            if (item instanceof FormSection)
                return item
        }
        return null
    }

    readonly property real padding: compact ? Theme.compactRowPadding : Theme.rowPadding
    readonly property bool showsGlyph: compact && glyph !== ""

    width: parent ? parent.width : implicitWidth
    implicitWidth: 2 * padding + labelText.implicitWidth + Theme.rowMinGap + valueText.implicitWidth
    implicitHeight: content.implicitHeight + 2 * Theme.sliderRowPaddingVertical
    opacity: enabled ? 1 : Theme.disabledOpacity

    Column {
        id: content

        x: row.padding
        y: Theme.sliderRowPaddingVertical
        width: row.width - 2 * row.padding
        spacing: row.compact ? Theme.compactSliderRowSpacing : Theme.sliderRowSpacing

        Item {
            width: content.width
            implicitHeight: Math.max(labelText.implicitHeight, valueText.implicitHeight)

            Item {
                visible: row.showsGlyph
                width: Theme.glyphColumnWidth
                height: parent.height

                Icon {
                    anchors.centerIn: parent
                    name: row.glyph
                    size: Theme.glyphSize
                    color: Theme.textSecondary
                }
            }

            StyledLabel {
                id: labelText

                x: row.showsGlyph ? Theme.glyphColumnWidth + Theme.glyphSpacing : 0
                width: Math.max(0, valueText.x - Theme.compactRowMinGap - x)
                anchors.verticalCenter: parent.verticalCenter
                text: row.label
                textStyle: row.compact ? Theme.calloutFont : Theme.bodyFont
                weight: row.compact ? Theme.weightRegular : Theme.weightMedium
                elide: Text.ElideRight
            }

            StyledLabel {
                id: valueText

                x: parent.width - width
                anchors.baseline: labelText.baseline
                text: row.valueText
                textStyle: row.compact ? Theme.calloutFont : Theme.bodyFont
                weight: row.compact ? Theme.weightRegular : Theme.weightSemibold
                secondary: row.compact
                monospacedDigits: row.compact
            }
        }

        Slider {
            id: control

            width: content.width
            small: row.compact
            from: row.from
            to: row.to
            stepSize: row.stepSize
            snapMode: Slider.SnapAlways
            value: row.value
            Accessible.name: row.label
            onMoved: row.moved(value)
        }
    }
}
