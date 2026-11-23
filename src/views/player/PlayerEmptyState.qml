// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player's no-video surface, following
// Features/Video/VideoPlayerScreen.swift `videoSurface`'s no-video
// ContentUnavailableView ("No Video Open" with the play.rectangle symbol,
// "Open a local video file to start watching." and "Open Video"). The
// settings button and the supported-file list are not part of the Swift empty
// view and are not drawn. The loading indicator is PlayerLoadingOverlay.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: emptyState

    signal fileChosen(url file)

    Column {
        anchors.centerIn: parent
        width: Math.min(440, Math.max(0, emptyState.width - 64))
        spacing: 12

        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: "play.rectangle"
            size: 40
            color: Theme.overlayForeground
        }

        Label {
            width: parent.width
            text: qsTr("No Video Open")
            color: Theme.overlayForeground
            font.pixelSize: Theme.fontTitle2
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            width: parent.width
            text: qsTr("Open a local video file to start watching.")
            color: Theme.overlayForeground
            opacity: 0.86
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Button {
            objectName: "openVideoButton"
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Open Video")
            icon.source: IconMap.source("play.rectangle")
            onClicked: openVideoDialog.open()
        }
    }

    FileDialog {
        id: openVideoDialog

        objectName: "openVideoDialog"
        title: qsTr("Open Video")
        fileMode: FileDialog.OpenFile
        onAccepted: emptyState.fileChosen(selectedFile)
    }
}
