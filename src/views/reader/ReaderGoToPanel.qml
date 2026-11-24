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
// lines and the position uses tabular digits. Both alerts are kit
// AlertDialogs; Jump to's default button is Go, as in Swift.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: panel

    signal closeRequested()

    property int selectedTab: 1

    spacing: 0

    PanelHeader {
        Layout.fillWidth: true
        title: qsTr("Go to")
        onCloseRequested: panel.closeRequested()
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.panelHeaderPaddingHorizontal
        Layout.rightMargin: Theme.panelHeaderPaddingHorizontal
        Layout.bottomMargin: Theme.summaryPaddingBottom
        spacing: Theme.spacingMd

        Rectangle {
            objectName: "goToCover"
            Layout.alignment: Qt.AlignTop
            implicitWidth: Theme.summaryCoverWidth
            implicitHeight: Theme.summaryCoverHeight
            radius: Theme.summaryCoverRadius
            color: Theme.coverPlaceholder

            RoundedImage {
                anchors.fill: parent
                source: ReaderViewModel.coverUrl
                fillMode: Image.PreserveAspectCrop
                radius: Theme.summaryCoverRadius
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.summarySpacing

            StyledLabel {
                objectName: "goToTitle"
                Layout.fillWidth: true
                text: ReaderViewModel.title
                textStyle: Theme.headlineFont
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            ProgressTrack {
                Layout.fillWidth: true
                value: ReaderViewModel.progress
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                StyledLabel {
                    objectName: "goToPosition"
                    Layout.fillWidth: true
                    text: ReaderViewModel.positionText
                    textStyle: Theme.captionFont
                    secondary: true
                    monospacedDigits: true
                    elide: Text.ElideRight
                }

                FormButton {
                    objectName: "goToJumpButton"
                    text: qsTr("Jump to")
                    glyph: "arrow.right.to.line"
                    small: true
                    onClicked: {
                        jumpDialog.fieldText = "";
                        jumpDialog.open();
                    }
                }
            }
        }
    }

    UnderlineTabBar {
        id: tabs
        Layout.fillWidth: true
        Layout.bottomMargin: Theme.inspectorTabBarPaddingBottom
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

    AlertDialog {
        id: jumpDialog
        objectName: "goToJumpDialog"
        title: qsTr("Jump to")
        message: ReaderViewModel.positionText
        acceptText: qsTr("Go")
        rejectText: qsTr("Cancel")
        hasField: true
        fieldName: "goToJumpField"
        fieldPlaceholder: ReaderViewModel.englishContent
                          ? qsTr("Word count") : qsTr("Character count")

        onOpened: jumpDialog.fieldText = ""
        onAccepted: {
            if (ReaderViewModel.jumpToDisplayCount(jumpDialog.fieldText))
                panel.closeRequested();
            else
                invalidDialog.open();
        }
    }

    AlertDialog {
        id: invalidDialog
        objectName: "goToInvalidDialog"
        title: qsTr("Invalid input")
        message: ReaderViewModel.englishContent
                 ? qsTr("Please enter a valid word count")
                 : qsTr("Please enter a valid character count")
    }
}
