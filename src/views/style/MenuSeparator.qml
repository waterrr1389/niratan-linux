// SPDX-License-Identifier: GPL-3.0-or-later
//
// Menu separator in the Niratan control style: a hairline inset from the
// menu's edges.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.MenuSeparator {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 5
    leftPadding: 10
    rightPadding: 10

    contentItem: Rectangle {
        implicitWidth: 160
        implicitHeight: 1
        color: Theme.separator
    }
}
