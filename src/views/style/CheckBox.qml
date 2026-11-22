// SPDX-License-Identifier: GPL-3.0-or-later
//
// Check box in the Niratan control style, after the macOS check box: a small
// rounded square, filled with the accent and a white mark when checked or a
// bar when partially checked.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.CheckBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 4
    spacing: 6
    opacity: enabled ? 1 : 0.45

    indicator: Rectangle {
        readonly property bool on: control.checkState !== Qt.Unchecked

        implicitWidth: 14
        implicitHeight: 14
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding
                                            : control.leftPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 4
        color: on ? Theme.accent : Theme.fieldBackground
        border.width: control.visualFocus ? 2 : on ? 0 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.controlStroke

        Icon {
            anchors.centerIn: parent
            size: 11
            name: "checkmark"
            color: Theme.textOnAccent
            visible: control.checkState === Qt.Checked
        }
        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 2
            radius: 1
            color: Theme.textOnAccent
            visible: control.checkState === Qt.PartiallyChecked
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
