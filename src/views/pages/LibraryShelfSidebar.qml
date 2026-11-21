// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Rectangle {
    id: sidebar
    required property LibraryViewModel model
    signal createRequested()
    signal renameRequested(string name)

    color: Theme.cardBackground
    border.width: 1
    border.color: Theme.cardStroke
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
                        onClicked: sidebar.renameRequested(shelfRow.modelData.name)
                    }
                    MenuItem {
                        text: qsTr("Delete Shelf")
                        onClicked: sidebar.model.deleteShelf(shelfRow.modelData.name)
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
        Button {
            Layout.fillWidth: true
            text: qsTr("New Shelf")
            onClicked: sidebar.createRequested()
        }
    }
}
