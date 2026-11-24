// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Sasayaki inspector content, following Features/Sasayaki/SasayakiSheet.swift
// (body, selectedContent, selectedTabBinding, selectDefaultTabIfNeeded,
// currentChapterTitle) with the header and tabs of
// NativeMac/NativeReaderInspector.swift. The page's trailing side panel hosts
// it, so this file owns the inspector contents and reporting a close request.
//
// Without audio the inspector opens on Resources; with audio and audiobook
// chapters it opens on Chapters. Any user choice sticks, so later audio or
// chapter changes do not move the selection. The playback header is the
// extracted SasayakiPlaybackHeader and appears only while audio is loaded; it
// stays mounted across tab switches, which therefore never touch playback.
//
// The settings model is created here and reloaded when the panel opens. A
// colour pick rebuilds the reader document through ReaderViewModel so cue
// colours change without reopening the book.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: root

    objectName: "sasayakiInspector"

    required property SasayakiViewModel player
    property bool opened: false

    property int selectedTab: 0
    property bool userSelectedTab: false

    signal closeRequested()

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

    onOpenedChanged: {
        if (!opened)
            return;
        sasayakiSettings.reload();
        playback.scrubTime = -1;
        // Swift builds the sheet per presentation, so its search window starts
        // from the default; RESET restores the model's 200.
        root.player.matchSearchWindow = undefined;
        root.selectDefaultTabIfNeeded();
    }

    SasayakiSettingsViewModel {
        id: sasayakiSettings
    }

    Connections {
        target: root.player

        function onHasAudioChanged() {
            root.selectDefaultTabIfNeeded();
        }

        function onChaptersChanged() {
            root.selectDefaultTabIfNeeded();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PanelHeader {
            Layout.fillWidth: true
            title: qsTr("Sasayaki")
            subtitle: root.currentChapterTitle()
            onCloseRequested: root.closeRequested()
        }

        SasayakiPlaybackHeader {
            id: playback
            Layout.fillWidth: true
            Layout.leftMargin: 18
            Layout.rightMargin: 18
            Layout.bottomMargin: 14
            visible: root.player.hasAudio
            player: root.player
        }

        UnderlineTabBar {
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.inspectorTabBarPaddingBottom
            tabs: [
                { text: qsTr("Resources"), icon: "tray.and.arrow.down" },
                { text: qsTr("Chapters"), icon: "list.bullet" },
                { text: qsTr("Settings"), icon: "gearshape" }
            ]
            currentIndex: root.selectedTab
            onTabActivated: index => {
                root.userSelectedTab = true;
                root.selectedTab = index;
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
                objectName: "sasayakiSettingsContent"
                player: root.player
                settings: sasayakiSettings
            }
        }
    }
}
