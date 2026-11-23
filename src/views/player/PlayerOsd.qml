// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player's on-screen display, following
// Features/Video/VideoPlayerScreen+OSD.swift: 30 from the top and 28 from the
// leading edge of the video stage, a title with an optional value and a meter
// under them, fading out with the page's OSD timer. The panel behind it is a
// floating theme surface so the text stays readable in both themes.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

Rectangle {
    id: osd

    property string title
    property string value
    property real meter: -1

    anchors.topMargin: 30
    anchors.leftMargin: 28
    width: Math.max(osdRow.implicitWidth, meterBar.visible ? meterBar.width : 0) + 24
    height: meterBar.visible ? 46 : 34
    radius: 10
    color: Theme.floatingBackground
    border.width: Theme.cardBorderWidth
    border.color: Theme.floatingStroke
    opacity: osd.title.length > 0 ? 1 : 0
    visible: osd.title.length > 0 || opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: 120
        }
    }

    Row {
        id: osdRow

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 8
        anchors.leftMargin: 12
        spacing: 8

        Label {
            text: osd.title
            color: Theme.textPrimary
            font.pixelSize: 13
            font.bold: true
        }

        Label {
            text: osd.value
            color: Theme.textSecondary
            font.pixelSize: 13
        }
    }

    Rectangle {
        id: meterBar

        visible: osd.meter >= 0
        anchors.left: osdRow.left
        anchors.top: osdRow.bottom
        anchors.topMargin: 7
        width: 210
        height: 5
        radius: height / 2
        color: Theme.floatingStroke

        Rectangle {
            width: parent.width * Math.max(0, osd.meter)
            height: parent.height
            radius: parent.radius
            color: Theme.accent
        }
    }
}
