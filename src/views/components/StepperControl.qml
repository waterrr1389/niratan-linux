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
// Editable integer settings also accept a typed value through stepped.
import QtQuick
import QtQuick.Controls
import Niratan

Item {
    id: stepper

    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 1
    property string valueText
    property bool compact: false
    property bool editable: false
    property bool editing: false

    readonly property Item upButton: upHalf
    readonly property Item downButton: downHalf

    signal stepped(real value)

    implicitWidth: Theme.stepperWidth + (valueText === "" && !editable ? 0
        : Theme.capsuleSpacing + Math.max(valueLabel.implicitWidth,
                                        editable ? Theme.wideInlineFieldWidth : 0))
    implicitHeight: Math.max(Theme.capsuleHeight, valueLabel.implicitHeight)

    Accessible.role: Accessible.SpinBox
    Accessible.description: valueText

    // Tab focuses the stepper; Up and Down step it like its two halves.
    activeFocusOnTab: true
    Keys.onPressed: event => {
        if (editable && activeFocus && /^[0-9]$/.test(event.text)
            && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
            event.accepted = true
            beginEdit(event.text, false)
        }
    }
    Keys.onUpPressed: {
        if (stepSize > 0 && value < to)
            stepped(Math.min(to, value + stepSize))
    }
    Keys.onDownPressed: {
        if (stepSize > 0 && value > from)
            stepped(Math.max(from, value - stepSize))
    }

    function beginEdit(text: string, selectValue: bool) {
        editor.text = text
        editing = true
        editor.forceActiveFocus()
        if (selectValue)
            editor.selectAll()
    }

    function finishEdit(commit: bool, keepFocus: bool) {
        if (!editing)
            return
        const text = editor.text.trim()
        editing = false
        if (keepFocus)
            forceActiveFocus()
        if (commit && /^[+-]?[0-9]+$/.test(text)) {
            const next = Math.max(from, Math.min(to, Number(text)))
            if (next !== value)
                stepped(next)
        }
    }

    StyledLabel {
        id: valueLabel

        objectName: "stepperValueLabel"
        visible: !stepper.editing && stepper.valueText !== ""
        anchors.right: body.left
        anchors.rightMargin: Theme.capsuleSpacing
        anchors.verticalCenter: parent.verticalCenter
        text: stepper.valueText
        textStyle: stepper.compact ? Theme.calloutFont : Theme.bodyFont
        weight: stepper.compact ? Theme.weightSemibold : Theme.weightRegular
        secondary: true
        monospacedDigits: true

        TapHandler {
            enabled: stepper.editable
            onTapped: stepper.beginEdit(String(stepper.value), true)
        }
    }

    TextField {
        id: editor

        objectName: "stepperValueEditor"
        visible: stepper.editing
        activeFocusOnTab: false
        anchors.left: parent.left
        anchors.right: body.left
        anchors.rightMargin: Theme.capsuleSpacing
        anchors.verticalCenter: parent.verticalCenter
        height: Theme.capsuleHeight
        font.pixelSize: valueLabel.font.pixelSize
        font.weight: valueLabel.font.weight
        font.features: valueLabel.font.features
        horizontalAlignment: TextInput.AlignRight
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        onAccepted: stepper.finishEdit(true, true)
        onActiveFocusChanged: {
            if (!activeFocus)
                stepper.finishEdit(true, false)
        }
        Keys.onShortcutOverride: event => {
            if (event.key === Qt.Key_Escape)
                event.accepted = true
        }
        Keys.onEscapePressed: stepper.finishEdit(false, true)
    }

    Rectangle {
        id: body

        x: stepper.width - width
        width: Theme.stepperWidth
        height: Theme.capsuleHeight
        anchors.verticalCenter: parent.verticalCenter
        radius: Theme.controlRadius
        color: Theme.controlBackground
        border.width: stepper.activeFocus ? Theme.focusRingWidth : Theme.hairlineWidth
        border.color: stepper.activeFocus ? Theme.accent : Theme.controlStroke

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
