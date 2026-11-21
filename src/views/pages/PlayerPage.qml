// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video player page: the mpv surface with Niratan's floating playback
// chrome. Metrics and behavior follow Features/Video/VideoControlsView.swift
// (floating controls 760 x 90, 24 corner radius, play button 42, resting
// track 4, knob 13, speed presets 0.25...5 in 4 x 2 rows, slider step 0.1),
// VideoPlayerScreen+OSD.swift (1.35 s on-screen display and the speed, delay
// and volume value text), VideoPlayerScreen+Shortcuts.swift with
// VideoShortcutActions.swift (the default key bindings), and
// VideoPlayerScreen+Chrome.swift (one second auto-hide, chrome revealed by
// pointer movement, visible while paused). Playback state comes only from
// MpvItem; the page never reaches libmpv.
//
// The stage is black in both themes, as in VideoPlayerScreen.swift, so the
// letterbox does not follow the window background. Every other surface,
// border and text colour comes from Theme; the empty-state card, the OSD panel
// and the chrome use floating tokens because their text sits over that black
// stage and must stay legible in the light theme too.
//
// The chrome hides one second after the last pointer movement over the page,
// but stays visible while paused, while the pointer is over the bar or the
// speed panel, and while the timeline is being scrubbed.
//
// Chapters, subtitle transcripts and mining history live in the docked study
// sidebar. The timeline also shows chapter markers. An inspector, an ambient backdrop,
// remote and media-server sources, playlist navigation, screenshot UI,
// drag-and-drop and interactive subtitle rendering are not included.
// Both the playback bar and the study dock project the same A-B state.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Niratan

Item {
    id: page

    signal backRequested()

    // VideoControlsView.swift: chromeSize and metrics(for: .floating).
    readonly property int chromeMaxWidth: 760
    readonly property bool compactChrome: mpv.videoSettings.videoControlBarLayout === "compactBottom"
    readonly property int chromeHeight: page.compactChrome ? 84 : 90
    readonly property int seekInterval: Math.floor(mpv.videoSettings.videoSeekInterval)
    readonly property int chromeSideInset: 16
    // playbackChromeEdgeInset (16) plus metrics.bottomInset (24).
    readonly property int chromeBottomInset: page.compactChrome ? 0 : 40

    // VideoMediaTypes.supportedExtensions, in the same order.
    readonly property var supportedExtensions: [
        "mkv", "webm", "avi", "m4v", "mp4", "mov", "qt",
        "mpg", "mpeg", "ts", "m2ts", "mts", "3gp", "ogv",
        "wmv", "asf", "flv",
        "m4b", "m4a", "mp3", "flac", "opus", "ogg", "oga",
        "weba", "wav", "aac", "aiff", "aif", "ape", "wv"
    ]
    readonly property string extensionList: page.supportedExtensions.join(", ")
    readonly property string extensionPattern: page.supportedExtensions.map(
        function (extension) {
            return "*." + extension
        }).join(" ")

    readonly property real displayTime: page.scrubbing ? page.scrubTime : mpv.position
    readonly property real displayRatio: mpv.duration > 0
        ? Math.max(0, Math.min(1, page.displayTime / mpv.duration))
        : 0

    // The bar is shown while the video is loaded and any of these hold. The
    // paused case follows the page requirement; the others keep the bar from
    // disappearing under the pointer or during a scrub.
    readonly property bool chromeShown: mpv.loaded
        && (page.chromeVisible || mpv.paused || page.barHovered
            || page.scrubbing || page.speedPanelOpen)
    readonly property bool fullScreen: page.Window.window
        ? page.Window.window.visibility === Window.FullScreen
        : false

    property bool chromeVisible: true
    property bool barHovered: false
    property bool scrubbing: false
    property real scrubTime: 0
    property bool speedPanelOpen: false
    property bool speedPanelHovered: false
    property bool studyOpen: false
    property string osdTitle: ""
    property string osdValue: ""
    property real osdMeter: -1

    focus: true
    Keys.priority: Keys.BeforeItem
    Keys.onPressed: function (event) {
        page.handleKey(event)
    }

    // VideoTimeFormatter.string(from:): floor to seconds, h:mm:ss only when
    // hours are present, otherwise m:ss.
    function formatTime(seconds) {
        if (!isFinite(seconds) || seconds <= 0)
            return "0:00"
        var total = Math.floor(seconds)
        var hours = Math.floor(total / 3600)
        var minutes = Math.floor((total % 3600) / 60)
        var remainder = total % 60
        var secondsText = remainder < 10 ? "0" + remainder : String(remainder)
        if (hours > 0) {
            var minutesText = minutes < 10 ? "0" + minutes : String(minutes)
            return hours + ":" + minutesText + ":" + secondsText
        }
        return minutes + ":" + secondsText
    }

    // VideoPlaybackSpeed.normalized: 0.25...5 in 0.1 steps, with 0.25 kept
    // as the floor for speeds at or below it.
    function formatSpeed(value) {
        return mpv.speedLabel(value) + "x"
    }

    function speedIs(value) {
        return mpv.speedLabel(mpv.speed) === mpv.speedLabel(value)
    }

    // VideoPlayerScreen.delayOSDValue: zero within 5 ms, else %+.2fs.
    function formatDelay(value) {
        if (Math.abs(value) < 0.005)
            return "0.00s"
        return (value > 0 ? "+" : "-") + Math.abs(value).toFixed(2) + "s"
    }

    function showOSD(title, value, meter) {
        page.osdTitle = title
        page.osdValue = value
        page.osdMeter = meter === undefined ? -1 : meter
        osdTimer.restart()
    }

    // VideoPlayerScreen+Chrome.swift: reveal on pointer movement and schedule
    // the hide one second later.
    function notePointerMovement() {
        if (!mpv.loaded)
            return
        page.chromeVisible = true
        hideTimer.restart()
    }

    function scheduleHide() {
        if (!mpv.loaded)
            return
        hideTimer.restart()
    }

    function handleHideTimeout() {
        if (page.barHovered || page.speedPanelHovered || page.scrubbing || mpv.paused
                || (page.speedPanelOpen && speedInput.activeFocus))
            return
        if (page.speedPanelOpen)
            page.speedPanelOpen = false
        page.chromeVisible = false
    }

    function timeAt(x) {
        if (mpv.duration <= 0 || timeline.width <= 0)
            return 0
        return Math.max(0, Math.min(mpv.duration, mpv.duration * x / timeline.width))
    }

    function beginScrub(x) {
        if (!mpv.loaded || mpv.duration <= 0)
            return
        hideTimer.stop()
        page.scrubbing = true
        page.scrubTime = page.timeAt(x)
        page.chromeVisible = true
    }

    function updateScrub(x) {
        if (page.scrubbing)
            page.scrubTime = page.timeAt(x)
    }

    function endScrub() {
        if (!page.scrubbing)
            return
        page.scrubbing = false
        mpv.seek(page.scrubTime)
        page.notePointerMovement()
    }

    function togglePause() {
        if (!mpv.loaded)
            return
        mpv.togglePause()
        page.notePointerMovement()
    }

    function skip(delta) {
        if (!mpv.loaded || mpv.duration <= 0)
            return
        mpv.skip(delta)
    }

    function setVolume(value) {
        if (!mpv.loaded)
            return
        var volume = Math.max(0, Math.min(100, value))
        mpv.volume = volume
        page.showOSD(qsTr("Volume"), String(Math.round(volume)), volume / 100)
    }

    function adjustVolume(delta) {
        page.setVolume(mpv.volume + delta)
    }

    function toggleMute() {
        if (!mpv.loaded)
            return
        mpv.muted = !mpv.muted
        page.showOSD(qsTr("Volume"), mpv.muted ? qsTr("Muted") : qsTr("Unmuted"),
                     mpv.muted ? 0 : mpv.volume / 100)
    }

    function setSpeed(value) {
        if (!mpv.loaded)
            return
        mpv.speed = value
        page.showOSD(qsTr("Speed"), page.formatSpeed(mpv.speed))
    }

    function adjustSpeed(delta) {
        page.setSpeed(mpv.speed + delta)
    }

    function resetSpeed() {
        if (!mpv.loaded)
            return
        mpv.resetSpeed()
        page.showOSD(qsTr("Speed"), page.formatSpeed(1.0))
    }

    function setSubtitleDelay(value) {
        if (!mpv.loaded)
            return
        var delay = Math.max(-60, Math.min(60, value))
        mpv.subtitleDelay = delay
        page.showOSD(qsTr("Subtitle Delay"), page.formatDelay(delay))
    }

    function adjustSubtitleDelay(delta) {
        page.setSubtitleDelay(mpv.subtitleDelay + delta)
    }

    function setAudioDelay(value) {
        if (!mpv.loaded)
            return
        var delay = Math.max(-30, Math.min(30, value))
        mpv.audioDelay = delay
        page.showOSD(qsTr("Audio Delay"), page.formatDelay(delay))
    }

    function adjustAudioDelay(delta) {
        page.setAudioDelay(mpv.audioDelay + delta)
    }

    function toggleSubtitles() {
        if (!mpv.loaded)
            return
        mpv.toggleSubtitles()
        page.showOSD(qsTr("Subtitles"),
                     mpv.subtitlesVisible ? qsTr("On") : qsTr("Off"))
    }

    function cycleSubtitles() {
        if (!mpv.loaded)
            return
        mpv.cycleSubtitles()
    }

    function setLoopA(time) {
        if (!mpv.loaded)
            return
        if (!mpv.setABLoopStartAt(time)) {
            page.showOSD(qsTr("A-B Loop"), qsTr("Cannot set A here"))
            return
        }
        page.showOSD(qsTr("A-B Loop"), qsTr("A set"))
    }

    function setLoopB(time) {
        if (!mpv.loaded)
            return
        if (mpv.loopStart < 0) {
            page.showOSD(qsTr("A-B Loop"), qsTr("Set A first"))
            return
        }
        if (!mpv.setABLoopEndAt(time)) {
            page.showOSD(qsTr("A-B Loop"), qsTr("B must come after A"))
            return
        }
        page.showOSD(qsTr("A-B Loop"), qsTr("B set"))
    }

    function clearABLoop() {
        if (!mpv.loaded)
            return
        mpv.clearABLoop()
        page.showOSD(qsTr("A-B Loop"), qsTr("Cleared"))
    }

    function toggleLooping() {
        if (!mpv.loaded)
            return
        mpv.looping = !mpv.looping
        page.showOSD(qsTr("Loop"), mpv.looping ? qsTr("On") : qsTr("Off"))
    }

    function rotateClockwise() {
        if (mpv.loaded)
            mpv.rotateClockwise()
    }

    function toggleFullScreen() {
        var window = page.Window.window
        if (!window)
            return
        if (window.visibility === Window.FullScreen)
            window.visibility = Window.Windowed
        else
            window.visibility = Window.FullScreen
        page.notePointerMovement()
    }

    function exitFullScreen() {
        var window = page.Window.window
        if (window && window.visibility === Window.FullScreen)
            window.visibility = Window.Windowed
    }

    function closePlayer() {
        // The player is a stack page here, unlike Niratan's own window, so a
        // full screen left behind would also make the bookshelf full screen.
        page.exitFullScreen()
        mpv.unload()
        page.backRequested()
    }

    function openVideo(url, fromBeginning) {
        if (!url || url.toString().length === 0)
            return
        page.speedPanelOpen = false
        page.speedPanelHovered = false
        page.chromeVisible = true
        page.osdTitle = ""
        page.osdValue = ""
        osdTimer.stop()
        studyModel.setVideo(url)
        mpv.open(url, fromBeginning === true)
        page.forceActiveFocus()
        page.notePointerMovement()
    }

    function toggleTranscript() {
        if (page.studyOpen && studySidebar.selectedTab === 1) {
            page.studyOpen = false
            page.forceActiveFocus()
        } else {
            studySidebar.selectedTab = 1
            page.studyOpen = true
        }
    }

    function loadSubtitle(file: url): bool {
        if (mpv.loadExternalSubtitle(file))
            return true
        studyModel.reportSubtitleLoadFailure()
        return false
    }

    function textFieldFocused() {
        var item = page.Window.activeFocusItem
        while (item) {
            if (item instanceof TextInput || item instanceof TextEdit
                    || item instanceof TextField || item instanceof TextArea)
                return true
            item = item.parent
        }
        return false
    }

    function handleKey(event) {
        if (event.key === Qt.Key_Escape && page.fullScreen) {
            event.accepted = true
            if (event.isAutoRepeat)
                return
            page.exitFullScreen()
            return
        }
        if (!mpv.loaded || page.textFieldFocused())
            return

        var alt = (event.modifiers & Qt.AltModifier) !== 0
        var shift = (event.modifiers & Qt.ShiftModifier) !== 0
        var control = (event.modifiers & Qt.ControlModifier) !== 0
        var meta = (event.modifiers & Qt.MetaModifier) !== 0
        if (event.key === Qt.Key_Z && control && shift && !alt && !meta) {
            event.accepted = true
            if (!event.isAutoRepeat) {
                studyModel.mineCurrentSubtitle()
                page.showOSD(qsTr("Mining History"), studyModel.lastMiningNotice)
            }
            return
        }
        if (control || meta)
            return

        var handled = true
        var plain = !alt && !shift
        if (event.key === Qt.Key_Space && plain) {
            if (!event.isAutoRepeat)
                page.togglePause()
        } else if (event.key === Qt.Key_Left && plain) {
            page.skip(-page.seekInterval)
        } else if (event.key === Qt.Key_Right && plain) {
            page.skip(page.seekInterval)
        } else if (event.key === Qt.Key_Up && plain) {
            page.adjustVolume(5)
        } else if (event.key === Qt.Key_Down && plain) {
            page.adjustVolume(-5)
        } else if (event.key === Qt.Key_BracketLeft && plain) {
            page.adjustSpeed(-0.1)
        } else if (event.key === Qt.Key_BracketRight && plain) {
            page.adjustSpeed(0.1)
        } else if (event.key === Qt.Key_Backslash && plain) {
            page.resetSpeed()
        } else if (event.key === Qt.Key_M && plain) {
            if (!event.isAutoRepeat)
                page.toggleMute()
        } else if (event.key === Qt.Key_V && plain) {
            if (!event.isAutoRepeat)
                page.toggleSubtitles()
        } else if (event.key === Qt.Key_S && plain) {
            if (!event.isAutoRepeat)
                page.cycleSubtitles()
        } else if (event.key === Qt.Key_T && plain) {
            if (!event.isAutoRepeat)
                page.toggleTranscript()
        } else if (event.key === Qt.Key_Comma && alt) {
            if (shift)
                page.adjustAudioDelay(-0.5)
            else
                page.adjustSubtitleDelay(-0.05)
        } else if (event.key === Qt.Key_Period && alt) {
            if (shift)
                page.adjustAudioDelay(0.5)
            else
                page.adjustSubtitleDelay(0.05)
        } else if (event.key === Qt.Key_Slash && alt && !shift) {
            page.setSubtitleDelay(0)
        } else if (event.key === Qt.Key_F && plain) {
            if (!event.isAutoRepeat)
                page.toggleFullScreen()
        } else if (event.key === Qt.Key_L && plain) {
            if (!event.isAutoRepeat)
                page.toggleLooping()
        } else if (event.key === Qt.Key_A && alt && !shift) {
            if (!event.isAutoRepeat)
                page.setLoopA(mpv.position)
        } else if (event.key === Qt.Key_B && alt && !shift) {
            if (!event.isAutoRepeat)
                page.setLoopB(mpv.position)
        } else if (event.key === Qt.Key_R && plain) {
            if (!event.isAutoRepeat)
                page.rotateClockwise()
        } else {
            handled = false
        }
        if (handled)
            event.accepted = true
    }

    PlayerStudyViewModel {
        id: studyModel
        chapters: mpv.chapters
        onSeekRequested: function(seconds) { mpv.seek(seconds) }
        onChapterRequested: function(id) { mpv.seekToChapter(id) }
        onVideoRequested: function(file) { page.openVideo(file) }
        onHistorySubtitleRequested: function(file, trackId, seconds) {
            if (file.toString().length > 0) {
                page.loadSubtitle(file)
            } else if (trackId > 0) {
                mpv.selectSubtitleTrack(trackId)
            }
            mpv.subtitlesVisible = true
            mpv.seek(seconds)
        }
    }

    StudySidebar {
        id: studySidebar
        visible: page.studyOpen
        anchors.top: page.top
        anchors.right: page.right
        anchors.bottom: page.bottom
        width: Math.min(studySidebar.preferredWidth,
                        Math.max(studySidebar.minimumWidth, page.width - 340))
        study: studyModel
        playbackReady: mpv.loaded
        subtitleTracks: mpv.tracks.filter(track => track.type === "sub")
        extractingTranscript: mpv.extractingTranscript
        loopStart: mpv.loopStart
        loopEnd: mpv.loopEnd
        onLoopStartRequested: function(time) { page.setLoopA(time) }
        onLoopEndRequested: function(time) { page.setLoopB(time) }
        onSubtitleFileRequested: function(file) { page.loadSubtitle(file) }
        onSubtitleTrackRequested: function(trackId, extract) {
            if (trackId > 0) {
                mpv.selectSubtitleTrack(trackId)
                if (extract)
                    mpv.extractTranscript()
            } else {
                mpv.disableSubtitles()
            }
        }
        onTranscriptRequested: mpv.extractTranscript()
        onCloseRequested: {
            page.studyOpen = false
            page.forceActiveFocus()
        }
    }

    Component.onCompleted: page.forceActiveFocus()

    Timer {
        id: hideTimer
        interval: 1000
        onTriggered: page.handleHideTimeout()
    }

    Timer {
        id: osdTimer
        interval: 1350
        onTriggered: {
            page.osdTitle = ""
            page.osdValue = ""
        }
    }

    // The player stage is black in both themes: Niratan draws the video
    // surface on black (VideoPlayerScreen.swift), and letterboxing must not
    // follow the window background.
    Rectangle {
        id: stage
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: page.studyOpen ? studySidebar.left : parent.right
        color: Theme.videoBackground

        MpvItem {
            id: mpv
            anchors.fill: parent

            onLoadedChanged: {
                studyModel.setPlaybackReady(mpv.loaded)
                if (!mpv.loaded)
                    return
                page.speedPanelOpen = false
                page.speedPanelHovered = false
                page.chromeVisible = true
                page.forceActiveFocus()
                page.notePointerMovement()
            }
            onPositionChanged: studyModel.updatePlayback(mpv.position, mpv.subtitleDelay)
            onSubtitleDelayChanged: studyModel.updatePlayback(mpv.position, mpv.subtitleDelay)
            onTracksChanged: studyModel.setTracks(mpv.tracks)
            onVideoSettingsChanged: studyModel.refreshHistory()
            onSubtitlesVisibleChanged: studyModel.setSubtitlesVisible(mpv.subtitlesVisible)
            onSubtitleCueChanged: studyModel.updateNativeCue(mpv.subtitleText, mpv.subtitleStart,
                                                             mpv.subtitleEnd)
            onSubtitleDocumentChanged: function(file, bytes, readable) {
                studyModel.setSubtitleDocument(file, bytes, readable)
            }
            onEmbeddedTranscriptChanged: function(bytes, error) {
                studyModel.setEmbeddedTranscript(bytes, error)
            }
            onResumeChanged: {
                if (mpv.resumeAvailable)
                    page.showOSD(qsTr("Resumed at %1").arg(page.formatTime(mpv.resumePosition)), "")
            }
            onPausedChanged: {
                if (!mpv.paused)
                    page.scheduleHide()
            }
            onEnded: {
                page.chromeVisible = true
                page.forceActiveFocus()
            }
        }
    }

    MouseArea {
        anchors.fill: stage
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        onPositionChanged: page.notePointerMovement()
        onExited: page.scheduleHide()
    }

    TapHandler {
        parent: stage
        enabled: mpv.loaded
        exclusiveSignals: TapHandler.SingleTap | TapHandler.DoubleTap
        onSingleTapped: page.togglePause()
        onDoubleTapped: page.toggleFullScreen()
    }

    Rectangle {
        id: emptyState
        anchors.centerIn: stage
        width: Math.min(440, Math.max(0, stage.width - 64))
        height: emptyColumn.implicitHeight + 40
        visible: !mpv.loaded
        radius: Theme.cardRadius
        color: Theme.floatingBackground
        border.width: Theme.cardBorderWidth
        border.color: Theme.floatingStroke

        Column {
            id: emptyColumn
            anchors.centerIn: parent
            width: parent.width - 32
            spacing: 10

            Label {
                width: parent.width
                text: qsTr("No Video Open")
                color: Theme.textPrimary
                font.pixelSize: 18
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }

            Button {
                width: parent.width
                text: qsTr("Open a video")
                onClicked: openMenu.popup()
            }

            Button {
                width: parent.width
                text: qsTr("Video Settings")
                onClicked: videoSettings.open()
            }

            Label {
                width: parent.width
                text: qsTr("Supported files: %1").arg(page.extensionList)
                color: Theme.textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                width: parent.width
                visible: mpv.error.length > 0
                text: mpv.error
                color: Theme.popupDanger
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // VideoPlayerScreen+OSD.swift places the OSD 30 from the top and 28 from
    // the leading edge; the panel is added so the text stays readable on both
    // themes over the black stage.
    Rectangle {
        id: osd
        anchors.top: stage.top
        anchors.left: stage.left
        anchors.topMargin: 30
        anchors.leftMargin: 28
        width: Math.max(osdRow.implicitWidth, meter.visible ? meter.width : 0) + 24
        height: meter.visible ? 46 : 34
        radius: 10
        color: Theme.floatingBackground
        border.width: Theme.cardBorderWidth
        border.color: Theme.floatingStroke
        opacity: page.osdTitle.length > 0 ? 1 : 0
        visible: page.osdTitle.length > 0 || opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: 120
            }
        }

        Row {
            id: osdRow
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 8
            anchors.leftMargin: 12
            spacing: 8

            Label {
                text: page.osdTitle
                color: Theme.textPrimary
                font.pixelSize: 13
                font.bold: true
            }

            Label {
                text: page.osdValue
                color: Theme.textSecondary
                font.pixelSize: 13
            }
        }

        Rectangle {
            id: meter
            visible: page.osdMeter >= 0
            anchors.left: osdRow.left
            anchors.top: osdRow.bottom
            anchors.topMargin: 7
            width: 210
            height: 5
            radius: height / 2
            color: Theme.floatingStroke

            Rectangle {
                width: parent.width * Math.max(0, page.osdMeter)
                height: parent.height
                radius: parent.radius
                color: Theme.accent
            }
        }
    }

    Rectangle {
        id: bar
        anchors.horizontalCenter: stage.horizontalCenter
        anchors.bottom: stage.bottom
        anchors.bottomMargin: page.chromeBottomInset
        width: page.compactChrome ? stage.width : Math.min(page.chromeMaxWidth,
                        Math.max(0, stage.width - page.chromeSideInset * 2))
        height: page.chromeHeight
        radius: page.compactChrome ? 0 : 24
        color: Theme.floatingBackground
        border.width: Theme.cardBorderWidth
        border.color: Theme.floatingStroke
        opacity: page.chromeShown ? 1 : 0
        visible: page.chromeShown || opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }

        HoverHandler {
            id: barHover
            onHoveredChanged: {
                page.barHovered = barHover.hovered
                if (barHover.hovered)
                    page.notePointerMovement()
                else
                    page.scheduleHide()
            }
        }

        Column {
            id: barColumn
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            // Timeline: 4 pt resting track, 13 pt knob, scrub preview on
            // drag, seek only on release (VideoTimelineTrack and
            // scrub/endScrubbing in VideoControlsView.swift).
            Item {
                id: timeline
                width: parent.width
                height: 18

                readonly property var markers: {
                    if (mpv.duration <= 0)
                        return []
                    const chapters = mpv.chapters.slice().sort((a, b) => a.start - b.start)
                    const result = []
                    let last = -Infinity
                    for (const chapter of chapters) {
                        if (chapter.start > 0 && chapter.start < mpv.duration
                                && chapter.start - last >= 0.05) {
                            result.push({ "id": chapter.id, "ratio": chapter.start / mpv.duration })
                            last = chapter.start
                        }
                    }
                    return result
                }
                readonly property var activeChapter: studyModel.currentChapter >= 0
                    ? mpv.chapters[studyModel.currentChapter] : null

                function markerAt(pointerX: real): int {
                    let closest = -1
                    let distance = 5
                    for (const marker of timeline.markers) {
                        const delta = Math.abs(pointerX - timeline.width * marker.ratio)
                        if (delta <= distance) {
                            closest = marker.id
                            distance = delta
                        }
                    }
                    return closest
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 4
                    radius: 2
                    color: Theme.cardStroke
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    width: timeline.width * page.displayRatio
                    height: 4
                    radius: 2
                    color: Theme.textPrimary
                    opacity: 0.85
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !!timeline.activeChapter && mpv.duration > 0
                    x: visible ? timeline.width * timeline.activeChapter.start / mpv.duration : 0
                    width: visible ? Math.max(0, Math.min(timeline.width - x,
                        timeline.width * (timeline.activeChapter.duration || 0) / mpv.duration)) : 0
                    height: 4
                    radius: 2
                    color: Theme.accent
                }

                Repeater {
                    model: timeline.markers
                    delegate: Rectangle {
                        id: chapterTick
                        required property var modelData
                        x: Math.max(0, Math.min(timeline.width - width,
                            timeline.width * chapterTick.modelData.ratio - width / 2))
                        anchors.verticalCenter: parent.verticalCenter
                        width: 2
                        height: 9
                        radius: 1
                        color: Theme.textSecondary
                    }
                }

                Rectangle {
                    id: knob
                    x: Math.max(0, Math.min(timeline.width - width,
                                            timeline.width * page.displayRatio - width / 2))
                    anchors.verticalCenter: parent.verticalCenter
                    width: 13
                    height: 13
                    radius: 6.5
                    color: Theme.textPrimary
                    visible: timelineMouse.containsMouse || page.scrubbing
                }

                Rectangle {
                    id: scrubBubble
                    x: Math.max(0, Math.min(timeline.width - width,
                                            timeline.width * page.displayRatio - width / 2))
                    y: -height - 6
                    width: scrubBubbleText.implicitWidth + 16
                    height: 22
                    radius: 6
                    color: Theme.popupBackground
                    border.width: Theme.cardBorderWidth
                    border.color: Theme.popupDivider
                    visible: page.scrubbing

                    Label {
                        id: scrubBubbleText
                        anchors.centerIn: parent
                        text: page.formatTime(page.scrubTime)
                        color: Theme.popupText
                        font.pixelSize: 11
                    }
                }

                MouseArea {
                    id: timelineMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    property int pressedChapter: -1
                    property real pressX: 0
                    onPressed: function (mouse) {
                        timelineMouse.pressX = mouse.x
                        timelineMouse.pressedChapter = timeline.markerAt(mouse.x)
                        if (timelineMouse.pressedChapter < 0)
                            page.beginScrub(mouse.x)
                    }
                    onPositionChanged: function (mouse) {
                        if (!timelineMouse.pressed)
                            return
                        if (timelineMouse.pressedChapter >= 0
                                && Math.abs(mouse.x - timelineMouse.pressX) > 5) {
                            timelineMouse.pressedChapter = -1
                            page.beginScrub(mouse.x)
                        } else if (timelineMouse.pressedChapter < 0) {
                            page.updateScrub(mouse.x)
                        }
                    }
                    onReleased: {
                        if (timelineMouse.pressedChapter >= 0)
                            mpv.seekToChapter(timelineMouse.pressedChapter)
                        else
                            page.endScrub()
                        timelineMouse.pressedChapter = -1
                    }
                    onCanceled: {
                        timelineMouse.pressedChapter = -1
                        page.endScrub()
                    }
                }
            }

            Flickable {
                id: controlScroller
                width: parent.width
                height: 42
                contentWidth: Math.max(width, controlRow.implicitWidth)
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalFlick
                interactive: contentWidth > width
                ScrollBar.horizontal: ScrollBar {}
            }

            RowLayout {
                id: controlRow
                parent: controlScroller.contentItem
                width: controlScroller.contentWidth
                height: controlScroller.height
                spacing: 2
                BarButton {
                    glyph: "\u2039"
                    tip: qsTr("Close Player")
                    glyphSize: 18
                    buttonWidth: 30
                    onClicked: page.closePlayer()
                }

                BarButton {
                    glyph: mpv.paused ? "\u25B6" : "II"
                    tip: mpv.paused ? qsTr("Play (Space)") : qsTr("Pause (Space)")
                    glyphSize: 15
                    buttonWidth: 42
                    buttonHeight: 42
                    onClicked: page.togglePause()
                }

                BarButton {
                    visible: mpv.resumeAvailable
                    glyph: "\u21A4"
                    tip: qsTr("Restart from Beginning")
                    Accessible.name: tip
                    onClicked: {
                        mpv.restartFromBeginning()
                        page.showOSD(qsTr("Restarted from beginning"), "")
                    }
                }

                BarButton {
                    glyph: "\u2212" + page.seekInterval
                    tip: qsTr("Back %1 s (Left)").arg(page.seekInterval)
                    buttonWidth: 30
                    onClicked: page.skip(-page.seekInterval)
                }

                BarButton {
                    glyph: "+" + page.seekInterval
                    tip: qsTr("Forward %1 s (Right)").arg(page.seekInterval)
                    buttonWidth: 30
                    onClicked: page.skip(page.seekInterval)
                }

                Label {
                    Layout.preferredWidth: 48
                    Layout.minimumWidth: 48
                    Layout.maximumWidth: 48
                    Layout.alignment: Qt.AlignVCenter
                    text: page.formatTime(page.displayTime)
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignRight
                }

                Label {
                    Layout.preferredWidth: 8
                    Layout.alignment: Qt.AlignVCenter
                    text: "/"
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                }

                Label {
                    Layout.preferredWidth: 48
                    Layout.minimumWidth: 48
                    Layout.maximumWidth: 48
                    Layout.alignment: Qt.AlignVCenter
                    text: page.formatTime(mpv.duration)
                    color: Theme.textSecondary
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignLeft
                }

                BarButton {
                    glyph: "M"
                    tip: mpv.muted ? qsTr("Unmute (M)") : qsTr("Mute (M)")
                    active: mpv.muted
                    onClicked: page.toggleMute()
                }

                Slider {
                    id: volumeSlider
                    Layout.preferredWidth: 56
                    Layout.minimumWidth: 56
                    Layout.maximumWidth: 56
                    Layout.preferredHeight: 24
                    Layout.minimumHeight: 24
                    Layout.maximumHeight: 24
                    Layout.alignment: Qt.AlignVCenter
                    from: 0
                    to: 100
                    stepSize: 1
                    focusPolicy: Qt.NoFocus
                    // The slider is pushed from the engine instead of bound,
                    // so dragging it does not break the value binding and the
                    // volume keys keep moving the handle afterwards.
                    Component.onCompleted: volumeSlider.value = mpv.volume
                    onMoved: page.setVolume(volumeSlider.value)

                    Connections {
                        target: mpv
                        function onVolumeChanged() {
                            volumeSlider.value = Math.max(0, Math.min(100, mpv.volume))
                        }
                    }
                }

                Button {
                    id: speedPill
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: 46
                    implicitHeight: 24
                    padding: 0
                    focusPolicy: Qt.NoFocus
                    text: page.formatSpeed(mpv.speed)
                    onClicked: {
                        page.speedPanelOpen = !page.speedPanelOpen
                        page.notePointerMovement()
                    }

                    ToolTip.visible: speedPill.hovered
                    ToolTip.text: qsTr("Playback Speed")

                    contentItem: Text {
                        text: speedPill.text
                        color: Theme.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 12
                        color: page.speedPanelOpen ? Theme.cardStroke
                                                   : Theme.floatingBackground
                        border.width: Theme.cardBorderWidth
                        border.color: Theme.cardStroke
                    }
                }

                BarButton {
                    glyph: "CC"
                    tip: qsTr("Cycle Subtitle Track (S)")
                    onClicked: page.cycleSubtitles()
                }

                BarButton {
                    glyph: "S\u2212"
                    tip: qsTr("Subtitle Delay -0.05 s (Alt+,)")
                    buttonWidth: 28
                    onClicked: page.adjustSubtitleDelay(-0.05)
                }

                BarButton {
                    glyph: "S+"
                    tip: qsTr("Subtitle Delay +0.05 s (Alt+.)")
                    buttonWidth: 28
                    onClicked: page.adjustSubtitleDelay(0.05)
                }

                BarButton {
                    glyph: "a\u2212"
                    tip: qsTr("Audio Delay -0.5 s (Alt+Shift+,)")
                    buttonWidth: 28
                    onClicked: page.adjustAudioDelay(-0.5)
                }

                BarButton {
                    glyph: "a+"
                    tip: qsTr("Audio Delay +0.5 s (Alt+Shift+.)")
                    buttonWidth: 28
                    onClicked: page.adjustAudioDelay(0.5)
                }

                BarButton {
                    glyph: "A"
                    tip: qsTr("Set A-B Loop Start (Alt+A)")
                    active: mpv.loopStart >= 0
                    onClicked: page.setLoopA(mpv.position)
                }

                BarButton {
                    glyph: "B"
                    tip: qsTr("Set A-B Loop End (Alt+B)")
                    active: mpv.loopEnd >= 0
                    onClicked: page.setLoopB(mpv.position)
                }

                BarButton {
                    glyph: "AB"
                    tip: qsTr("Clear A-B Loop")
                    buttonWidth: 30
                    onClicked: page.clearABLoop()
                }

                BarButton {
                    glyph: "\u21BB"
                    tip: qsTr("Rotate Clockwise (R)")
                    glyphSize: 14
                    onClicked: page.rotateClockwise()
                }

                BarButton {
                    glyph: page.fullScreen ? "][" : "[]"
                    tip: page.fullScreen ? qsTr("Leave Full Screen (F)")
                                        : qsTr("Full Screen (F)")
                    onClicked: page.toggleFullScreen()
                }

                BarButton {
                    glyph: qsTr("Open")
                    tip: qsTr("Open a Video")
                    buttonWidth: 38
                    onClicked: openMenu.popup()
                }
                BarButton {
                    glyph: "\u2699"
                    tip: qsTr("Video Settings")
                    Accessible.name: tip
                    onClicked: videoSettings.open()
                }
            }
        }

        // VideoControlsView.speedControlPanel: 264 wide, 4 x 2 presets and the
        // 0.25...5 slider. It is a popup surface, so it uses the popup tokens.
        Rectangle {
            id: speedPanel
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.bottom: parent.top
            anchors.bottomMargin: 8
            width: 264
            height: speedColumn.implicitHeight + 28
            visible: page.speedPanelOpen
            onVisibleChanged: {
                if (visible)
                    speedInput.text = mpv.speedLabel(mpv.speed)
            }
            radius: 20
            color: Theme.popupBackground
            border.width: Theme.cardBorderWidth
            border.color: Theme.popupDivider

            HoverHandler {
                id: speedPanelHover
                onHoveredChanged: {
                    page.speedPanelHovered = speedPanelHover.hovered
                    if (!speedPanelHover.hovered)
                        page.scheduleHide()
                }
            }

            Column {
                id: speedColumn
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    width: parent.width
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Playback Speed")
                        color: Theme.popupText
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Label {
                        text: page.formatSpeed(mpv.speed)
                        color: Theme.popupTextSecondary
                        font.pixelSize: 13
                    }
                }

                GridLayout {
                    width: parent.width
                    columns: 4
                    columnSpacing: 6
                    rowSpacing: 6

                    Repeater {
                        model: [0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0]

                        delegate: Button {
                            id: speedPreset
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 26
                            focusPolicy: Qt.NoFocus
                            text: page.formatSpeed(speedPreset.modelData)
                            onClicked: page.setSpeed(speedPreset.modelData)

                            contentItem: Text {
                                text: speedPreset.text
                                color: page.speedIs(speedPreset.modelData)
                                    ? Theme.windowBackground : Theme.popupText
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                radius: 6
                                color: page.speedIs(speedPreset.modelData)
                                    ? Theme.accent : Theme.popupDivider
                            }
                        }
                    }
                }

                RowLayout {
                    width: parent.width
                    spacing: 8

                    Slider {
                        id: speedSlider
                        Layout.fillWidth: true
                        focusPolicy: Qt.NoFocus
                        from: 0.3
                        to: 5.0
                        stepSize: 0.1
                        Component.onCompleted: speedSlider.value = mpv.speed
                        onMoved: page.setSpeed(speedSlider.value)

                        Connections {
                            target: mpv
                            function onSpeedChanged() {
                                speedSlider.value = mpv.speed
                            }
                        }
                    }

                    TextField {
                        id: speedInput
                        Layout.preferredWidth: 62
                        placeholderText: qsTr("Custom")
                        Accessible.name: qsTr("Custom playback speed")
                        color: Theme.popupTextSecondary
                        font.pixelSize: 11
                        horizontalAlignment: TextInput.AlignRight
                        onAccepted: {
                            if (mpv.acceptSpeedInput(text))
                                page.showOSD(qsTr("Speed"), page.formatSpeed(mpv.speed))
                            text = mpv.speedLabel(mpv.speed)
                        }

                        Connections {
                            target: mpv
                            function onSpeedChanged() {
                                speedInput.text = mpv.speedLabel(mpv.speed)
                            }
                        }
                    }

                    Label {
                        text: "x"
                        color: Theme.popupTextSecondary
                    }
                }
            }
        }
    }

    BarButton {
        anchors.top: stage.top
        anchors.right: stage.right
        anchors.margins: Theme.spacingMd
        visible: mpv.loaded && (page.chromeShown || page.studyOpen)
        glyph: qsTr("Study")
        tip: qsTr("Toggle Study Sidebar")
        active: page.studyOpen
        buttonWidth: 58
        onClicked: {
            page.studyOpen = !page.studyOpen
            page.forceActiveFocus()
            page.notePointerMovement()
        }
    }

    VideoSettingsPanel {
        id: videoSettings
        engine: mpv
        anchors.centerIn: parent
        width: Math.min(560, page.width)
        height: Math.min(690, page.height)
        onClosed: page.forceActiveFocus()
    }

    VideoOpenMenu {
        id: openMenu
        extensionPattern: page.extensionPattern
        onVideoChosen: (file, fromBeginning) => page.openVideo(file, fromBeginning)
        onSelectionCanceled: page.forceActiveFocus()
    }

    component BarButton: Button {
        id: barButton

        property string glyph
        property string tip
        property bool active: false
        property int glyphSize: 11
        property int buttonWidth: 28
        property int buttonHeight: 28

        implicitWidth: barButton.buttonWidth
        implicitHeight: barButton.buttonHeight
        padding: 0
        focusPolicy: Qt.NoFocus
        Layout.alignment: Qt.AlignVCenter
        text: barButton.glyph

        ToolTip.visible: barButton.hovered && barButton.tip.length > 0
        ToolTip.text: barButton.tip
        ToolTip.delay: 700

        contentItem: Text {
            text: barButton.glyph
            color: barButton.active ? Theme.accent : Theme.textPrimary
            font.pixelSize: barButton.glyphSize
            font.bold: barButton.active
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: barButton.buttonHeight / 2
            color: barButton.active || barButton.down || barButton.hovered
                ? Theme.cardStroke : Theme.floatingBackground
            border.width: barButton.active ? Theme.cardBorderWidth : 0
            border.color: Theme.cardStroke
        }
    }
}
