// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pane in the Niratan control style: the page background.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.Pane {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)

    padding: 12

    background: Rectangle {
        color: Theme.windowBackground
    }
}
