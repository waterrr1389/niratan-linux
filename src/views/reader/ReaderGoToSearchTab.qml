// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Search tab of the Go to panel, following
// Features/Reader/Search/ReaderGoToView.swift (searchTab, searchResultsContent,
// runSearch, clearSearch, groupedSearchResults, highlightedSnippet): the
// capsule search field, then the prompt before a query, Searching..., the
// failure state, No matches, or the grouped results. A result jumps to its
// character and closes the inspector through jumpRequested.
//
// Result rows follow NativeReaderInspectorRowBody: 9 px radius with a primary
// tint at .06 on hover and .1 while pressed.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: root

    signal jumpRequested()

    property string submittedQuery: ""

    function runSearch() {
        if (!ReaderViewModel.hasSearchableText(searchField.text)) {
            clearSearch();
            return;
        }
        root.submittedQuery = searchField.text;
        ReaderViewModel.searchBook(searchField.text);
    }

    function clearSearch() {
        root.submittedQuery = "";
        ReaderViewModel.clearBookSearch();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.bottomMargin: 10
            spacing: Theme.spacingSm

            Icon {
                name: "magnifyingglass"
                size: 14
                color: Theme.textSecondary
            }

            TextField {
                id: searchField
                objectName: "goToSearchField"
                Layout.fillWidth: true
                placeholderText: qsTr("Search in this book")
                onAccepted: root.runSearch()
                onTextChanged: {
                    if (!ReaderViewModel.hasSearchableText(text))
                        root.clearSearch();
                }
            }

            IconButton {
                icon: "magnifyingglass"
                diameter: 28
                iconSize: 13
                accessibleName: qsTr("Search")
                enabled: ReaderViewModel.hasSearchableText(searchField.text)
                         && ReaderViewModel.searchState !== 1
                onClicked: root.runSearch()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            BusyIndicator {
                anchors.centerIn: parent
                running: ReaderViewModel.searchState === 1
                visible: running
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 36
                visible: root.submittedQuery.length === 0 && ReaderViewModel.searchState === 0
                text: qsTr("Enter text to search this book")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 36
                visible: ReaderViewModel.searchState === 1
                text: qsTr("Searching…")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 36
                visible: ReaderViewModel.searchState === 3
                text: qsTr("Could not search this book")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 36
                visible: root.submittedQuery.length > 0
                         && ReaderViewModel.searchState === 2
                         && ReaderViewModel.searchResults.length === 0
                text: qsTr("No matches")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                horizontalAlignment: Text.AlignHCenter
            }

            ListView {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.bottomMargin: 14
                clip: true
                visible: ReaderViewModel.searchResults.length > 0
                model: ReaderViewModel.searchResults
                spacing: 2

                delegate: Item {
                    id: searchRow
                    required property var modelData
                    width: ListView.view.width
                    implicitHeight: searchRowContent.implicitHeight + 14
                    objectName: "goToSearchRow"

                    signal clicked()

                    onClicked: {
                        ReaderViewModel.jumpToCharacter(searchRow.modelData.character);
                        root.jumpRequested();
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 9
                        color: searchMouse.pressed
                            ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                                      Theme.textPrimary.b, 0.1)
                            : searchMouse.containsMouse
                                ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                                          Theme.textPrimary.b, 0.06)
                                : "transparent"
                    }

                    ColumnLayout {
                        id: searchRowContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 3

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            text: searchRow.modelData.before
                                  + "<span style=\"background-color:"
                                  + Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                                  + "\">" + searchRow.modelData.match + "</span>"
                                  + searchRow.modelData.after
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                        }

                        Text {
                            Layout.fillWidth: true
                            text: ReaderViewModel.positionTextAt(searchRow.modelData.character)
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            font.features: ({ "tnum": 1 })
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: searchMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: searchRow.clicked()
                    }
                }
            }
        }
    }
}
