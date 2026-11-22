// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Chapters tab of the Go to panel, following
// Features/Reader/Search/ReaderGoToView.swift (chaptersTab, scrollToCurrentChapter)
// and Features/Reader/ChapterListView/ChapterListViewModel.swift
// (generateRows, flattenTOC): one row per table-of-contents entry with the
// indent level, the display-unit character count, and the current chapter
// selected. Choosing a row jumps to its spine item and fragment and closes the
// inspector through jumpRequested.
//
// ReaderViewModel.chapters supplies the table-of-contents rows; a book without
// a table of contents has none and shows No Chapters, as ChapterListViewModel
// produces no rows from an empty TOC.

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
        model: ReaderViewModel.chapters
        currentIndex: {
            const offset = ReaderViewModel.characterOffset;
            let best = -1;
            let start = 0;
            for (let i = 0; i < model.length; ++i) {
                const candidate = model[i].characterStart;
                if (candidate >= start && candidate <= offset) {
                    best = i;
                    start = candidate;
                }
            }
            return best;
        }

        delegate: Item {
            id: chapterRow
            required property var modelData
            width: ListView.view.width
            implicitHeight: chapterRowContent.implicitHeight + 14
            objectName: "goToChapterRow"
            property bool selected: ListView.isCurrentItem

            signal clicked()

            onClicked: {
                ReaderViewModel.goToContentsEntry(chapterRow.modelData.index,
                                                 chapterRow.modelData.fragment);
                root.jumpRequested();
            }

            Rectangle {
                anchors.fill: parent
                radius: 9
                color: chapterRow.selected
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                              chapterMouse.pressed ? 0.26 : 0.18)
                    : chapterMouse.pressed
                        ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                                  Theme.textPrimary.b, 0.1)
                        : chapterMouse.containsMouse
                            ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g,
                                      Theme.textPrimary.b, 0.06)
                            : "transparent"
            }

            RowLayout {
                id: chapterRowContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10 + 14 * chapterRow.modelData.level
                anchors.rightMargin: 10
                spacing: Theme.spacingSm

                Text {
                    Layout.fillWidth: true
                    text: chapterRow.modelData.title
                    color: chapterRow.modelData.level > 0 ? Theme.textSecondary
                                                          : Theme.textPrimary
                    font.pixelSize: Theme.fontBody
                    font.weight: chapterRow.modelData.level > 0 ? Font.Normal
                                                                : Font.Medium
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                Text {
                    visible: chapterRow.modelData.characterStart >= 0
                    text: (ReaderViewModel.englishContent
                           ? Math.ceil(chapterRow.modelData.characterStart / 5)
                           : chapterRow.modelData.characterStart)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    font.features: ({ "tnum": 1 })
                }
            }

            MouseArea {
                id: chapterMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: chapterRow.clicked()
            }
        }
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - 36
        visible: ReaderViewModel.chapters.length === 0
        text: qsTr("No Chapters")
        color: Theme.textSecondary
        font.pixelSize: Theme.fontBody
        horizontalAlignment: Text.AlignHCenter
    }
}
