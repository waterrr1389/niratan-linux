// SPDX-License-Identifier: GPL-3.0-or-later
//
// The labels-hidden switch of a kit toggle row, drawn like the style's Switch
// (src/views/style/Switch.qml): a capsule track, the accent when on, a white
// knob that slides. `small` is SwiftUI's .controlSize(.small), which
// Features/Video/VideoInspectorView.swift toggleRow gives its switch: 28 by 16
// instead of 34 by 20. The control has no padding and no label, so the track
// is the whole control.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.Switch {
    id: control

    property bool small: false

    implicitWidth: implicitIndicatorWidth
    implicitHeight: implicitIndicatorHeight
    padding: 0
    spacing: 0
    opacity: enabled ? 1 : Theme.disabledOpacity

    indicator: Rectangle {
        implicitWidth: control.small ? Theme.smallSwitchWidth : Theme.switchWidth
        implicitHeight: control.small ? Theme.smallSwitchHeight : Theme.switchHeight
        width: control.width
        height: control.height
        radius: height / 2
        color: control.checked ? Theme.accent : Theme.trackBackground
        border.width: control.visualFocus ? Theme.focusRingWidth : 0
        border.color: Theme.accent

        Rectangle {
            readonly property real inset: Theme.switchKnobInset

            width: parent.height - 2 * inset
            height: width
            radius: width / 2
            y: inset
            x: Math.max(inset, Math.min(parent.width - width - inset,
                                        control.visualPosition * parent.width - width / 2))
            color: Theme.knob
            border.width: Theme.hairlineWidth
            border.color: Theme.controlStroke

            Behavior on x {
                enabled: !control.down
                NumberAnimation { duration: Theme.motionFast }
            }
        }
    }

    contentItem: Item {}
}
