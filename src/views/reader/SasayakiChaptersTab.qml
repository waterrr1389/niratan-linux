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
// SF Symbols are approximated by text glyphs, as elsewhere in the port.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
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

    component ChapterRow: Rectangle {
        id: chapterRow

        required property var modelData
        required property int index

        readonly property bool current: root.player.currentAudiobookChapterId === chapterRow.modelData.id
        readonly property bool hovered: hover.hovered

        Layout.fillWidth: true
        implicitHeight: content.implicitHeight + 14
        radius: 9
        color: chapterRow.current
               ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
               : (chapterRow.hovered
                  ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.06)
                  : "transparent")

        HoverHandler {
            id: hover
        }

        TapHandler {
            onTapped: root.player.seekToAudiobookChapter(chapterRow.modelData.id)
        }

        RowLayout {
            id: content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.topMargin: 7
            spacing: 8

            Item {
                Layout.preferredWidth: 14
                Layout.preferredHeight: 17

                Label {
                    anchors.centerIn: parent
                    visible: chapterRow.current
                    text: "\u266b"
                    color: Theme.accent
                    font.pixelSize: 11
                }

                Rectangle {
                    anchors.centerIn: parent
                    visible: !chapterRow.current
                    implicitWidth: 4
                    implicitHeight: 4
                    radius: 2
                    color: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g,
                                   Theme.textSecondary.b, 0.5)
                }
            }

            Label {
                Layout.fillWidth: true
                text: chapterRow.modelData.title
                color: Theme.textPrimary
                font.pixelSize: 13
                font.bold: chapterRow.current
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Label {
                Layout.leftMargin: 8
                Layout.alignment: Qt.AlignVCenter
                text: root.formatChapterTime(chapterRow.modelData.startTime)
                color: Theme.textSecondary
                font.pixelSize: 11
                font.family: "monospace"
            }
        }
    }

    // ContentUnavailableView's list.bullet glyph over its title, centred.
    ColumnLayout {
        anchors.centerIn: parent
        visible: !root.player.isLoadingAudiobookChapters && root.player.chapters.length === 0
        spacing: 10

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "\u2630"
            color: Theme.textSecondary
            font.pixelSize: 28
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("No Chapters")
            color: Theme.textPrimary
            font.pixelSize: 15
            font.bold: true
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        visible: root.player.isLoadingAudiobookChapters
        spacing: 10

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: root.player.isLoadingAudiobookChapters
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Loading Chapters...")
            color: Theme.textSecondary
            font.pixelSize: 13
        }
    }

    ListView {
        id: chapterList

        anchors.fill: parent
        visible: !root.player.isLoadingAudiobookChapters && root.player.chapters.length > 0
        model: root.player.chapters
        spacing: 2
        clip: true
        leftMargin: 10
        rightMargin: 10
        bottomMargin: 14
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        delegate: ChapterRow {}
    }
}
