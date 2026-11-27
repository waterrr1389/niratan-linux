// SPDX-License-Identifier: GPL-3.0-or-later
//
// Radio button in the Niratan control style, after the macOS radio button: a
// small circle, filled with the accent and a white dot when checked.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.RadioButton {
    id: control

    // A click leaves the keyboard where it was, as on macOS (see Button.qml).
    focusPolicy: Qt.TabFocus

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 4
    spacing: 6
    opacity: enabled ? 1 : 0.45

    indicator: Rectangle {
        implicitWidth: 14
        implicitHeight: 14
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding
                                            : control.leftPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: width / 2
        color: control.checked ? Theme.accent : Theme.fieldBackground
        border.width: control.visualFocus ? 2 : control.checked ? 0 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.controlStroke

        Rectangle {
            anchors.centerIn: parent
            width: 6
            height: 6
            radius: 3
            color: Theme.textOnAccent
            visible: control.checked
        }
    }

    contentItem: Text {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0
        text: control.text
        font: control.font
        color: Theme.textPrimary
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
