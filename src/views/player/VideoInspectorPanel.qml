// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video Inspector panel, following Features/Video/VideoInspectorView.swift
// (`body`, `header`, `VideoInspectorTabBar`, `inspectorSection`,
// `VideoInspectorGlassSurface`) and Features/Video/VideoPlayerScreen.swift
// (`inspectorOverlay`, `inspectorOverlayTrailingInset`,
// `inspectorOverlayVerticalInset`, `selectedInspectorTab`, `toggleInspector`).
//
// The panel floats at the trailing edge of the video stage; the page owns
// `inspectorOpen` and supplies the mpv engine and the Inspector model. One
// 26 px rounded surface holds a now-playing header, the icon tab strip and the
// scrolling tab content. Tab content lives in VideoInspectorEpisodesTab,
// VideoInspectorVideoTab, VideoInspectorAudioTab and
// VideoInspectorSubtitlesTab; the panel keeps the selected tab (default
// Subtitles) and forwards their intents as its own signals.
//
// Signals: closeRequested hides the panel and returns focus to the player;
// the rest mirror the model's requests, except openSubtitleFileRequested,
// which asks the page to show the subtitle file dialog and load the file.
//
// Object names: videoInspectorPanel, inspectorHeaderIcon, inspectorTitle,
// inspectorCaption, inspectorCloseButton, inspectorTab (in the tab strip).

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Rectangle {
    id: panel

    objectName: "videoInspectorPanel"

    required property MpvItem engine
    required property PlayerInspectorViewModel model
    property bool open: false
    // 0 episodes, 1 video, 2 audio, 3 subtitles.
    property int selectedTab: 3
    // The page sets the room between the top inset and the playback bar;
    // the panel is as tall as its content up to that height and scrolls the
    // tab content beyond it.
    property real maximumHeight: implicitHeight

    signal closeRequested()
    signal selectEpisodeRequested(var url)
    signal selectTrackRequested(string type, int id)
    signal speedRequested(real value)
    signal subtitleDelayRequested(real seconds)
    signal audioDelayRequested(real seconds)
    signal openSubtitleFileRequested(var file)
    signal openTranscriptRequested()

    visible: open
    // Never wider than the stage the page gives it.
    property real maximumWidth: Theme.inspectorWidth

    width: Math.min(Theme.inspectorWidth, maximumWidth)
    implicitHeight: scaffold.y + scaffold.flickable.contentHeight + Theme.inspectorScrollInset
                    + 2 * border.width
    height: Math.min(implicitHeight, maximumHeight)
    radius: Theme.inspectorRadius
    color: Theme.inspectorBackground
    border.width: Theme.cardBorderWidth
    border.color: Theme.floatingStroke

    Column {
        id: chrome

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: panel.border.width
        anchors.rightMargin: panel.border.width

        Item {
            width: parent.width
            height: header.implicitHeight + Theme.inspectorHeaderPadding
                    + Theme.inspectorHeaderPaddingBottom

            Row {
                id: header

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.inspectorHeaderPadding
                anchors.rightMargin: Theme.inspectorHeaderPadding
                y: Theme.inspectorHeaderPadding
                spacing: Theme.inspectorHeaderSpacing

                Rectangle {
                    objectName: "inspectorHeaderIcon"
                    width: Theme.inspectorHeaderIconSize
                    height: Theme.inspectorHeaderIconSize
                    radius: Theme.inspectorHeaderIconRadius
                    color: Theme.accent

                    Icon {
                        anchors.centerIn: parent
                        name: "play.rectangle.fill"
                        size: Theme.inspectorHeaderGlyphSize
                        color: Theme.overlayForeground
                    }
                }

                Column {
                    width: header.width - Theme.inspectorHeaderIconSize - Theme.inspectorCloseDiameter
                           - 2 * header.spacing
                    spacing: Theme.panelHeaderTitleSpacing

                    StyledLabel {
                        objectName: "inspectorCaption"
                        width: parent.width
                        text: panel.model.headerCaption
                        textStyle: Theme.captionFont
                        weight: Theme.weightMedium
                        secondary: true
                        elide: Text.ElideRight
                    }

                    StyledLabel {
                        objectName: "inspectorTitle"
                        width: parent.width
                        text: panel.model.displayTitle
                        textStyle: Theme.headlineFont
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideMiddle
                    }
                }

                IconButton {
                    objectName: "inspectorCloseButton"
                    icon: "xmark"
                    diameter: Theme.inspectorCloseDiameter
                    iconSize: Theme.inspectorCloseGlyphSize
                    accessibleName: qsTr("Close")
                    onClicked: panel.closeRequested()
                }
            }
        }

        Item {
            width: parent.width
            height: tabBar.implicitHeight + Theme.inspectorTabBarPaddingBottom

            IconTabBar {
                id: tabBar

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.inspectorTabBarInset
                anchors.rightMargin: Theme.inspectorTabBarInset
                height: implicitHeight
                itemName: "inspectorTab"
                tabs: [
                    { key: "episodes", title: qsTr("Episodes"), icon: "list.number" },
                    { key: "video", title: qsTr("Video"), icon: "film" },
                    { key: "audio", title: qsTr("Audio"), icon: "waveform" },
                    { key: "subtitles", title: qsTr("Subtitles"), icon: "captions.bubble" }
                ]
                selectedTab: panel.selectedTab
                onTabSelected: tab => panel.selectedTab = tab
            }
        }
    }

    PanelScaffold {
        id: scaffold

        // The scroll viewport keeps the same inset under the tab bar, above the
        // rounded bottom edge and at both sides, so its rectangular clip and
        // its corners stay inside the panel's rounded shape. The side inset is
        // taken out of the content padding, so rows keep their position.
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.inspectorScrollInset
        anchors.rightMargin: Theme.inspectorScrollInset
        y: chrome.height + Theme.inspectorScrollInset
        height: panel.height - y - Theme.inspectorScrollInset
        horizontalPadding: Theme.panelPaddingHorizontal - Theme.inspectorScrollInset
        compact: true
        persistentScrollBar: true
        header: Item {}

        VideoInspectorEpisodesTab {
            visible: panel.selectedTab === 0
            model: panel.model
            onSelectEpisodeRequested: url => panel.selectEpisodeRequested(url)
        }

        VideoInspectorVideoTab {
            visible: panel.selectedTab === 1
            model: panel.model
            engine: panel.engine
            onSelectTrackRequested: (type, id) => panel.selectTrackRequested(type, id)
            onSpeedRequested: value => panel.speedRequested(value)
        }

        VideoInspectorAudioTab {
            visible: panel.selectedTab === 2
            model: panel.model
            onSelectTrackRequested: (type, id) => panel.selectTrackRequested(type, id)
            onAudioDelayRequested: seconds => panel.audioDelayRequested(seconds)
        }

        VideoInspectorSubtitlesTab {
            visible: panel.selectedTab === 3
            model: panel.model
            engine: panel.engine
            onSelectTrackRequested: (type, id) => panel.selectTrackRequested(type, id)
            onSubtitleDelayRequested: seconds => panel.subtitleDelayRequested(seconds)
            onOpenSubtitleFileRequested: file => panel.openSubtitleFileRequested(file)
            onOpenTranscriptRequested: panel.openTranscriptRequested()
        }
    }
}
