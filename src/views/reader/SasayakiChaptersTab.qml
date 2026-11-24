// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Chapters tab of the Sasayaki panel, following
// Features/Sasayaki/SasayakiSheet.swift (chaptersTab, formatChapterTime) and
// NativeMac/NativeReaderInspector.swift (NativeReaderInspectorRowBody: 9 px
// radius, accent 0.18 fill when selected, primary 0.06 on hover, 10/7 padding).
//
// The row marker is the current chapter's speaker glyph in the accent colour
// at 11 px, otherwise a 4 px circle at half-opacity secondary, both in a 14 px
// column. The title stays semibold while current and wraps to two lines; the
// start time uses the monospaced caption style and sits at least 8 px away.
// A click seeks to the chapter's start and clears a pending stop, through
// SasayakiViewModel.seekToAudiobookChapter.
//
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

Item {
    id: root

    required property SasayakiViewModel player

    function formatChapterTime(seconds) {
        var total = Math.max(0, Math.floor(seconds));
        var hours = Math.floor(total / 3600);
        var minutes = Math.floor((total % 3600) / 60);
        var rest = total % 60;
        function pad(value) {
            return (value < 10 ? "0" : "") + value;
        }
        return hours > 0 ? hours + ":" + pad(minutes) + ":" + pad(rest) : minutes + ":" + pad(rest);
    }

    component ChapterRow: InspectorListRow {
        id: chapterRow

        required property var modelData
        required property int index

        readonly property bool current: root.player.currentAudiobookChapterId === chapterRow.modelData.id
        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
        selected: current
        Accessible.name: modelData.title
        onClicked: root.player.seekToAudiobookChapter(chapterRow.modelData.id)

        contentItem: RowLayout {
            spacing: Theme.spacingSm

            Item {
                Layout.preferredWidth: Theme.readerChapterIndent
                Layout.preferredHeight: Theme.readerChapterMarkerHeight

                Icon {
                    anchors.centerIn: parent
                    visible: chapterRow.current
                    name: "speaker.wave.2.fill"
                    size: Theme.readerChapterMarkerGlyph
                    color: Theme.accent
                }

                Rectangle {
                    anchors.centerIn: parent
                    visible: !chapterRow.current
                    implicitWidth: Theme.spacingXs
                    implicitHeight: implicitWidth
                    radius: Theme.rowSubtitleSpacing
                    color: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g,
                                   Theme.textSecondary.b, 0.5)
                }
            }

            StyledLabel {
                Layout.fillWidth: true
                text: chapterRow.modelData.title
                textStyle: Theme.calloutFont
                weight: chapterRow.current ? Theme.weightSemibold : Theme.weightRegular
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            StyledLabel {
                Layout.leftMargin: Theme.spacingSm
                Layout.alignment: Qt.AlignVCenter
                text: root.formatChapterTime(chapterRow.modelData.startTime)
                secondary: true
                textStyle: Theme.captionFont
                monospacedDigits: true
            }
        }
    }

    // ContentUnavailableView's list.bullet glyph over its title, centred.
    ListStatus {
        visible: !root.player.isLoadingAudiobookChapters && root.player.chapters.length === 0
        symbol: "list.bullet"
        text: qsTr("No Chapters")
    }

    ListStatus {
        visible: root.player.isLoadingAudiobookChapters
        busy: true
        text: qsTr("Loading Chapters...")
    }

    InspectorList {
        anchors.fill: parent
        visible: !root.player.isLoadingAudiobookChapters && root.player.chapters.length > 0
        model: root.player.chapters
        spacing: Theme.readerListSpacing
        leftMargin: Theme.readerListInset
        rightMargin: Theme.readerListInset
        bottomMargin: Theme.readerListBottomInset

        delegate: ChapterRow {}
    }
}
