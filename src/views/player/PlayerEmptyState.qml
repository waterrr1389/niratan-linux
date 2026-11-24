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
import QtQuick.Dialogs
import Niratan

Item {
    id: emptyState

    signal fileChosen(url file)

    Column {
        anchors.centerIn: parent
        width: Math.min(Theme.emptyStateWidth, Math.max(0, emptyState.width - Theme.emptyStateInset))
        spacing: Theme.spacingMd

        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: "play.rectangle"
            size: Theme.emptyStateGlyphSize
            color: Theme.overlayForeground
        }

        StyledLabel {
            width: parent.width
            text: qsTr("No Video Open")
            textStyle: Theme.title2Font
            weight: Theme.weightSemibold
            color: Theme.overlayForeground
            horizontalAlignment: Text.AlignHCenter
        }

        StyledLabel {
            width: parent.width
            text: qsTr("Open a local video file to start watching.")
            color: Theme.overlayForeground
            opacity: Theme.chromeOverlayTextOpacity
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        FormButton {
            objectName: "openVideoButton"
            anchors.horizontalCenter: parent.horizontalCenter
            sizeToLabel: true
            prominent: true
            glyph: "play.rectangle"
            text: qsTr("Open Video")
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
