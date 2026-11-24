// SPDX-License-Identifier: GPL-3.0-or-later
//
// Local verification sheet for the video playback chrome: the real
// src/views/player components with static demo values over colourful stand-in
// frames, because the offscreen renderer has no OpenGL for libmpv. Shown with
// NIRATAN_THEME_PREVIEW=player. Left, top to bottom: a wide stage with the
// OSD, the floating bar with its speed panel open and the inspector; a stage
// with the compact bottom bar; then a narrow floating bar, a condensed compact
// bar and the no-video empty state side by side. Right: the study sidebar.
// It is not part of any user flow and wires nothing to playback.
import QtQuick
import Niratan

Item {
    id: sheet

    // A stand-in video frame: a bright diagonal gradient with a pale band,
    // so the chrome is judged against busy content, not plain black.
    component Frame: Rectangle {
        clip: true
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#3A7BD5" }
            GradientStop { position: 0.5; color: "#E8C07A" }
            GradientStop { position: 1.0; color: "#2E8B57" }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            y: parent.height * 0.62
            height: parent.height * 0.22
            color: "#F4F1EA"
            opacity: 0.85
        }
    }

    // The demo playback snapshot shared by every bar.
    component DemoBar: VideoControlBar {
        videoFileSet: true
        chromeShown: true
        isPlaying: true
        isPaused: false
        volume: 70
        currentTime: 754
        duration: 1440
        speed: 1.5
        canMine: true
        chapters: [{ "start": 300 }, { "start": 900 }]
    }

    readonly property int sidebarWidth: 420
    readonly property int stageWidth: width - sidebarWidth - 3 * gap
    readonly property int gap: 12

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBackground
    }

    MpvItem {
        id: demoEngine

        visible: false
        width: 0
        height: 0
    }

    PlayerInspectorViewModel {
        id: inspectorModel
    }

    PlayerStudyViewModel {
        id: studyModel
    }

    Column {
        x: sheet.gap
        y: sheet.gap
        spacing: sheet.gap

        Frame {
            id: wideStage

            width: sheet.stageWidth
            height: 520

            PlayerOsd {
                anchors.top: parent.top
                anchors.left: parent.left
                title: qsTr("Volume")
                value: "70%"
                meter: 0.7
            }

            DemoBar {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height - height - Theme.inspectorFloatingBarInset
                availableWidth: parent.width
                layoutMode: "floating"
                speedPanelOpen: true
                inspectorOpen: true
            }

            VideoInspectorPanel {
                engine: demoEngine
                model: inspectorModel
                open: true
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: Theme.inspectorOverlayInset
                anchors.rightMargin: Theme.inspectorOverlayInset
                maximumHeight: 220
            }

            // The Subtitles tab scrolled mid-way, to check the clip at the
            // rounded edges and the scroll bar gutter.
            VideoInspectorPanel {
                id: scrolledInspector

                engine: demoEngine
                model: inspectorModel
                open: true
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: 110
                anchors.leftMargin: Theme.inspectorOverlayInset
                maximumHeight: 400
                Component.onCompleted: Qt.callLater(() => {
                    const flick = scrolledInspector.children.find(c => c.flickable)?.flickable
                    if (flick)
                        flick.contentY = (flick.contentHeight - flick.height) / 2
                })
            }
        }

        Frame {
            width: sheet.stageWidth
            height: 160

            DemoBar {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height - height
                availableWidth: parent.width
                layoutMode: "compactBottom"
                studyOpen: true
            }
        }

        Row {
            spacing: sheet.gap

            Frame {
                width: 420
                height: 200

                DemoBar {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - height - Theme.spacingLg
                    availableWidth: parent.width
                    layoutMode: "floating"
                }
            }

            Frame {
                width: 520
                height: 200

                DemoBar {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - height
                    availableWidth: parent.width
                    layoutMode: "compactBottom"
                }
            }

            Rectangle {
                width: sheet.stageWidth - 940 - 2 * sheet.gap
                height: 200
                color: Theme.videoBackground

                PlayerEmptyState {
                    anchors.fill: parent
                }
            }
        }
    }

    // Demo study content: a short subtitle document, playback inside its
    // second cue, two chapters and an A-B loop starting at that cue.
    readonly property string demoSubtitles: "1\n00:00:01,000 --> 00:00:04,000\n今日はいい天気ですね。\n\n"
        + "2\n00:00:05,000 --> 00:00:09,000\nそうですね、散歩に行きましょう。\n\n"
        + "3\n00:00:10,000 --> 00:00:14,000\n駅前の公園まで歩きませんか。\n\n"
        + "4\n00:00:15,000 --> 00:00:19,000\nいいですね。お弁当も持っていきます。\n"

    Component.onCompleted: {
        studyModel.chapters = [{ "title": qsTr("Opening"), "start": 0 },
                               { "title": qsTr("Part A"), "start": 6 },
                               { "title": qsTr("Part B"), "start": 14 }]
        studyModel.setPlaybackReady(true)
        studyModel.setSubtitleDocument("file:///demo.srt", sheet.demoSubtitles, true)
        studyModel.updatePlayback(6, 0)
    }

    Column {
        x: sheet.width - sheet.sidebarWidth - sheet.gap
        y: sheet.gap
        spacing: sheet.gap

        StudySidebar {
            width: sheet.sidebarWidth
            height: (sheet.height - 3 * sheet.gap) / 2
            preferredWidth: sheet.sidebarWidth
            study: studyModel
            selectedTab: 1
            playbackReady: true
            loopStart: 5
        }

        StudySidebar {
            width: sheet.sidebarWidth
            height: (sheet.height - 3 * sheet.gap) / 2
            preferredWidth: sheet.sidebarWidth
            study: studyModel
            selectedTab: 2
            playbackReady: true
        }
    }
}
