// SPDX-License-Identifier: GPL-3.0-or-later
//
// Menu row in the Niratan control style, after macOS menu items: compact text
// with a check column, an accent fill with white text while highlighted, and a
// chevron for a submenu.
import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T
import Niratan

T.MenuItem {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 3
    leftPadding: 22
    rightPadding: 12
    spacing: 6
    opacity: enabled ? 1 : 0.45

    readonly property color textColor: control.highlighted ? Theme.textOnAccent : Theme.textPrimary

    icon.width: 14
    icon.height: 14
    icon.color: textColor

    contentItem: IconLabel {
        readonly property real arrowPadding: control.subMenu && control.arrow
                                             ? control.arrow.width + control.spacing : 0
        rightPadding: arrowPadding
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display
        alignment: Qt.AlignLeft
        icon: control.icon
        text: control.text
        font: control.font
        color: control.textColor
    }

    indicator: Icon {
        x: 6
        y: control.topPadding + (control.availableHeight - height) / 2
        size: 12
        name: "checkmark"
        color: control.textColor
        visible: control.checked
    }

    arrow: Icon {
        x: control.width - width - 8
        y: control.topPadding + (control.availableHeight - height) / 2
        size: 11
        name: "chevron.right"
        color: control.textColor
        visible: control.subMenu
    }

    background: Rectangle {
        implicitWidth: 180
        implicitHeight: 24
        radius: 5
        color: control.highlighted || control.down ? Theme.accent : "transparent"
    }
}
