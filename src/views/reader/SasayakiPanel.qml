// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Sasayaki inspector for the reader page, following
// Features/Sasayaki/SasayakiSheet.swift (body, playbackHeader, audiobookCover,
// currentChapterTitle, the tab selection rule and the Resources, Chapters and
// Settings tabs), Features/Sasayaki/SasayakiMatchView.swift,
// NativeMac/NativeReaderInspector.swift (NativeReaderInspectorHeader and
// NativeReaderInspectorTabBar) and NativeMac/NativeReaderView.swift
// (NativeReaderSidePanel width 440).
//
// D4 removes the cue list and the current-cue box. The drawer leaves the page
// visible behind it and closes with its close control or Escape. The playback
// header and the transport stay; the tabs own their content in
// SasayakiResourcesTab, SasayakiChaptersTab and SasayakiSettingsTab.
//
// The settings model is created here and reloaded when the drawer opens. A
// colour pick rebuilds the reader document through ReaderViewModel so cue
// colours change without reopening the book.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: root

    // The page's model; the panel never creates the player.
    required property SasayakiViewModel player
    readonly property bool opened: panel.visible
    signal closed

    property int selectedTab: 0
    property bool userSelectedTab: false
    property real scrubTime: -1

    function open() {
        panel.open();
    }

    function close() {
        panel.close();
    }

    function selectDefaultTabIfNeeded() {
        if (userSelectedTab)
            return;
        selectedTab = (player.hasAudio && player.chapters.length > 0) ? 1 : 0;
    }

    function currentChapterTitle() {
        const id = player.currentAudiobookChapterId;
        if (id < 0)
            return "";
        const chapters = player.chapters;
        for (var index = 0; index < chapters.length; ++index) {
            if (chapters[index].id === id)
                return chapters[index].title;
        }
        return "";
    }

    function formatTime(seconds) {
        var total = Math.max(0, Math.floor(seconds));
        var hours = Math.floor(total / 3600);
        var minutes = Math.floor((total % 3600) / 60);
        var rest = total % 60;
        return (hours < 10 ? "0" : "") + hours + ":" + (minutes < 10 ? "0" : "") + minutes + ":" + (rest < 10 ? "0" : "") + rest;
    }

    SasayakiSettingsViewModel {
        id: sasayakiSettings
    }

    Connections {
        target: root.player

        function onMatchApplied() {
            // ReaderPage owns this panel. Its apply path also highlights the
            // current cue after installing the ranges in the web view.
            const readerPage = root.parent;
            if (readerPage && typeof readerPage.applySasayakiCues === "function")
                readerPage.applySasayakiCues();
        }

        function onHasAudioChanged() {
            root.selectDefaultTabIfNeeded();
        }

        function onChaptersChanged() {
            root.selectDefaultTabIfNeeded();
        }
    }

    Drawer {
        id: panel
        edge: Qt.RightEdge
        width: 440
        height: Overlay.overlay ? Overlay.overlay.height : 640
        modal: false
        padding: 0
        closePolicy: Popup.CloseOnEscape
        onAboutToShow: {
            sasayakiSettings.reload();
            root.scrubTime = -1;
            // Swift builds the sheet per presentation, so its search window
            // starts from the default; RESET restores the model's 200.
            root.player.matchSearchWindow = undefined;
        }
        onAboutToHide: root.player.cancelSubtitleMatch()
        onOpened: root.selectDefaultTabIfNeeded()
        onClosed: root.closed()

        background: Rectangle {
            color: Theme.cardBackground
            border.width: Theme.cardBorderWidth
            border.color: Theme.cardStroke
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // NativeReaderInspectorHeader: 28 px round close control, 18/16/10
            // padding.
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.topMargin: 16
                Layout.bottomMargin: 10
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Sasayaki")
                        color: Theme.textPrimary
                        font.pixelSize: 20
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.currentChapterTitle().length > 0
                        text: root.currentChapterTitle()
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }

                RoundButton {
                    implicitWidth: 28
                    implicitHeight: 28
                    text: "\u2715"
                    font.pixelSize: 11
                    onClicked: panel.close()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Close")
                    Accessible.name: qsTr("Close")
                }
            }

            // SasayakiSheet.playbackHeader: 12 spacing, 18 horizontal, 14
            // bottom. The shadow is omitted with the other glass surfaces.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.bottomMargin: 14
                spacing: 12
                visible: root.player.hasAudio

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        implicitWidth: 64
                        implicitHeight: 64
                        radius: 10
                        color: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g,
                                       Theme.textSecondary.b, 0.10)
                        clip: true

                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            source: root.player.audiobookArtworkSource.length > 0
                                    ? root.player.audiobookArtworkSource : ReaderViewModel.coverUrl
                            visible: source.toString().length > 0
                        }

                        Rectangle {
                            anchors.fill: parent
                            visible: root.player.audiobookArtworkSource.length === 0
                                     && ReaderViewModel.coverUrl.length === 0
                            color: Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g,
                                           Theme.textSecondary.b, 0.14)
                            radius: 10

                            Label {
                                anchors.centerIn: parent
                                text: "\u224b"
                                color: Theme.textSecondary
                                font.pixelSize: 22
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: root.player.audiobookTitle.length > 0
                                  ? root.player.audiobookTitle : ReaderViewModel.title
                            color: Theme.textPrimary
                            font.pixelSize: 14
                            font.bold: true
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: root.player.audiobookArtist.length > 0
                            text: root.player.audiobookArtist
                            color: Theme.textSecondary
                            font.pixelSize: 13
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Slider {
                        id: scrubber
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(root.player.duration, 1)
                        enabled: root.player.duration > 0
                        value: root.scrubTime >= 0 ? root.scrubTime : root.player.position
                        onMoved: root.scrubTime = value
                        onPressedChanged: {
                            if (!pressed && root.scrubTime >= 0) {
                                root.player.seek(root.scrubTime);
                                root.scrubTime = -1;
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: root.formatTime(root.scrubTime >= 0 ? root.scrubTime : root.player.position)
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            font.family: "monospace"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "-" + root.formatTime(Math.max(root.player.duration - (root.scrubTime >= 0 ? root.scrubTime : root.player.position), 0))
                            color: Theme.textSecondary
                            font.pixelSize: 11
                            font.family: "monospace"
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 10

                    RoundButton {
                        implicitWidth: 34
                        implicitHeight: 34
                        text: "15\u21ba"
                        font.pixelSize: 11
                        onClicked: root.player.skipForward(false)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Back 15 seconds")
                    }

                    RoundButton {
                        implicitWidth: 34
                        implicitHeight: 34
                        text: "\u00ab"
                        font.pixelSize: 13
                        onClicked: root.player.previousCue()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Previous cue")
                    }

                    RoundButton {
                        implicitWidth: 48
                        implicitHeight: 48
                        text: root.player.playing ? "\u275a\u275a" : "\u25b6"
                        font.pixelSize: 18
                        font.bold: true
                        onClicked: root.player.togglePlayback()
                        ToolTip.visible: hovered
                        ToolTip.text: root.player.playing ? qsTr("Pause") : qsTr("Play")
                    }

                    RoundButton {
                        implicitWidth: 34
                        implicitHeight: 34
                        text: "\u00bb"
                        font.pixelSize: 13
                        onClicked: root.player.nextCue()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Next cue")
                    }

                    RoundButton {
                        implicitWidth: 34
                        implicitHeight: 34
                        text: "15\u21bb"
                        font.pixelSize: 11
                        onClicked: root.player.skipForward(true)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Forward 15 seconds")
                    }
                }
            }

            // NativeReaderInspectorTabBar: equal widths, glyph over label,
            // 7 px spacing, callout text, 2 px accent underline, a divider
            // under the bar and 10 px below it.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 10
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 0

                    Repeater {
                        model: [
                            { title: qsTr("Resources"), glyph: "\u2913" },
                            { title: qsTr("Chapters"), glyph: "\u2630" },
                            { title: qsTr("Settings"), glyph: "\u2699" }
                        ]

                        delegate: Item {
                            id: tabItem
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            implicitHeight: 44

                            readonly property bool selected: root.selectedTab === tabItem.index

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 7

                                Label {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 6
                                    text: tabItem.modelData.glyph
                                    color: tabItem.selected ? Theme.textPrimary : Theme.textSecondary
                                    font.pixelSize: 15
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: tabItem.modelData.title
                                    color: tabItem.selected ? Theme.textPrimary : Theme.textSecondary
                                    font.pixelSize: 13
                                    font.bold: tabItem.selected
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 2
                                    radius: 1
                                    color: tabItem.selected ? Theme.accent : "transparent"
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.userSelectedTab = true;
                                    root.selectedTab = tabItem.index;
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.popupDivider
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.selectedTab

                SasayakiResourcesTab {
                    player: root.player
                }

                SasayakiChaptersTab {
                    player: root.player
                }

                SasayakiSettingsTab {
                    player: root.player
                    settings: sasayakiSettings
                }
            }
        }
    }
}
