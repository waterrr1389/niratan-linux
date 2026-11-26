// SPDX-License-Identifier: GPL-3.0-or-later
//
// The chrome is VideoControlBar, following VideoControlsView.swift; this page
// supplies the playback snapshot, the chrome visibility rule and the actions.
// VideoPlayerScreen+OSD.swift specifies the OSD; VideoPlayerScreen+Shortcuts.swift
// and VideoShortcutActions.swift specify keys. VideoPlayerScreen+Chrome.swift
// specifies auto-hide and pointer reveal. Playback goes through MpvItem only.
// Floating theme tokens keep overlays legible over the black video stage.
// The playback bar and study sidebar share the same A-B loop state.
// Episode navigation follows VideoPlaylist.swift (previousURL, nextURL) and
// VideoPlayerViewModel.swift (playPrevious, playNext, onPlaybackEnded); the
// autoplay preference is Core/UserConfig.swift's videoAutoPlayNext.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Niratan

Item {
    id: page

    // VideoControlsView.swift: the bar chooses its layout and metrics.
    readonly property bool compactChrome: mpv.videoSettings.videoControlBarLayout === "compactBottom"
    readonly property int seekInterval: Math.floor(mpv.videoSettings.videoSeekInterval)
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
            || page.scrubbing || page.speedPanelOpen || page.studyOpen
            || page.inspectorOpen || lookupLayer.hasPopup)
    readonly property bool fullScreen: page.Window.window
        ? page.Window.window.visibility === Window.FullScreen
        : false

    // The readiness the shortcut and delay actions use. It normally follows
    // mpv; keeping it as a page property lets headless tests exercise the key
    // handlers without an OpenGL render context.
    property bool playbackReady: mpv.loaded
    property bool chromeVisible: true
    property bool barHovered: false
    property bool scrubbing: false
    property real scrubTime: 0
    property bool speedPanelOpen: false
    property bool speedPanelHovered: false
    property bool studyOpen: false
    // The Inspector panel: VideoPlayerScreen.isInspectorVisible.
    property bool inspectorOpen: false
    property string osdTitle: ""
    property string osdValue: ""
    property real osdMeter: -1
    // The loaded local video, for the video mining context.
    property url videoFile
    // VideoPlayerScreen.model.currentURL != nil: a Video session is open from
    // the open request until unloadVideo, and the library suspends thumbnail
    // generation for its whole lifetime (Decisions D9, D10).
    readonly property bool videoSessionActive: page.videoFile.toString().length > 0
    // The library opens the player with its first video already chosen.
    property url initialVideoUrl
    property url initialSubtitleUrl
    property bool startFromBeginning: false
    // A bound subtitle waits for mpv to open the file; sub-add before it fails.
    property url pendingSubtitleUrl
    // VideoPlayerScreen.shouldShowVideoLoadingIndicator: an open with a URL,
    // no loaded file and no error.
    readonly property bool openingVideo: page.videoFile.toString().length > 0
                                         && !mpv.loaded && mpv.error.length === 0

    onStudyOpenChanged: page.scheduleHide()

    focus: true
    Keys.priority: Keys.BeforeItem
    Keys.onPressed: function (event) {
        page.handleKey(event)
    }
    Keys.onReleased: function (event) {
        if (event.key === Qt.Key_Shift) {
            lookupLayer.setShiftPressed(false)
            event.accepted = true
        }
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

    // VideoPlayerScreen+Chrome.swift: reveal on pointer movement and on the
    // episode commands; hiding is scheduled only once a video is loaded.
    function notePointerMovement() {
        page.chromeVisible = true
        page.scheduleHide()
    }

    function scheduleHide() {
        if (!mpv.loaded || page.studyOpen || page.inspectorOpen) {
            hideTimer.stop()
            return
        }
        hideTimer.restart()
    }

    function handleHideTimeout() {
        if (page.barHovered || page.speedPanelHovered || page.scrubbing || mpv.paused
                || page.speedPanelOpen || page.inspectorOpen || lookupLayer.hasPopup)
            return
        if (page.speedPanelOpen)
            page.speedPanelOpen = false
        page.chromeVisible = false
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
        // Swift shows the normalized request; while a subtitle gap accelerates,
        // the engine keeps the temporary speed and only the base moves.
        page.showOSD(qsTr("Speed"), page.formatSpeed(value))
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
        if (!page.playbackReady)
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

    // VideoPlayerScreen.setSubtitleDelayWithOSD: the Inspector changes the
    // delay before a file is loaded, so these actions skip the loaded guard
    // the shortcut actions keep (Decision).
    function applySubtitleDelay(value) {
        var delay = Math.max(-60, Math.min(60, value))
        mpv.subtitleDelay = delay
        page.showOSD(qsTr("Subtitle Delay"), page.formatDelay(delay))
    }

    function applyAudioDelay(value) {
        var delay = Math.max(-30, Math.min(30, value))
        mpv.audioDelay = delay
        page.showOSD(qsTr("Audio Delay"), page.formatDelay(delay))
    }

    // PlayerInspectorPanel.selectTrackRequested: sub goes through the
    // remembering subtitle calls, audio and video set vid/aid.
    function applyTrackSelection(type, id) {
        if (type === "sub") {
            if (id > 0) {
                mpv.selectSubtitleTrack(id)
                mpv.subtitlesVisible = true
            } else {
                mpv.disableSubtitles()
            }
        } else if (type === "audio" || type === "video") {
            mpv.selectTrack(type, id)
        }
    }

    // The Inspector mirrors mpv's snapshot; the page pushes it on every
    // signal the panel shows.
    function syncInspectorState() {
        inspectorModel.setPlaybackState(mpv.speed, mpv.subtitleDelay, mpv.audioDelay)
    }

    function toggleInspector() {
        if (page.inspectorOpen) {
            page.inspectorOpen = false
            page.forceActiveFocus()
            page.notePointerMovement()
            return
        }
        inspectorModel.setVideo(page.videoFile, "")
        inspectorModel.setTracks(mpv.tracks)
        page.syncInspectorState()
        page.inspectorOpen = true
        page.chromeVisible = true
        page.notePointerMovement()
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
        const id = mpv.cycleSubtitles()
        if (id === 0) {
            page.showOSD(qsTr("Subtitles Off"), "")
        } else if (id > 0) {
            const track = studySidebar.subtitleTracks.find(track => track.id === id)
            if (track)
                page.showOSD(qsTr("Subtitle Track"), track.title || track.language
                             || qsTr("Subtitle Track %1").arg(id))
        }
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

    // Clears everything that belongs to the previous clip, so a reused window
    // never shows its title, panels or alert.
    function resetVideoState() {
        page.pendingSubtitleUrl = ""
        page.inspectorOpen = false
        page.studyOpen = false
        page.speedPanelOpen = false
        page.speedPanelHovered = false
        page.osdTitle = ""
        page.osdValue = ""
        page.osdMeter = -1
        osdTimer.stop()
        if (videoErrorDialog.visible)
            videoErrorDialog.close()
    }

    // Called by the Video window when it closes: stop the player, leave full
    // screen and forget the clip before the window hides.
    function unloadVideo() {
        page.exitFullScreen()
        mpv.unload()
        page.videoFile = ""
        inspectorModel.setVideo("", "")
        page.resetVideoState()
    }

    // VideoWindow.openVideo -> VideoPlayerScreen+Opening.handleExternalOpenRequest:
    // replay the page state, then open the new clip with its subtitle.
    function openRequest(url, subtitleUrl, fromBeginning) {
        page.resetVideoState()
        page.initialVideoUrl = url
        page.initialSubtitleUrl = subtitleUrl
        page.startFromBeginning = fromBeginning
        page.openVideo(url, fromBeginning)
        page.pendingSubtitleUrl = subtitleUrl
    }

    function closePlayer() {
        const window = page.Window.window
        if (window)
            window.close()
    }

    function openVideo(url, fromBeginning) {
        if (!url || url.toString().length === 0)
            return
        // VideoPlayerScreen+Opening.openPlaylistEpisode leaves the Inspector
        // open; resetVideoState closes it for unloads and fresh opens.
        const keepInspector = page.inspectorOpen
        page.resetVideoState()
        page.inspectorOpen = keepInspector
        page.pendingSubtitleUrl = ""
        page.speedPanelOpen = false
        page.speedPanelHovered = false
        page.chromeVisible = true
        page.osdTitle = ""
        page.osdValue = ""
        osdTimer.stop()
        page.videoFile = url
        studyModel.setVideo(url)
        inspectorModel.setVideo(url, "")
        inspectorModel.setTracks(mpv.tracks)
        page.syncInspectorState()
        page.forceActiveFocus()
        mpv.open(url, fromBeginning === true)
        page.notePointerMovement()
    }

    // VideoPlayerScreen.swift onPrevious/onNext: the transport buttons and the
    // shortcuts run the same episode command and reveal the controls.
    function playPreviousEpisode() {
        if (!inspectorModel.playPreviousEpisode())
            return false
        page.notePointerMovement()
        return true
    }

    function playNextEpisode() {
        if (!inspectorModel.playNextEpisode())
            return false
        page.notePointerMovement()
        return true
    }

    // VideoPlayerScreen.swift toggleSubtitleGapFastForward: flip the durable
    // preference; the engine reloads it and reevaluates the current position.
    function toggleSubtitleGapFastForward() {
        mpv.changeVideoSetting("videoSubtitleGapFastForwardEnabled",
                               !mpv.videoSettings.videoSubtitleGapFastForwardEnabled)
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
        page.showSubtitleLoadError()
        return false
    }

    function showSubtitleLoadError() {
        studyModel.reportSubtitleLoadFailure()
        videoErrorDialog.showError(qsTr("Could not load the subtitle file."))
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
        if (event.key === Qt.Key_Shift) {
            lookupLayer.setShiftPressed(true)
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Escape && page.fullScreen) {
            event.accepted = true
            if (event.isAutoRepeat)
                return
            page.exitFullScreen()
            return
        }
        if (!page.playbackReady || page.textFieldFocused())
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
        // Swift uses Command+Shift+arrows for episodes; Linux follows the
        // Control+Shift mapping from Niratan-win. They act only with a neighbor
        // present and never take a repeat, plain arrows or the subtitle
        // combinations.
        if (event.key === Qt.Key_Left && control && shift && !alt && !meta) {
            event.accepted = !event.isAutoRepeat && page.playPreviousEpisode()
            return
        }
        if (event.key === Qt.Key_Right && control && shift && !alt && !meta) {
            event.accepted = !event.isAutoRepeat && page.playNextEpisode()
            return
        }
        if (control || meta)
            return

        // VideoShortcutActions.swift toggleSubtitleGapFastForward is Shift+F;
        // a repeat does not flip the preference twice.
        if (event.key === Qt.Key_F && shift && !alt) {
            event.accepted = !event.isAutoRepeat
            if (!event.isAutoRepeat)
                page.toggleSubtitleGapFastForward()
            return
        }

        var handled = true
        var plain = !alt && !shift
        if (event.key === Qt.Key_Left && alt && !shift) {
            if (event.isAutoRepeat || !studyModel.seekPreviousSubtitleCue())
                handled = false
        } else if (event.key === Qt.Key_Right && alt && !shift) {
            if (event.isAutoRepeat || !studyModel.seekNextSubtitleCue())
                handled = false
        } else if (event.key === Qt.Key_Left && shift && !alt) {
            if (event.isAutoRepeat || !studyModel.alignPreviousSubtitleToCurrentTime())
                handled = false
        } else if (event.key === Qt.Key_Right && shift && !alt) {
            if (event.isAutoRepeat || !studyModel.alignNextSubtitleToCurrentTime())
                handled = false
        } else if (event.key === Qt.Key_Space && plain) {
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

    MiningViewModel {
        id: mining
    }

    AnkiMiningToast {
        status: mining.toastStatus
        message: mining.toastMessage
    }

    PlayerInspectorViewModel {
        id: inspectorModel
        objectName: "inspectorModel"
        onEpisodeRequested: function (url) { page.openVideo(url, false) }
    }

    PlayerStudyViewModel {
        id: studyModel
        chapters: mpv.chapters
        onSeekRequested: function(seconds) { mpv.seek(seconds) }
        onSubtitleAlignmentRequested: function(delay) {
            if (lookupLayer.hasPopup)
                lookupLayer.closeLookup()
            page.setSubtitleDelay(delay)
        }
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

    // The gap policy runs in the engine against the primary transcript the
    // study model parsed, never the filtered window.
    Connections {
        target: studyModel

        function onTranscriptChanged() {
            mpv.setPrimaryCues(studyModel.primaryCues)
        }
    }

    StudySidebar {
        id: studySidebar
        objectName: "studySidebar"
        visible: page.studyOpen
        anchors.top: page.top
        anchors.right: page.right
        anchors.bottom: page.bottom
        preferredWidth: mpv.videoSettings.studySidebarWidth
        width: Math.min(studySidebar.implicitWidth,
                        Math.max(studySidebar.minimumWidth, page.width - 340))
        onWidthSaveRequested: width => mpv.changeVideoSetting("studySidebarWidth", width)
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

    Component.onCompleted: {
        page.forceActiveFocus()
        if (page.initialVideoUrl.toString().length > 0) {
            page.openVideo(page.initialVideoUrl, page.startFromBeginning)
            page.pendingSubtitleUrl = page.initialSubtitleUrl
        }
    }

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
            objectName: "mpvItem"
            anchors.fill: parent

            onErrorChanged: videoErrorDialog.showError(mpv.error)
            onSubtitleLoadFailed: page.showSubtitleLoadError()
            onLoadedChanged: {
                studyModel.setPlaybackReady(mpv.loaded)
                inspectorModel.setTracks(mpv.tracks)
                page.syncInspectorState()
                if (!mpv.loaded)
                    return
                if (page.pendingSubtitleUrl.toString().length > 0) {
                    const subtitle = page.pendingSubtitleUrl
                    page.pendingSubtitleUrl = ""
                    page.loadSubtitle(subtitle)
                }
                page.speedPanelOpen = false
                page.speedPanelHovered = false
                page.chromeVisible = true
                if (!videoErrorDialog.visible)
                    page.forceActiveFocus()
                page.notePointerMovement()
            }
            onPositionChanged: studyModel.updatePlayback(mpv.position, mpv.subtitleDelay)
            onSubtitleDelayChanged: {
                studyModel.updatePlayback(mpv.position, mpv.subtitleDelay)
                page.syncInspectorState()
            }
            onTracksChanged: {
                studyModel.setTracks(mpv.tracks)
                inspectorModel.setTracks(mpv.tracks)
            }
            onSpeedChanged: page.syncInspectorState()
            onAudioDelayChanged: page.syncInspectorState()
            onVideoSettingsChanged: studyModel.refreshHistory()
            onSubtitlesVisibleChanged: studyModel.setSubtitlesVisible(mpv.subtitlesVisible)
            onSubtitleCueChanged: studyModel.updateNativeCue(mpv.subtitleText, mpv.subtitleStart,
                                                             mpv.subtitleEnd)
            onSubtitleDocumentChanged: function(file, bytes, readable) {
                studyModel.setSubtitleDocument(file, bytes, readable)
                if (file.toString().length > 0 && !readable)
                    videoErrorDialog.showError(qsTr("Could not read the subtitle file."))
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
                // VideoPlayerViewModel.onPlaybackEnded: only EOF consults the
                // stored autoplay flag, and only when it is currently true.
                if (mpv.videoSettings.videoAutoPlayNext)
                    page.playNextEpisode()
            }
        }

        // VideoPlayerScreen.swift draws the overlay and the lookup popup in
        // the video canvas above the surface.
        VideoLookupLayer {
            id: lookupLayer
            anchors.fill: parent
            z: 2
            engine: mpv
            miningModel: mining
            videoFile: page.videoFile
            paused: mpv.paused
            bottomInset: page.compactChrome ? 28 : 56
            subtitleBottomInset: page.chromeShown
                ? Math.max(0, stage.y + stage.height - controlBar.y) + Theme.inspectorOverlayBarGap
                : 0
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
        onSingleTapped: {
            if (page.inspectorOpen) {
                page.inspectorOpen = false
                page.forceActiveFocus()
                page.notePointerMovement()
                return
            }
            page.togglePause()
        }
        onDoubleTapped: {
            if (!page.inspectorOpen)
                page.toggleFullScreen()
        }
    }

    PlayerEmptyState {
        objectName: "playerEmptyState"
        anchors.fill: stage
        visible: page.videoFile.toString().length === 0
        onFileChosen: function (file) { page.openVideo(file, false) }
    }

    PlayerLoadingOverlay {
        anchors.fill: stage
        z: 50
        visible: page.openingVideo
    }

    PlayerOsd {
        anchors.top: stage.top
        anchors.left: stage.left
        title: page.osdTitle
        value: page.osdValue
        meter: page.osdMeter
    }

    VideoControlBar {
        id: controlBar

        objectName: "videoControlBar"
        z: 2
        anchors.horizontalCenter: stage.horizontalCenter
        y: page.compactChrome
           ? Math.max(0, stage.height - controlBar.height)
           : Math.max(16, stage.height - page.chromeBottomInset - controlBar.height)
        availableWidth: stage.width
        chapters: mpv.chapters
        layoutMode: page.compactChrome ? "compactBottom" : "floating"
        videoFileSet: page.videoFile.toString().length > 0
        chromeShown: page.chromeShown
        isPlaying: mpv.loaded && !mpv.paused
        isPaused: mpv.paused
        isMuted: mpv.muted
        volume: mpv.volume
        currentTime: page.displayTime
        duration: mpv.duration
        speed: mpv.speed
        scrubbing: page.scrubbing
        scrubTime: page.scrubTime
        canMine: studyModel.canMine
        studyOpen: page.studyOpen
        isFullScreen: page.fullScreen
        speedPanelOpen: page.speedPanelOpen
        inspectorOpen: page.inspectorOpen
        previousEnabled: inspectorModel.canPlayPreviousEpisode
        nextEnabled: inspectorModel.canPlayNextEpisode
        gapFastForwardEnabled: mpv.videoSettings.videoSubtitleGapFastForwardEnabled

        onTogglePlaybackRequested: page.togglePause()
        onPreviousRequested: page.playPreviousEpisode()
        onNextRequested: page.playNextEpisode()
        onVolumeChangeRequested: function (value) {
            page.setVolume(value)
        }
        onToggleMuteRequested: page.toggleMute()
        onSpeedChangeRequested: function (value) {
            page.setSpeed(value)
        }
        onSpeedPanelToggleRequested: {
            page.speedPanelOpen = !page.speedPanelOpen
            page.notePointerMovement()
        }
        onSpeedPanelCloseRequested: page.speedPanelOpen = false
        onToggleStudyRequested: {
            if (page.studyOpen && studySidebar.selectedTab === 0) {
                page.studyOpen = false
            } else {
                studySidebar.selectedTab = 0
                page.studyOpen = true
            }
            page.forceActiveFocus()
        }
        onOpenVideoRequested: openMenu.popup()
        onMineSubtitleRequested: studyModel.mineCurrentSubtitle()
        onToggleInspectorRequested: page.toggleInspector()
        onToggleFullScreenRequested: page.toggleFullScreen()
        onToggleGapFastForwardRequested: page.toggleSubtitleGapFastForward()
        onScrubStarted: function (seconds) {
            page.scrubbing = true
            page.scrubTime = seconds
            page.chromeVisible = true
        }
        onScrubUpdated: function (seconds) {
            page.scrubTime = seconds
        }
        onScrubFinished: function (seconds) {
            page.scrubbing = false
            mpv.seek(seconds)
            page.notePointerMovement()
        }
        onBarHoverChanged: function (hovered) {
            page.barHovered = hovered
            if (hovered)
                page.notePointerMovement()
            else
                page.scheduleHide()
        }
        onSpeedPanelHoverChanged: function (hovered) {
            page.speedPanelHovered = hovered
            if (!hovered)
                page.scheduleHide()
        }
    }

    // Whether the last item that held the keyboard was in the lookup popup.
    property bool lastFocusInLookup: false

    function inLookup(item) {
        for (let current = item; current; current = current.parent) {
            if (current === lookupLayer)
                return true
        }
        return false
    }

    Connections {
        target: page.Window.window

        function onActiveFocusItemChanged() {
            const focused = page.Window.activeFocusItem
            if (focused && focused !== page.Window.contentItem)
                page.lastFocusInLookup = page.inLookup(focused)
        }
    }

    Connections {
        target: lookupLayer

        function onHasPopupChanged() {
            if (lookupLayer.hasPopup) {
                page.chromeVisible = true
                hideTimer.stop()
            } else {
                // A click in the popup gives its web view the keyboard; once
                // it hides, the player's shortcuts (Space, T) must work again.
                // Hiding may already have dropped the focus, so the last item
                // that held it decides; focus elsewhere is left alone.
                const focused = page.Window.activeFocusItem
                const dropped = focused === null || focused === page.Window.contentItem
                if (page.inLookup(focused) || (dropped && page.lastFocusInLookup))
                    page.forceActiveFocus()
                page.scheduleHide()
            }
        }
    }

    VideoErrorDialog {
        id: videoErrorDialog

        focusTarget: page
    }

    // Popups are not items, so the page exposes item handles that mirror their
    // visibility and forward close(), for tests and for the bar wiring.
    Item {
        objectName: "videoOpenMenu"
        visible: openMenu.visible

        function close() {
            openMenu.close()
        }
    }

    VideoInspectorPanel {
        id: inspectorPanel

        engine: mpv
        model: inspectorModel
        open: page.inspectorOpen
        z: 1
        anchors.top: stage.top
        anchors.right: stage.right
        anchors.topMargin: Theme.inspectorOverlayInset
        anchors.rightMargin: Theme.inspectorOverlayInset
        maximumWidth: stage.width - 2 * Theme.inspectorOverlayInset
        // VideoPlayerScreen.inspectorOverlayBottomInset over the rendered bar
        // (Decision D3); the panel follows its content up to that room.
        maximumHeight: stage.height - Theme.inspectorOverlayInset - (page.compactChrome
            ? controlBar.height + Theme.inspectorOverlayBarGap
            : Theme.inspectorOverlayInset + Theme.inspectorFloatingBarInset
              + controlBar.height + Theme.inspectorOverlayBarGap)

        onCloseRequested: {
            page.inspectorOpen = false
            page.forceActiveFocus()
            page.notePointerMovement()
        }
        onSelectEpisodeRequested: function (url) { page.openVideo(url, false) }
        onSelectTrackRequested: function (type, id) { page.applyTrackSelection(type, id) }
        onSpeedRequested: function (value) { page.setSpeed(value) }
        onSubtitleDelayRequested: function (seconds) { page.applySubtitleDelay(seconds) }
        onAudioDelayRequested: function (seconds) { page.applyAudioDelay(seconds) }
        onOpenSubtitleFileRequested: function (file) { page.loadSubtitle(file) }
        onOpenTranscriptRequested: {
            studySidebar.selectedTab = 1
            page.studyOpen = true
            page.inspectorOpen = false
            page.forceActiveFocus()
        }
    }

    VideoOpenMenu {
        id: openMenu

        extensionPattern: page.extensionPattern
        onVideoChosen: (file, fromBeginning) => page.openVideo(file, fromBeginning)
        onSelectionCanceled: page.forceActiveFocus()
    }

}
