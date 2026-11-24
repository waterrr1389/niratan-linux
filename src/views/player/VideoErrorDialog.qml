// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player's error alert, following Features/Video/VideoPlayerScreen.swift's
// "Video Error" alert. The page's load, subtitle and playback failures all
// reach this one alert; it stays closed until showError receives a non-empty
// message, and closing returns focus to the page so the player's shortcuts
// keep working. It is the kit's alert with a single OK button.
pragma ComponentBehavior: Bound

import QtQuick
import Niratan

AlertDialog {
    id: dialog

    function showError(error: string) {
        if (error.length > 0) {
            dialog.message = error
            dialog.open()
        }
    }

    // The item that regains focus when the alert closes: the player page.
    property Item focusTarget

    title: qsTr("Video Error")
    onClosed: {
        if (dialog.focusTarget)
            dialog.focusTarget.forceActiveFocus()
    }
}
