// SPDX-License-Identifier: GPL-3.0-or-later
//
// Segment in the Niratan control style: a raised capsule behind the checked
// segment, plain text and glyph otherwise.
import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import Niratan

T.TabButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 4
    leftPadding: 10
    rightPadding: 10
    spacing: 5
    opacity: enabled ? 1 : 0.45

    icon.width: 14
    icon.height: 14
    icon.color: Theme.textPrimary

    // The bar sizes every segment to the widest; a label that changes after
    // load asks it to measure again.
    onImplicitWidthChanged: (T.TabBar.tabBar as TabBar)?.remeasure()

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        icon: control.icon
        text: control.text
        font: control.font
        color: control.checked ? Theme.textPrimary : Theme.textSecondary
    }

    background: Rectangle {
        implicitHeight: Theme.controlHeight - 4
        radius: height / 2
        color: control.checked ? Theme.segmentSelected
                               : control.hovered ? Theme.hoverTint : "transparent"
        border.width: control.visualFocus ? 2 : control.checked ? 1 : 0
        border.color: control.visualFocus ? Theme.accent : Theme.controlStroke
    }
}
