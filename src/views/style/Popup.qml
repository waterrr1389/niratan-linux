// SPDX-License-Identifier: GPL-3.0-or-later
//
// Generic popup in the Niratan control style: the floating surface.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.Popup {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 12

    background: FloatingBackground {
        radius: 12
    }

    T.Overlay.modal: Rectangle {
        color: Theme.shadow
    }
}
