// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video library's dialogs, following
// Features/Video/VideoLibraryView.swift `body`'s Error alert and Delete
// Collection alert, `deleteCollectionAction`, `deleteCollection` and
// `presentFolderImporter`.
//
// The Error alert shows the view model's error with OK and clears it on
// dismissal. The Delete Collection confirmation has the title "Delete
// Collection?", the message "This removes the collection but keeps its videos
// in your library.", a destructive "Delete Collection" and Cancel; confirming
// removes only the collection. The folder picker chooses a directory and hands
// it to the page.
//
// The alerts are page-level overlays rather than Popups so their content stays
// inside the page's item tree; the scrim dims the library behind them.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Niratan

Item {
    id: dialogs

    property var model

    signal foldersChosen(var folders)

    function showError(message) {
        if (message.length > 0) {
            errorDialog.visible = true
            errorDialog.forceActiveFocus()
        }
    }

    function askDeleteCollection(collectionId) {
        deleteCollectionDialog.collectionId = collectionId
        deleteCollectionDialog.visible = true
        deleteCollectionDialog.forceActiveFocus()
    }

    function openFolderPicker() {
        folderDialog.open()
    }

    // The Error alert.
    Item {
        id: errorDialog

        objectName: "errorDialog"
        property string title: qsTr("Error")

        anchors.fill: parent
        z: 10
        visible: false
        focus: true

        function accept() {
            errorDialog.visible = false
            if (dialogs.model)
                dialogs.model.clearError()
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.windowBackground
            opacity: 0.7
        }

        // Swallows clicks that would otherwise reach the library behind the
        // alert; the card below stays interactive.
        MouseArea {
            anchors.fill: parent
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(400, dialogs.width - 32)
            height: Math.max(120, messageLabel.implicitHeight + 108)
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.width: 1
            border.color: Theme.cardStroke

            Text {
                id: errorTitle

                x: 20
                y: 18
                text: errorDialog.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontTitle2
                font.weight: Font.DemiBold
            }

            Text {
                id: messageLabel

                x: 20
                y: errorTitle.y + errorTitle.height + 10
                width: parent.width - 40
                text: dialogs.model ? dialogs.model.errorMessage : ""
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }

            Button {
                objectName: "errorDialogOk"
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 16
                text: qsTr("OK")
                onClicked: errorDialog.accept()
            }
        }

        Keys.onEscapePressed: errorDialog.accept()
    }

    // The Delete Collection confirmation.
    Item {
        id: deleteCollectionDialog

        objectName: "deleteCollectionDialog"
        property string collectionId
        property string title: qsTr("Delete Collection?")

        anchors.fill: parent
        z: 10
        visible: false
        focus: true

        function accept() {
            if (dialogs.model && deleteCollectionDialog.collectionId.length > 0)
                dialogs.model.removeCollection(deleteCollectionDialog.collectionId)
            deleteCollectionDialog.visible = false
        }

        function reject() {
            deleteCollectionDialog.visible = false
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.windowBackground
            opacity: 0.7
        }

        // Swallows clicks that would otherwise reach the library behind the
        // confirmation.
        MouseArea {
            anchors.fill: parent
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(400, dialogs.width - 32)
            height: Math.max(150, deleteMessage.implicitHeight + 118)
            radius: Theme.cardRadius
            color: Theme.cardBackground
            border.width: 1
            border.color: Theme.cardStroke

            Text {
                id: deleteTitle

                x: 20
                y: 18
                text: deleteCollectionDialog.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontTitle2
                font.weight: Font.DemiBold
            }

            Text {
                id: deleteMessage

                x: 20
                y: deleteTitle.y + deleteTitle.height + 10
                width: parent.width - 40
                text: qsTr("This removes the collection but keeps its videos in your library.")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
            }

            Row {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 16
                spacing: 8

                Button {
                    objectName: "cancelDeleteCollection"
                    text: qsTr("Cancel")
                    onClicked: deleteCollectionDialog.reject()
                }

                Button {
                    objectName: "confirmDeleteCollection"
                    text: qsTr("Delete Collection")
                    highlighted: true
                    onClicked: deleteCollectionDialog.accept()
                }
            }
        }

        Keys.onEscapePressed: deleteCollectionDialog.reject()
    }

    FolderDialog {
        id: folderDialog

        objectName: "folderDialog"
        title: qsTr("Add Video Folder")
        acceptLabel: qsTr("Add")

        onAccepted: dialogs.foldersChosen([selectedFolder])
    }
}
