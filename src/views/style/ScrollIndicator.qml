// SPDX-License-Identifier: GPL-3.0-or-later
//
// Non-interactive scroll indicator in the Niratan control style: the overlay
// scroll bar's thumb, thinner.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.ScrollIndicator {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 3

    contentItem: Rectangle {
        implicitWidth: 3
        implicitHeight: 3
        radius: width / 2
        color: Theme.textSecondary
        visible: control.size < 1.0
        opacity: 0.0

        states: State {
            name: "active"
            when: control.active
            PropertyChanges { control.contentItem.opacity: 0.75 }
        }
        transitions: Transition {
            from: "active"
            SequentialAnimation {
                PauseAnimation { duration: 600 }
                NumberAnimation {
                    target: control.contentItem
                    duration: 200
                    property: "opacity"
                    to: 0.0
                }
            }
        }
    }
}
