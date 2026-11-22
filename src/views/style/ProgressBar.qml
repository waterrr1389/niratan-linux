// SPDX-License-Identifier: GPL-3.0-or-later
//
// Progress bar in the Niratan control style, after the macOS linear progress
// indicator: a thin rounded track with an accent fill, or a sliding accent
// segment while indeterminate.
import QtQuick
import QtQuick.Templates as T
import Niratan

T.ProgressBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    contentItem: Item {
        implicitWidth: 120
        implicitHeight: 4
        clip: true

        Rectangle {
            visible: !control.indeterminate
            x: control.mirrored ? parent.width - width : 0
            width: control.position * parent.width
            height: parent.height
            radius: height / 2
            color: Theme.accent
        }

        Rectangle {
            id: segment
            visible: control.indeterminate
            width: parent.width / 3
            height: parent.height
            radius: height / 2
            color: Theme.accent

            XAnimator on x {
                running: control.visible && control.indeterminate
                from: -segment.width
                to: segment.parent ? segment.parent.width : 0
                duration: 1200
                loops: Animation.Infinite
            }
        }
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 4
        y: (control.height - height) / 2
        height: 4
        radius: height / 2
        color: Theme.trackBackground
    }
}
