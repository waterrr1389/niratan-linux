// SPDX-License-Identifier: GPL-3.0-or-later
//
// Toolbar button in the Niratan control style: a glyph or short label with no
// fill until hovered, the accent tint when checked. Toolbars group these in a
// GlassCapsule, as Mac Niratan's toolbar does.
import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import Niratan

T.ToolButton {
    id: control

    // A click leaves the keyboard where it was, as on macOS (see Button.qml).
    focusPolicy: Qt.TabFocus

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 6
    spacing: 4
    opacity: enabled ? 1 : 0.45

    icon.width: 16
    icon.height: 16
    icon.color: control.checked ? Theme.accent : Theme.textPrimary

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.checked ? Theme.accent : Theme.textPrimary
    }

    background: Rectangle {
        implicitWidth: Theme.controlHeight
        implicitHeight: Theme.controlHeight
        radius: height / 2
        color: control.down ? Theme.pressTint
                            : control.hovered || control.checked ? Theme.hoverTint
                                                                 : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
}
