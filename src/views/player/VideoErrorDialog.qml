// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player's error alert, following Features/Video/VideoPlayerScreen.swift's
// "Video Error" alert. The page's load, subtitle and playback failures all
// reach this one alert; it stays closed until showError receives a non-empty
// message, and closing returns focus to the page so the player's shortcuts
// keep working.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Dialog {
    id: dialog

    property string message

    function showError(error: string) {
        if (error.length > 0) {
            dialog.message = error
            dialog.open()
        }
    }

    anchors.centerIn: parent
    width: parent ? Math.min(440, parent.width) : 440
    modal: true
    focus: true
    title: qsTr("Video Error")
    standardButtons: Dialog.Ok
    onClosed: {
        if (dialog.parent)
            dialog.parent.forceActiveFocus()
    }

    contentItem: Label {
        text: dialog.message
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
}
