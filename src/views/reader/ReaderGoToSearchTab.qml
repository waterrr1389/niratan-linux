// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Search tab of the Go to panel, following
// Features/Reader/Search/ReaderGoToView.swift (searchTab, searchResultsContent,
// runSearch, clearSearch, groupedSearchResults, highlightedSnippet): the
// capsule search field, then the prompt before a query, Searching..., the
// failure state, No matches, or the grouped results. A result jumps to its
// character and closes the inspector through jumpRequested.
//
pragma ComponentBehavior: Bound

import QtQuick
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

        SearchBar {
            id: searchField
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.readerListInset
            fieldObjectName: "goToSearchField"
            placeholderText: qsTr("Search in this book")
            showSubmitButton: true
            submitEnabled: ReaderViewModel.hasSearchableText(text)
                           && ReaderViewModel.searchState !== 1
            onSubmitted: root.runSearch()
            onTextChanged: {
                if (!ReaderViewModel.hasSearchableText(text))
                    root.clearSearch();
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListStatus {
                visible: text.length > 0
                symbol: "magnifyingglass"
                busy: ReaderViewModel.searchState === 1
                text: {
                    const state = ReaderViewModel.searchState;
                    if (state === 0 && root.submittedQuery.length === 0)
                        return qsTr("Enter text to search this book");
                    if (state === 1)
                        return qsTr("Searching…");
                    if (state === 3)
                        return qsTr("Could not search this book");
                    if (state === 2 && root.submittedQuery.length > 0
                            && ReaderViewModel.searchResults.length === 0)
                        return qsTr("No matches");
                    return "";
                }
            }

            InspectorList {
                anchors.fill: parent
                anchors.leftMargin: Theme.readerListInset
                anchors.rightMargin: Theme.readerListInset
                anchors.bottomMargin: Theme.readerListBottomInset
                visible: ReaderViewModel.searchResults.length > 0
                model: ReaderViewModel.searchResults
                spacing: Theme.readerListSpacing

                delegate: InspectorListRow {
                    id: searchRow
                    required property var modelData
                    width: ListView.view.width
                    objectName: "goToSearchRow"

                    onClicked: {
                        ReaderViewModel.jumpToCharacter(searchRow.modelData.character);
                        root.jumpRequested();
                    }
                    contentItem: ColumnLayout {
                        spacing: Theme.readerListDetailSpacing

                        StyledLabel {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            text: searchRow.modelData.before
                                  + "<span style=\"background-color:" + Theme.readerSearchMatch
                                  + "\">" + searchRow.modelData.match + "</span>"
                                  + searchRow.modelData.after
                            textStyle: Theme.calloutFont
                        }

                        StyledLabel {
                            Layout.fillWidth: true
                            text: ReaderViewModel.positionTextAt(searchRow.modelData.character)
                            secondary: true
                            textStyle: Theme.caption2Font
                            monospacedDigits: true
                            elide: Text.ElideRight
                        }
                    }

                }
            }
        }
    }
}
