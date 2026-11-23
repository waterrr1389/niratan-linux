// SPDX-License-Identifier: GPL-3.0-or-later
//
// Slider in the Niratan control style, after the macOS slider: a thin rounded
// track filled with the accent up to a white pill knob. `small` is SwiftUI's
// .controlSize(.small), which the inspector rows of the control kit use.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.Slider {
    id: control

    property bool small: false

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitHandleHeight + topPadding + bottomPadding)

    padding: 2
    opacity: enabled ? 1 : 0.45

    handle: Rectangle {
        x: control.leftPadding + (control.horizontal
                                  ? control.visualPosition * (control.availableWidth - width)
                                  : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal
                                 ? (control.availableHeight - height) / 2
                                 : control.visualPosition * (control.availableHeight - height))
        // A pill along the track, as macOS 26 draws the slider knob.
        implicitWidth: control.horizontal ? (control.small ? 20 : 24) : (control.small ? 14 : 16)
        implicitHeight: control.horizontal ? (control.small ? 14 : 16) : (control.small ? 20 : 24)
        radius: Math.min(width, height) / 2
        color: Theme.knob
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.controlStroke
    }

    background: Item {
        x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)
        readonly property real thickness: control.small ? 3 : 4

        implicitWidth: control.horizontal ? 160 : thickness
        implicitHeight: control.horizontal ? thickness : 160
        width: control.horizontal ? control.availableWidth : implicitWidth
        height: control.horizontal ? implicitHeight : control.availableHeight

        Rectangle {
            anchors.fill: parent
            radius: parent.thickness / 2
            color: Theme.trackBackground
        }
        Rectangle {
            x: control.horizontal && control.mirrored ? parent.width - width : 0
            y: control.horizontal ? 0 : control.visualPosition * parent.height
            width: control.horizontal ? control.position * parent.width : parent.width
            height: control.horizontal ? parent.height : control.position * parent.height
            radius: parent.thickness / 2
            color: Theme.accent
        }
    }
}
