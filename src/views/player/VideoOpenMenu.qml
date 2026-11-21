// SPDX-License-Identifier: GPL-3.0-or-later
// VideoLibraryView.swift: open(_:fromBeginning:) dispatches both open intents.

pragma ComponentBehavior: Bound

import QtQuick.Controls
import QtQuick.Dialogs

Menu {
    id: menu

    required property string extensionPattern
    signal videoChosen(url file, bool fromBeginning)
    signal selectionCanceled()

    MenuItem {
        text: qsTr("Open Video")
        onTriggered: picker.choose(false)
    }
    MenuItem {
        text: qsTr("Play from Beginning")
        onTriggered: picker.choose(true)
    }

    FileDialog {
        id: picker
        property bool fromBeginning: false

        function choose(fromBeginning: bool) {
            picker.fromBeginning = fromBeginning
            picker.open()
        }

        title: fromBeginning ? qsTr("Play from Beginning") : qsTr("Open Video")
        nameFilters: [
            qsTr("Video and audio files (%1)").arg(menu.extensionPattern),
            qsTr("All files (*)")
        ]
        fileMode: FileDialog.OpenFile
        onAccepted: menu.videoChosen(picker.selectedFile, picker.fromBeginning)
        onRejected: menu.selectionCanceled()
    }
}
