// SPDX-License-Identifier: GPL-3.0-or-later
//
// The surface of a floating overlay (menu, pop-up list, tool tip, dialog): the
// card fill with a hairline and a soft drop shadow, as macOS draws menus and
// sheets. The shadow is a blurred-looking pair of offset fills so the software
// renderer draws it too.
import QtQuick
import Niratan

Item {
    id: surface

    property real radius: 10
    property color color: Theme.cardBackground

    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        anchors.topMargin: 1
        anchors.bottomMargin: -4
        radius: surface.radius + 1
        color: Theme.shadow
        opacity: 0.35
    }
    Rectangle {
        anchors.fill: parent
        anchors.bottomMargin: -2
        radius: surface.radius
        color: Theme.shadow
        opacity: 0.5
    }
    Rectangle {
        anchors.fill: parent
        radius: surface.radius
        color: surface.color
        border.width: 1
        border.color: Theme.cardStroke
    }
}
