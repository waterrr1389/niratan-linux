// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's Go to side panel, following
// Features/Reader/Search/ReaderGoToView.swift (body, bookSummary,
// selectedContent, the Jump to alert and the Invalid input alert) with the
// header and tabs of NativeMac/NativeReaderInspector.swift.
//
// The panel opens on Chapters. The summary is the cover, the two-line title,
// the book progress and the localized position with Jump to. The Search,
// Chapters and Highlights contents live in ReaderGoToSearchTab,
// ReaderGoToChaptersTab and ReaderGoToHighlightsTab; a chapter, search result
// or highlight row closes the panel through jumpRequested, while deleting a
// highlight keeps it open.
//
// The Jump to alert shows the current position, Cancel changes nothing, and a
// count that is not a non-negative integer raises Invalid input; a valid count
// jumps through ReaderViewModel.jumpToDisplayCount and closes the panel.
//
// The summary cover is 46x66 with a 6 px radius; the title takes at most two
// lines and the position uses tabular digits.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: panel

    signal closeRequested()

    property int selectedTab: 1

    spacing: 0

    ReaderInspectorHeader {
        Layout.fillWidth: true
        title: qsTr("Go to")
        onCloseRequested: panel.closeRequested()
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 18
        Layout.rightMargin: 18
        Layout.bottomMargin: 14
        spacing: Theme.spacingMd

        Rectangle {
            objectName: "goToCover"
            implicitWidth: 46
            implicitHeight: 66
            radius: 6
            color: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g,
                           Theme.textSecondary.b, 0.18)
            clip: true

            Image {
                anchors.fill: parent
                source: ReaderViewModel.coverUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                objectName: "goToTitle"
                Layout.fillWidth: true
                text: ReaderViewModel.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                font.bold: true
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: ReaderViewModel.progress
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                Text {
                    objectName: "goToPosition"
                    Layout.fillWidth: true
                    text: ReaderViewModel.positionText
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    font.features: ({ "tnum": 1 })
                    elide: Text.ElideRight
                }

                Button {
                    objectName: "goToJumpButton"
                    text: qsTr("Jump to")
                    font.pixelSize: Theme.fontCaption
                    onClicked: {
                        jumpDialog.jumpInput = "";
                        jumpDialog.open();
                    }
                }
            }
        }
    }

    ReaderInspectorTabBar {
        id: tabs
        Layout.fillWidth: true
        Layout.bottomMargin: 10
        tabs: [
            { text: qsTr("Search"), icon: "magnifyingglass" },
            { text: qsTr("Chapters"), icon: "list.bullet" },
            { text: qsTr("Highlights"), icon: "highlighter" }
        ]
        currentIndex: panel.selectedTab
        onTabActivated: index => {
            panel.selectedTab = index;
        }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: panel.selectedTab

        ReaderGoToSearchTab {
            onJumpRequested: panel.closeRequested()
        }

        ReaderGoToChaptersTab {
            onJumpRequested: panel.closeRequested()
        }

        ReaderGoToHighlightsTab {
            onJumpRequested: panel.closeRequested()
        }
    }

    Dialog {
        id: jumpDialog
        objectName: "goToJumpDialog"
        modal: true
        implicitWidth: 320
        title: qsTr("Jump to")
        standardButtons: Dialog.Cancel | Dialog.Ok

        property alias jumpInput: jumpField.text

        onOpened: jumpField.text = ""
        onAccepted: {
            if (ReaderViewModel.jumpToDisplayCount(jumpField.text))
                panel.closeRequested();
            else
                invalidDialog.open();
        }

        contentItem: ColumnLayout {
            spacing: Theme.spacingSm

            TextField {
                id: jumpField
                objectName: "goToJumpField"
                Layout.fillWidth: true
                placeholderText: ReaderViewModel.englishContent
                                 ? qsTr("Word count") : qsTr("Character count")
            }

            Text {
                Layout.fillWidth: true
                text: ReaderViewModel.positionText
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
        }
    }

    Dialog {
        id: invalidDialog
        objectName: "goToInvalidDialog"
        modal: true
        implicitWidth: 320
        title: qsTr("Invalid input")
        standardButtons: Dialog.Ok

        contentItem: Text {
            text: ReaderViewModel.englishContent
                  ? qsTr("Please enter a valid word count")
                  : qsTr("Please enter a valid character count")
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }
    }
}
