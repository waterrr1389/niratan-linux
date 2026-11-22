// SPDX-License-Identifier: GPL-3.0-or-later
//
// An image with rounded corners, as SwiftUI's clipShape(RoundedRectangle)
// draws covers and the sidebar icon. A rounded clip needs a shader, which the
// software scene graph does not draw, so the corners are covered instead: a
// frame in `maskColor`, the uniform colour around the image, whose inner edge
// is the rounded rectangle. The frame reaches `maskWidth` past the image on
// every side, so the surround must be that colour at least that far out or be
// drawn over afterwards; it covers the corners while `maskWidth` is at least
// 0.42 of `radius`.
import QtQuick
import Niratan

Item {
    id: rounded

    property alias source: image.source
    property alias fillMode: image.fillMode
    property alias sourceSize: image.sourceSize
    property alias status: image.status
    property real radius: 6
    property real maskWidth: radius
    property color maskColor: Theme.cardBackground

    Image {
        id: image

        anchors.fill: parent
        asynchronous: true
        smooth: true
        mipmap: true
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -rounded.maskWidth
        radius: rounded.radius + rounded.maskWidth
        color: "transparent"
        border.width: rounded.maskWidth
        border.color: rounded.maskColor
    }
}
