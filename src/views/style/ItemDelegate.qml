// SPDX-License-Identifier: GPL-3.0-or-later
//
// List row in the Niratan control style, after macOS source list and table
// rows: a rounded hover tint, and the accent fill with white text while
// highlighted.
import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import Niratan

T.ItemDelegate {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 6
    leftPadding: 10
    rightPadding: 10
    spacing: 8
    opacity: enabled ? 1 : 0.45

    icon.width: 16
    icon.height: 16
    icon.color: control.highlighted ? Theme.textOnAccent : Theme.textPrimary

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: control.display === IconLabel.IconOnly
                   || control.display === IconLabel.TextUnderIcon ? Qt.AlignCenter : Qt.AlignLeft
        icon: control.icon
        text: control.text
        font: control.font
        color: control.highlighted ? Theme.textOnAccent : Theme.textPrimary
    }

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: Theme.controlHeight
        radius: Theme.rowRadius
        color: control.highlighted ? Theme.accent
                                   : control.down ? Theme.pressTint
                                                  : control.hovered ? Theme.hoverTint
                                                                    : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
}
