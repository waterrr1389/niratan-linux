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
// by the page. Optional pieces for the Inspector: `leadingGlyph` and
// `trailingGlyph` at the slider's ends (subtitlePositionSlider),
// `valueHighlighted` draws the value in the accent (videoEqualizerSlider),
// and children sit before the value (its per-row reset button).
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
    property bool valueHighlighted: false
    property string leadingGlyph
    property string trailingGlyph
    // The stored key the row writes, readable on the slider.
    property string settingKey
    // The object name of the slider, for tests that drive the control.
    property alias controlName: control.objectName
    default property alias accessory: extras.data
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

            Row {
                id: extras

                x: valueText.x - (width > 0 ? width + Theme.compactRowMinGap : 0)
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacingXs
            }

            StyledLabel {
                id: labelText

                x: row.showsGlyph ? Theme.glyphColumnWidth + Theme.glyphSpacing : 0
                width: Math.max(0, (extras.width > 0 ? extras.x : valueText.x) - Theme.compactRowMinGap - x)
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
                color: row.valueHighlighted ? Theme.accent
                       : row.compact ? Theme.textSecondary : Theme.textPrimary
                monospacedDigits: row.compact
            }
        }

        Item {
            width: content.width
            implicitHeight: control.implicitHeight

            Icon {
                id: leadingIcon

                visible: row.leadingGlyph !== ""
                anchors.verticalCenter: parent.verticalCenter
                name: row.leadingGlyph
                size: Theme.glyphSize
                color: Theme.textSecondary
            }

            Slider {
                id: control

                readonly property string settingKey: row.settingKey

                x: leadingIcon.visible ? leadingIcon.width + Theme.spacingSm : 0
                width: parent.width - x - (trailingIcon.visible ? trailingIcon.width + Theme.spacingSm : 0)
                small: row.compact
                from: row.from
                to: row.to
                stepSize: row.stepSize
                snapMode: Slider.SnapAlways
                value: row.value
                Accessible.name: row.label
                onMoved: {
                    row.moved(value)
                    if (!pressed)
                        value = Qt.binding(() => row.value)
                }
                // After a drag the slider binds to `value` again, so a change
                // the page does not store springs back.
                onPressedChanged: {
                    if (!pressed)
                        value = Qt.binding(() => row.value)
                }
            }

            Icon {
                id: trailingIcon

                visible: row.trailingGlyph !== ""
                x: parent.width - width
                anchors.verticalCenter: parent.verticalCenter
                name: row.trailingGlyph
                size: Theme.glyphSize
                color: Theme.textSecondary
            }
        }
    }
}
