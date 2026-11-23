// SPDX-License-Identifier: GPL-3.0-or-later
//
// A value text followed by a two-half stepper, 8 apart. Follows
// Features/Settings/VideoSettingsView.swift Default Seek Interval row (cards:
// the value in body tabular digits secondary, then a labels-hidden Stepper)
// and the Features/Video/VideoInspectorView.swift Subtitle Weight Stepper
// (compact: callout semibold tabular digits secondary). A press on the up or
// down half emits stepped with value plus or minus stepSize clamped to
// [from, to], so a value off the step grid still reaches a bound; a half is
// disabled at its bound, and both are disabled unless stepSize is positive.
// The page writes the value back and formats valueText.
import QtQuick
import Niratan

Item {
    id: stepper

    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 1
    property string valueText
    property bool compact: false

    readonly property Item upButton: upHalf
    readonly property Item downButton: downHalf

    signal stepped(real value)

    implicitWidth: (valueText !== "" ? valueLabel.implicitWidth + Theme.capsuleSpacing : 0)
                   + Theme.stepperWidth
    implicitHeight: Math.max(Theme.capsuleHeight, valueLabel.implicitHeight)

    Accessible.role: Accessible.SpinBox
    Accessible.description: valueText

    StyledLabel {
        id: valueLabel

        visible: stepper.valueText !== ""
        anchors.verticalCenter: parent.verticalCenter
        text: stepper.valueText
        textStyle: stepper.compact ? Theme.calloutFont : Theme.bodyFont
        weight: stepper.compact ? Theme.weightSemibold : Theme.weightRegular
        secondary: true
        monospacedDigits: true
    }

    Rectangle {
        id: body

        x: stepper.width - width
        width: Theme.stepperWidth
        height: Theme.capsuleHeight
        anchors.verticalCenter: parent.verticalCenter
        radius: Theme.controlRadius
        color: Theme.controlBackground
        border.width: Theme.hairlineWidth
        border.color: Theme.controlStroke

        Item {
            id: upHalf

            width: parent.width
            height: parent.height / 2
            enabled: stepper.stepSize > 0 && stepper.value < stepper.to

            TapHandler {
                id: upTap

                onTapped: stepper.stepped(Math.min(stepper.to, stepper.value + stepper.stepSize))
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.controlRadius
                color: upTap.pressed ? Theme.pressTint : "transparent"
            }

            Icon {
                anchors.centerIn: parent
                name: "chevron.up"
                size: Theme.stepperGlyphSize
                color: Theme.textPrimary
                opacity: upHalf.enabled ? 1 : Theme.disabledOpacity
            }
        }

        Item {
            id: downHalf

            y: parent.height / 2
            width: parent.width
            height: parent.height / 2
            enabled: stepper.stepSize > 0 && stepper.value > stepper.from

            TapHandler {
                id: downTap

                onTapped: stepper.stepped(Math.max(stepper.from, stepper.value - stepper.stepSize))
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.controlRadius
                color: downTap.pressed ? Theme.pressTint : "transparent"
            }

            Icon {
                anchors.centerIn: parent
                name: "chevron.down"
                size: Theme.stepperGlyphSize
                color: Theme.textPrimary
                opacity: downHalf.enabled ? 1 : Theme.disabledOpacity
            }
        }
    }
}
