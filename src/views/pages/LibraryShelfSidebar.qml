// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Rectangle {
    id: sidebar
    required property LibraryViewModel model

    color: Theme.cardBackground
    border.width: 1
    border.color: Theme.cardStroke

    Dialog {
        id: shelfDialog
        parent: Overlay.overlay
        title: qsTr("New Shelf")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField {
            id: shelfField
            width: 240
            placeholderText: qsTr("Shelf name")
        }
        onAboutToShow: shelfField.text = qsTr("New Shelf")
        onAccepted: {
            const created = sidebar.model.createShelf(shelfField.text)
            if (created.length > 0)
                sidebar.model.selectedShelf = "shelf:" + created
        }
    }

    Dialog {
        id: renameShelfDialog
        property string fromName
        parent: Overlay.overlay
        title: qsTr("Rename Shelf")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField {
            id: renameShelfField
            width: 240
            onAccepted: renameShelfDialog.accept()
        }
        onAboutToShow: renameShelfField.text = renameShelfDialog.fromName
        onOpened: renameShelfField.forceActiveFocus()
        onAccepted: sidebar.model.renameShelf(renameShelfDialog.fromName, renameShelfField.text)
    }

    Dialog {
        id: deleteShelfDialog
        property string shelfName
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Overlay.overlay ? Math.min(420, Overlay.overlay.width - 32) : 420
        title: qsTr("Delete Shelf")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        contentItem: Label {
            text: qsTr("\"%1\" will be deleted. Books on this shelf stay in your library.").arg(deleteShelfDialog.shelfName)
            color: Theme.textPrimary
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
        onAccepted: sidebar.model.deleteShelf(deleteShelfDialog.shelfName)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 2
        Label {
            Layout.margins: 8
            text: "Niratan"
            color: Theme.textPrimary
            font.pixelSize: 20
            font.bold: true
        }

        Repeater {
            model: [
                { key: "all", label: qsTr("All Books") },
                { key: "reading", label: qsTr("Currently Reading") },
                { key: "unshelved", label: qsTr("Unshelved") }
            ]
            delegate: ItemDelegate {
                id: smartRow
                required property var modelData
                Layout.fillWidth: true
                text: smartRow.modelData.label
                highlighted: sidebar.model.selectedShelf === smartRow.modelData.key
                onClicked: sidebar.model.selectedShelf = smartRow.modelData.key
            }
        }

        Label {
            Layout.margins: 8
            text: qsTr("Shelves")
            color: Theme.textSecondary
            font.pixelSize: 12
        }

        Repeater {
            model: sidebar.model.shelves
            delegate: ItemDelegate {
                id: shelfRow
                required property var modelData
                Layout.fillWidth: true
                text: shelfRow.modelData.name + " (" + shelfRow.modelData.count + ")"
                highlighted: sidebar.model.selectedShelf === "shelf:" + shelfRow.modelData.name
                onClicked: sidebar.model.selectedShelf = "shelf:" + shelfRow.modelData.name
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: shelfMenu.popup()
                }
                Menu {
                    id: shelfMenu
                    MenuItem {
                        text: qsTr("Rename Shelf")
                        onClicked: {
                            renameShelfDialog.fromName = shelfRow.modelData.name
                            renameShelfDialog.open()
                        }
                    }
                    MenuItem {
                        text: qsTr("Delete Shelf")
                        onClicked: {
                            deleteShelfDialog.shelfName = shelfRow.modelData.name
                            deleteShelfDialog.open()
                        }
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
        Button {
            Layout.fillWidth: true
            text: qsTr("New Shelf")
            onClicked: shelfDialog.open()
        }
    }
}
