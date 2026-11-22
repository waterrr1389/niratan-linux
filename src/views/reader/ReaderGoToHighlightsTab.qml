// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Highlights tab of the Go to panel, following
// Features/Reader/Search/ReaderGoToView.swift (highlightsTab,
// highlightSections): highlights grouped by their chapter label, each row
// showing the trimmed text beside its colour bar and the display-unit
// position. Choosing a row jumps and closes the inspector through
// jumpRequested; the row's context menu deletes it and keeps the inspector
// open. An empty book shows "No Highlights".

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: root

    signal jumpRequested()

    ListView {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 14
        clip: true
        spacing: 2
        model: ReaderViewModel.highlights
        section.property: "label"
        section.delegate: Text {
            required property string section
            width: ListView.view.width
            text: section
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.bold: true
            leftPadding: 10
            topPadding: 10
            bottomPadding: 2
        }

        delegate: Item {
            id: highlightRow
            required property var modelData
            width: ListView.view.width
            implicitHeight: highlightContent.implicitHeight + 14
            objectName: "goToHighlightRow"

            signal clicked()

            onClicked: {
                ReaderViewModel.jumpToCharacter(highlightRow.modelData.character);
                root.jumpRequested();
            }

            Rectangle {
                anchors.fill: parent
                radius: 9
                color: highlightMouse.pressed
                    ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                              Theme.textPrimary.b, 0.1)
                    : highlightMouse.containsMouse
                        ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                                  Theme.textPrimary.b, 0.06)
                        : "transparent"
            }

            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 9
                anchors.bottomMargin: 9
                width: 3
                radius: 2
                color: Theme.accent
            }

            ColumnLayout {
                id: highlightContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 21
                anchors.rightMargin: 10
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: highlightRow.modelData.text.trim()
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.Wrap
                    maximumLineCount: 4
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: ReaderViewModel.positionTextAt(highlightRow.modelData.character)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                id: highlightMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
                    if (mouse.button === Qt.RightButton)
                        highlightMenu.popup();
                    else
                        highlightRow.clicked();
                }
            }

            Menu {
                id: highlightMenu
                MenuItem {
                    text: qsTr("Delete")
                    onTriggered: ReaderViewModel.removeHighlight(highlightRow.modelData.id)
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - 36
        visible: ReaderViewModel.highlights.length === 0
        text: qsTr("No Highlights")
        color: Theme.textSecondary
        font.pixelSize: Theme.fontBody
        horizontalAlignment: Text.AlignHCenter
    }
}
