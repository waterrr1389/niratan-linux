// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player's empty state, following VideoPlayerScreen.swift (the no-video
// screen) and the floating theme tokens the player uses over the black stage:
// the title, Open a video, Video Settings and the supported file list.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

Rectangle {
    id: emptyState

    signal openRequested()
    signal settingsRequested()

    implicitWidth: 440
    implicitHeight: emptyColumn.implicitHeight + 40
    radius: Theme.cardRadius
    color: Theme.floatingBackground
    border.width: Theme.cardBorderWidth
    border.color: Theme.floatingStroke

    Column {
        id: emptyColumn
        anchors.centerIn: parent
        width: parent.width - 32
        spacing: 10

        Label {
            width: parent.width
            text: qsTr("No Video Open")
            color: Theme.textPrimary
            font.pixelSize: 18
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Button {
            width: parent.width
            text: qsTr("Open a video")
            onClicked: emptyState.openRequested()
        }

        Button {
            width: parent.width
            text: qsTr("Video Settings")
            onClicked: emptyState.settingsRequested()
        }

        Label {
            width: parent.width
            text: qsTr("Supported files: %1").arg(emptyState.extensionList)
            color: Theme.textSecondary
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    property string extensionList: ""
}
