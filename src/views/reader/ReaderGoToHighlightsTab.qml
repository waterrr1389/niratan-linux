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

    InspectorList {
        anchors.fill: parent
        anchors.leftMargin: Theme.readerListInset
        anchors.rightMargin: Theme.readerListInset
        anchors.bottomMargin: Theme.readerListBottomInset
        spacing: Theme.readerListSpacing
        model: ReaderViewModel.highlights
        section.property: "label"
        section.delegate: ListSectionHeader {
            required property string section
            width: ListView.view.width
            // highlightsTab draws no header for an unnamed chapter.
            visible: section.length > 0
            height: visible ? implicitHeight : 0
            compact: true
            title: section
        }

        delegate: InspectorListRow {
            id: highlightRow
            required property var modelData
            width: ListView.view.width
            objectName: "goToHighlightRow"

            hasContextMenu: true
            onContextMenuRequested: highlightMenu.popup()

            onClicked: {
                ReaderViewModel.jumpToCharacter(highlightRow.modelData.character);
                root.jumpRequested();
            }

            contentItem: RowLayout {
                spacing: Theme.spacingSm

                Rectangle {
                    Layout.fillHeight: true
                    Layout.topMargin: Theme.readerListSpacing
                    Layout.bottomMargin: Theme.readerListSpacing
                    implicitWidth: Theme.readerHighlightWidth
                    radius: Theme.focusRingWidth
                    color: Theme.accent
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.readerListDetailSpacing

                    StyledLabel {
                        Layout.fillWidth: true
                        text: highlightRow.modelData.text.trim()
                        textStyle: Theme.calloutFont
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                    }

                    StyledLabel {
                        Layout.fillWidth: true
                        text: ReaderViewModel.positionTextAt(highlightRow.modelData.character)
                        secondary: true
                        textStyle: Theme.caption2Font
                        elide: Text.ElideRight
                    }
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

    ListStatus {
        visible: ReaderViewModel.highlights.length === 0
        text: qsTr("No Highlights")
        symbol: "highlighter"
    }
}
