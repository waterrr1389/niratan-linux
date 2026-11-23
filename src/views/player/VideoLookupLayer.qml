// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video lookup layer: the subtitle overlay, the lookup popup surface and
// the wiring between them, following Features/Video/VideoPlayerScreen.swift
// (popupView: the overlay instance, popupView's miningContextProvider,
// areSubtitlesVisible and configureSubtitleRendering),
// Features/Video/VideoLookupCoordinator.swift (present, closeAll,
// resumePlaybackAfterPopupIfNeeded) and
// Features/Video/Playback/PlaybackEngine.swift (VideoPlaybackSource.title).
//
// The layer owns one PlayerLookupViewModel. Pause and play requests pause and
// resume mpv. The stored videoAutoPauseOnLookup and desktopLookupHoverDelayMs
// settings are pushed on creation and whenever the video settings change.
//
// Text subtitles: a readable external SRT/WebVTT/ASS/SSA document or a
// non-empty extracted embedded SRT becomes the lookup document and turns
// mpv's native rendering off, so the overlay draws it. An empty document, a
// read failure or a cancelled extraction turns native rendering back on
// (image tracks stay with mpv). The subtitle toggle hides the overlay without
// losing the selected track because native rendering stays off while a text
// document is loaded.
//
// A lookup: the overlay's click or Shift-hover calls lookupAt, remembers the
// cue, the anchor rectangle and the highlight result, and the view model
// opens the popup. The popup's generated HTML is loaded from the model's
// htmlChanged, and the theme colours are bound to the model. A no-result
// lookup or any close clears the highlight. The mining context is set when
// the popup opens, again when the active cue changes and immediately before
// each mine command, and closing clears it so a later dictionary mine never
// carries video fields.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: layer

    required property MpvItem engine
    required property MiningViewModel miningModel
    property url videoFile
    property bool paused: false
    property bool shiftPressed: false
    property real bottomInset: 56
    // Space the visible playback bar takes from the bottom of the picture;
    // the subtitle overlay keeps its block above it.
    property real subtitleBottomInset: 0

    readonly property bool hasPopup: playerLookup.hasPopup
    readonly property bool textDocumentLoaded: layer.externalText || layer.embeddedText
    // PlayerLookupViewModel's popupModel is a QObject; the surface needs the
    // concrete DictionaryPopupModel it holds.
    readonly property var popupSurfaceModel: playerLookup.popupModel
    property bool externalText: false
    property bool embeddedText: false
    property string highlightedCueId: ""
    property var lookupHighlight: null
    property rect lookupAnchor

    function closeLookup() {
        playerLookup.closeLookup()
        layer.clearHighlight()
    }

    function clearHighlight() {
        layer.highlightedCueId = ""
        layer.lookupHighlight = null
    }

    function applyLookupSettings() {
        playerLookup.setLookupSettings(layer.engine.videoSettings.videoAutoPauseOnLookup === true,
                                      layer.engine.videoSettings.desktopLookupHoverDelayMs)
    }

    function performLookup(cueId, offset, anchor) {
        const result = playerLookup.lookupAt(cueId, offset)
        if (!result || Object.keys(result).length === 0) {
            layer.clearHighlight()
            return
        }
        layer.lookupAnchor = anchor
        layer.highlightedCueId = cueId
        layer.lookupHighlight = result
    }

    function setShiftPressed(pressed) {
        layer.shiftPressed = pressed
        playerLookup.setShiftPressed(pressed)
        if (!pressed)
            playerLookup.cancelShiftHover()
    }

    // PlaybackEngine.swift VideoPlaybackSource.title: the last path component
    // without its final extension.
    function videoTitle(path) {
        const name = path.substring(path.lastIndexOf("/") + 1)
        const dot = name.lastIndexOf(".")
        return dot > 0 ? name.substring(0, dot) : name
    }

    function localPath(url) {
        const text = url.toString()
        const prefix = "file://"
        const path = text.startsWith(prefix) ? text.substring(prefix.length) : text
        try {
            return decodeURIComponent(path)
        } catch (error) {
            return path
        }
    }

    // MpvItem::screenshot takes a QUrl; a bare path has no file scheme, so
    // the capture would be rejected as not local.
    function fileUrl(path) {
        const segments = (path.startsWith("/") ? path : "/" + path).split("/")
        return "file://" + segments.map(encodeURIComponent).join("/")
    }

    function selectedAudioStreamIndex() {
        const tracks = layer.engine.tracks
        for (let i = 0; i < tracks.length; ++i) {
            const track = tracks[i]
            if (track.type === "audio" && track.isSelected)
                return track.ffIndex === undefined ? -1 : track.ffIndex
        }
        return -1
    }

    function updateVideoMiningContext() {
        const selection = playerLookup.miningSelection()
        if (!selection || Object.keys(selection).length === 0) {
            layer.miningModel.clearVideoContext()
            return
        }
        const context = {
            "path": layer.localPath(layer.videoFile),
            "title": layer.videoTitle(layer.localPath(layer.videoFile)),
            "audioStreamIndex": layer.selectedAudioStreamIndex(),
            "cueText": selection.cueText,
            "cueStart": selection.cueStart,
            "cueEnd": selection.cueEnd,
            "subtitleDelay": layer.engine.subtitleDelay,
            "duration": layer.engine.duration
        }
        if (selection.previousCueText !== undefined)
            context.previousCueText = selection.previousCueText
        if (selection.nextCueText !== undefined)
            context.nextCueText = selection.nextCueText
        layer.miningModel.setVideoContext(context)
    }

    PlayerLookupViewModel {
        id: playerLookup

        onPauseRequested: {
            if (layer.engine.loaded)
                layer.engine.paused = true
        }
        onPlayRequested: {
            if (layer.engine.loaded)
                layer.engine.paused = false
        }
    }

    function syncPlayback() {
        playerLookup.updatePlayback(layer.engine.position, layer.engine.subtitleDelay)
    }

    Binding {
        target: layer.engine
        property: "nativeSubtitleRendering"
        value: !layer.textDocumentLoaded
    }

    Connections {
        target: playerLookup

        function onPopupChanged() {
            if (!playerLookup.hasPopup) {
                layer.clearHighlight()
                // The shared mining view model must not carry video fields
                // into a later reader or dictionary mine.
                layer.miningModel.clearVideoContext()
            } else {
                layer.updateVideoMiningContext()
            }
        }
        function onActiveCueChanged() {
            if (playerLookup.hasPopup)
                layer.updateVideoMiningContext()
        }
    }

    // ReaderPage.qml gives its DictionaryPopupModel the theme colours; the
    // video popup model is created by the view model, so bind them here.
    Binding {
        target: layer.popupSurfaceModel
        property: "dark"
        value: Theme.dark
    }
    Binding {
        target: layer.popupSurfaceModel
        property: "background"
        value: Theme.popupBackground
    }
    Binding {
        target: layer.popupSurfaceModel
        property: "textColor"
        value: Theme.popupText
    }

    Connections {
        target: layer.popupSurfaceModel

        function onHtmlChanged() {
            if (layer.popupSurfaceModel.hasResults)
                lookupSurface.showResults(layer.popupSurfaceModel.html)
        }
    }

    Connections {
        target: layer.engine

        function onVideoSettingsChanged() {
            layer.applyLookupSettings()
        }
        function onPositionChanged() {
            layer.syncPlayback()
        }
        function onSubtitleDelayChanged() {
            layer.syncPlayback()
        }
        function onPausedChanged() {
            playerLookup.setPlaying(!layer.engine.paused)
        }
        function onSubtitleDocumentChanged(file, bytes, readable) {
            layer.externalText = file.toString().length > 0 && readable
            if (layer.externalText)
                layer.embeddedText = false
            playerLookup.setSubtitleDocument(file, bytes, readable)
            layer.syncPlayback()
        }
        function onEmbeddedTranscriptChanged(bytes, error) {
            layer.embeddedText = error.length === 0 && bytes.length > 0
            if (layer.embeddedText) {
                layer.externalText = false
                playerLookup.setEmbeddedSubtitle(bytes)
            } else {
                playerLookup.setSubtitleDocument("", "", false)
            }
            layer.syncPlayback()
        }
    }

    Connections {
        target: layer.miningModel

        function onScreenshotRequested(path) {
            layer.miningModel.screenshotFinished(
                layer.engine.screenshot(layer.fileUrl(path)))
        }
    }

    Component.onCompleted: {
        layer.applyLookupSettings()
        playerLookup.setPlaying(!layer.engine.paused)
    }

    VideoSubtitleOverlay {
        anchors.fill: parent
        bottomObstruction: layer.subtitleBottomInset
        visible: layer.textDocumentLoaded && layer.engine.subtitlesVisible
        engine: layer.engine
        lookupModel: playerLookup
        paused: layer.paused
        shiftPressed: layer.shiftPressed
        highlightedCueId: layer.highlightedCueId
        lookupHighlight: layer.lookupHighlight
        onLookupRequested: function(cueId, offset, anchor) {
            layer.performLookup(cueId, offset, anchor)
        }
    }

    // VideoPlayerScreen.swift shouldShowVideoDismissLayer and
    // dismissVideoOverlaysFromCanvas: while a popup is open, the canvas above
    // the subtitle overlay and below the popup closes it instead of toggling
    // pause or starting another lookup.
    MouseArea {
        anchors.fill: parent
        enabled: playerLookup.hasPopup
        acceptedButtons: Qt.LeftButton
        onClicked: layer.closeLookup()
    }

    VideoLookupSurface {
        id: lookupSurface
        visible: playerLookup.hasPopup
        popupModel: layer.popupSurfaceModel
        miningModel: layer.miningModel
        bottomInset: layer.bottomInset
        selectionAnchor: layer.lookupAnchor
        prepareMining: layer.updateVideoMiningContext
        onDismissRequested: layer.closeLookup()
    }
}
