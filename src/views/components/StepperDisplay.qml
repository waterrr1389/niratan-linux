// SPDX-License-Identifier: GPL-3.0-or-later
//
// A value centred between round step buttons, following
// Features/Video/VideoInspectorView.swift `VideoInspectorStepperDisplay`:
// title3 semibold tabular digits, primary while `neutral` and the accent
// otherwise, filled 30 px glyph buttons with tooltips, 6 apart, inside the
// compact row padding.
//
// `decrements` and `increments` list { name, glyph, help, step }; a click
// emits stepped(step) and the page applies it. `name` becomes the button's
// object name and `valueName` the value label's.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

Item {
    id: stepper

    property string valueText
    property bool neutral: true
    property var decrements: []
    property var increments: []
    property alias valueName: valueLabel.objectName

    signal stepped(real step)

    component StepButton: GlyphButton {
        id: button

        required property var modelData

        objectName: modelData.name
        glyph: modelData.glyph
        ToolTip.text: modelData.help
        ToolTip.visible: hovered
        ToolTip.delay: Theme.tooltipDelay
        Accessible.name: modelData.help
        onClicked: stepper.stepped(modelData.step)
    }

    width: parent ? parent.width : implicitWidth
    implicitWidth: content.implicitWidth + 2 * Theme.compactRowPadding
    implicitHeight: Theme.glyphButtonDiameter + 2 * Theme.compactRowPadding

    Row {
        id: content

        x: Theme.compactRowPadding
        width: stepper.width - 2 * Theme.compactRowPadding
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.chipSpacing

        Repeater {
            id: before

            model: stepper.decrements

            delegate: StepButton {}
        }

        StyledLabel {
            id: valueLabel

            anchors.verticalCenter: parent.verticalCenter
            width: content.width - (stepper.decrements.length + stepper.increments.length)
                   * (Theme.glyphButtonDiameter + content.spacing)
            text: stepper.valueText
            textStyle: Theme.title3Font
            weight: Theme.weightSemibold
            monospacedDigits: true
            color: stepper.neutral ? Theme.textPrimary : Theme.accent
            horizontalAlignment: Text.AlignHCenter
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Math.round(Theme.title3Font.pixelSize * 0.7)
        }

        Repeater {
            model: stepper.increments

            delegate: StepButton {}
        }
    }
}
