// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tool tip in the Niratan control style, after macOS help tags: small text on
// a compact floating surface.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.ToolTip {
    id: control

    // Below the control, as macOS shows help tags under the pointer.
    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: parent ? parent.height + 4 : 0

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: 6
    topPadding: 3
    bottomPadding: 3
    leftPadding: 7
    rightPadding: 7
    font.pixelSize: Theme.fontSubheadline

    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent
                 | T.Popup.CloseOnReleaseOutsideParent

    contentItem: Text {
        text: control.text
        font: control.font
        wrapMode: Text.Wrap
        color: Theme.textPrimary
    }

    background: FloatingBackground {
        radius: 6
    }
}
