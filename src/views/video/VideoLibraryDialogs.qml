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
            opacity: Theme.alertScrimOpacity
        }

        // Swallows clicks that would otherwise reach the library behind the
        // alert; the card below stays interactive.
        MouseArea {
            anchors.fill: parent
        }

        AlertCard {
            title: errorDialog.title
            message: dialogs.model ? dialogs.model.errorMessage : ""

            FormButton {
                objectName: "errorDialogOk"
                text: qsTr("OK")
                prominent: true
                sizeToLabel: true
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
            opacity: Theme.alertScrimOpacity
        }

        // Swallows clicks that would otherwise reach the library behind the
        // confirmation.
        MouseArea {
            anchors.fill: parent
        }

        AlertCard {
            title: deleteCollectionDialog.title
            message: qsTr("This removes the collection but keeps its videos in your library.")

            FormButton {
                objectName: "cancelDeleteCollection"
                text: qsTr("Cancel")
                sizeToLabel: true
                onClicked: deleteCollectionDialog.reject()
            }

            FormButton {
                objectName: "confirmDeleteCollection"
                text: qsTr("Delete Collection")
                destructive: true
                sizeToLabel: true
                onClicked: deleteCollectionDialog.accept()
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

    // The alert card, laid out as the kit's AlertDialog: a headline title,
    // the secondary message and the trailing buttons given as children.
    component AlertCard: Rectangle {
        id: alertCard

        property string title
        property string message
        default property alias buttons: buttonRow.data

        anchors.centerIn: parent
        width: Math.min(Theme.alertWidth, parent.width - 2 * Theme.spacingLg)
        height: alertColumn.implicitHeight + 2 * Theme.alertPadding
        radius: Theme.cardRadius
        color: Theme.cardBackground
        border.width: Theme.hairlineWidth
        border.color: Theme.cardStroke

        Column {
            id: alertColumn

            anchors.fill: parent
            anchors.margins: Theme.alertPadding
            spacing: Theme.alertSpacing

            StyledLabel {
                width: parent.width
                text: alertCard.title
                textStyle: Theme.headlineFont
                wrapMode: Text.Wrap
            }

            StyledLabel {
                width: parent.width
                text: alertCard.message
                textFormat: Text.PlainText
                textStyle: Theme.footnoteFont
                secondary: true
                wrapMode: Text.Wrap
            }

            Row {
                id: buttonRow

                anchors.right: parent.right
                spacing: Theme.capsuleSpacing
            }
        }
    }
}
