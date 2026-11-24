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

        delegate: InspectorListRow {
            id: chapterRow
            required property var modelData
            width: ListView.view.width
            objectName: "goToChapterRow"
            selected: ListView.isCurrentItem
            leftPadding: Theme.readerListInset + Theme.readerChapterIndent * modelData.level

            onClicked: {
                ReaderViewModel.goToContentsEntry(chapterRow.modelData.index,
                                                 chapterRow.modelData.fragment);
                root.jumpRequested();
            }

            contentItem: RowLayout {
                spacing: Theme.spacingSm

                StyledLabel {
                    Layout.fillWidth: true
                    text: chapterRow.modelData.title
                    textStyle: Theme.calloutFont
                    secondary: chapterRow.modelData.level > 0
                    weight: secondary ? Theme.weightRegular : Theme.weightMedium
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                StyledLabel {
                    visible: chapterRow.modelData.characterStart >= 0
                    text: (ReaderViewModel.englishContent
                           ? Math.ceil(chapterRow.modelData.characterStart / 5)
                           : chapterRow.modelData.characterStart)
                    secondary: true
                    textStyle: Theme.caption2Font
                    monospacedDigits: true
                }
            }
        }
    }

    ListStatus {
        visible: ReaderViewModel.chapters.length === 0
        text: qsTr("No Chapters")
        symbol: "list.bullet"
    }
}
