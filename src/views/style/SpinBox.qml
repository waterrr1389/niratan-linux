// SPDX-License-Identifier: GPL-3.0-or-later
//
// Spin box in the Niratan control style, after the macOS text field with a
// stepper: a rounded field with the value and stacked up and down chevrons at
// the trailing edge.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.SpinBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 4
    leftPadding: 9
    rightPadding: 4 + stepperWidth
    opacity: enabled ? 1 : 0.45

    readonly property real stepperWidth: 20

    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        clip: width < implicitWidth
        font: control.font
        color: Theme.textPrimary
        selectionColor: Theme.accent
        selectedTextColor: Theme.textOnAccent
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    up.indicator: Item {
        x: control.width - width - 2
        y: 2
        implicitWidth: control.stepperWidth
        implicitHeight: (control.height - 4) / 2

        Icon {
            anchors.centerIn: parent
            size: 12
            name: "chevron.up"
            opacity: control.value < Math.max(control.from, control.to) ? 1 : 0.35
            color: control.up.pressed || control.up.hovered ? Theme.textPrimary
                                                            : Theme.textSecondary
        }
    }

    down.indicator: Item {
        x: control.width - width - 2
        y: control.height / 2
        implicitWidth: control.stepperWidth
        implicitHeight: (control.height - 4) / 2

        Icon {
            anchors.centerIn: parent
            size: 12
            name: "chevron.down"
            opacity: control.value > Math.min(control.from, control.to) ? 1 : 0.35
            color: control.down.pressed || control.down.hovered ? Theme.textPrimary
                                                                : Theme.textSecondary
        }
    }

    background: Rectangle {
        implicitWidth: 90
        implicitHeight: Theme.controlHeight
        radius: 7
        color: Theme.fieldBackground
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.accent : Theme.controlStroke
    }
}
