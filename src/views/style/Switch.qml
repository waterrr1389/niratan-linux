// SPDX-License-Identifier: GPL-3.0-or-later
//
// Switch in the Niratan control style, after the macOS switch: a capsule track,
// the accent when on, with a white knob that slides.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 4
    spacing: 8
    opacity: enabled ? 1 : 0.45

    indicator: Rectangle {
        implicitWidth: 34
        implicitHeight: 20
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding
                                            : control.leftPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: control.checked ? Theme.accent : Theme.trackBackground
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent

        Rectangle {
            readonly property real inset: 2

            width: parent.height - 2 * inset
            height: width
            radius: width / 2
            y: inset
            x: Math.max(inset, Math.min(parent.width - width - inset,
                                        control.visualPosition * parent.width - width / 2))
            color: Theme.knob
            border.width: 1
            border.color: Theme.controlStroke

            Behavior on x {
                enabled: !control.down
                NumberAnimation { duration: Theme.motionFast }
            }
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
