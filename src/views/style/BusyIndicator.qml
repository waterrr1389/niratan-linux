// SPDX-License-Identifier: GPL-3.0-or-later
//
// Busy indicator in the Niratan control style, after the macOS spinning
// progress indicator: twelve spokes in the secondary label colour, fading
// around the circle while the wheel turns.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import Niratan

T.BusyIndicator {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 2

    contentItem: Item {
        id: wheel

        readonly property int spokes: 12

        implicitWidth: 20
        implicitHeight: 20
        opacity: control.running ? 1 : 0

        Behavior on opacity {
            OpacityAnimator { duration: 250 }
        }

        Repeater {
            model: wheel.spokes

            Rectangle {
                id: spoke

                required property int index

                x: (wheel.width - width) / 2
                y: 0
                width: Math.max(1.5, wheel.width / 11)
                height: wheel.height * 0.28
                radius: width / 2
                color: Theme.textSecondary
                opacity: 1 - spoke.index / wheel.spokes * 0.85
                transform: Rotation {
                    origin.x: spoke.width / 2
                    origin.y: wheel.height / 2
                    angle: -spoke.index * 360 / wheel.spokes
                }
            }
        }

        RotationAnimator on rotation {
            running: control.running && control.visible
            from: 0
            to: 360
            duration: 1000
            loops: Animation.Infinite
        }
    }
}
