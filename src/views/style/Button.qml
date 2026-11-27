// SPDX-License-Identifier: GPL-3.0-or-later
//
// Push button in the Niratan control style, after the macOS push button: a
// capsule with the control fill and a hairline, the accent fill when
// highlighted, checked or prominent, and no fill when flat until hovered.
// Hover and press lay a tint over the fill; a disabled button is dimmed.
// `prominent` marks a dialog's default button: DialogButtonBox resets
// `highlighted` on the buttons it creates, so the style keeps its own flag.
import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import Niratan

T.Button {
    id: control

    // macOS controls do not take the keyboard on a click, so the view that
    // held it (the player and its shortcuts) keeps it; Tab still reaches
    // them. Every clickable control of this style does the same.
    focusPolicy: Qt.TabFocus

    property bool prominent: false
    readonly property bool filled: control.highlighted || control.checked || control.prominent

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 5
    // The capsule inset. A control that sets its own padding resets this too,
    // since horizontalPadding takes precedence over padding.
    horizontalPadding: 12
    spacing: 6
    opacity: enabled ? 1 : 0.45

    icon.width: 14
    icon.height: 14
    icon.color: filled ? Theme.textOnAccent : Theme.textPrimary

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.filled ? Theme.textOnAccent : Theme.textPrimary
    }

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: Theme.controlHeight
        radius: height / 2
        visible: !control.flat || control.filled || control.hovered || control.down
        color: control.filled ? Theme.accent
                              : control.flat ? "transparent" : Theme.controlBackground
        border.width: control.visualFocus ? 2 : control.flat || control.filled ? 0 : 1
        border.color: control.visualFocus ? Theme.accent : Theme.controlStroke

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.down ? Theme.pressTint
                                : control.hovered ? Theme.hoverTint : "transparent"
        }
    }
}
