// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows Features/Popup/PopupView.swift (popupContent, sasayakiControls,
// mineEntry's sasayakiCue and the wasPaused/onPause/onSasayakiJumpDismiss
// wiring), Core/AnkiManager.swift (needsSasayakiAudio), PopupWebView.swift
// (Coordinator.userContentController, updateUIView) and
// NativeMac/NativeReaderView.swift (handleSelection's cue and auto-pause,
// closePopup(resumePausedPlayback:)).

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebChannel
import QtWebEngine
import Niratan

Item {
    id: surface

    required property DictionaryPopupModel popupModel
    required property MiningViewModel miningModel
    required property SasayakiViewModel player
    property rect selectionAnchor
    // The cue captured at selection time and the pause this lookup recorded
    // (NativeReaderPopup.sasayakiCue and NativeReaderModel.wasPaused).
    property var popupCue: ({})
    property bool wasPaused: false
    readonly property bool hasPopupCue: visible && popupCue.id !== undefined
    signal dismissRequested(bool resumePausedPlayback)

    // PopupModels.swift: PopupLayout positions the surface in reader coordinates.
    readonly property int gap: 4
    readonly property int borderInset: 6
    readonly property real leftSpace: selectionAnchor.x - gap
    readonly property real anchorRight: selectionAnchor.x + selectionAnchor.width
    readonly property real anchorBottom: selectionAnchor.y + selectionAnchor.height
    readonly property real rightSpace: parent.width - anchorRight - gap
    readonly property real aboveSpace: selectionAnchor.y - gap
    readonly property real belowSpace: parent.height - anchorBottom - gap
    readonly property bool placeRight: rightSpace >= leftSpace || rightSpace >= ReaderViewModel.popupWidth
    readonly property bool placeBelow: belowSpace >= aboveSpace || belowSpace >= ReaderViewModel.popupHeight
    readonly property bool fullWidth: ReaderViewModel.popupFullWidth
    readonly property bool vertical: ReaderViewModel.verticalWriting
    readonly property real insetWidth: parent.width - 2 * borderInset
    readonly property real insetHeight: parent.height - 2 * borderInset
    property bool swipeDismissed: false

    function showResults(html: string): void {
        // NativeReaderView.handleSelection pauses only once results exist.
        if (player.playing)
            wasPaused = player.pauseForLookup()
        results.loadingPopupDocument = true
        results.loadHtml(html, "about:blank")
    }

    // NativeReaderView.handleSelection: the selection's offset picks the cue of
    // the reader's current chapter before the popup is built.
    function captureCue(): void {
        popupCue = ReaderViewModel.lookupOffset >= 0
            ? player.findPopupCue(ReaderViewModel.chapterIndex, ReaderViewModel.lookupOffset)
            : ({})
    }

    // NativeReaderView.closePopup(resumePausedPlayback:): only a recorded pause
    // resumes, and every close forgets it.
    function dismiss(resumePausedPlayback: bool): void {
        const recorded = wasPaused
        wasPaused = false
        if (resumePausedPlayback)
            player.resumeAfterLookup(recorded)
    }

    // The header control clears the remembered pause before toggling.
    function togglePlayback(): void {
        wasPaused = false
        player.togglePlayback()
    }

    // PopupView.sasayakiControls: all three stop word audio first.
    function replayCue(): void {
        if (!hasPopupCue)
            return
        surface.popupModel.stopWordAudio()
        player.playCue(popupCue.id, true)
    }

    function toggleCuePlayback(): void {
        if (!hasPopupCue)
            return
        surface.popupModel.stopWordAudio()
        if (wasPaused)
            wasPaused = false
        else
            player.togglePlayback()
    }

    // NativeReaderView.onSasayakiJumpDismiss and jumpToSasayakiCue: sync the
    // bookmark at the cue's reader progress, play without a stop, and close
    // without resuming.
    function jumpToCue(): void {
        if (!hasPopupCue)
            return
        const cue = popupCue
        surface.popupModel.stopWordAudio()
        if (cue.chapterIndex !== ReaderViewModel.chapterIndex)
            player.prepareTransition()
        ReaderViewModel.syncBookmarkToProgress(cue.chapterIndex, cue.progress)
        player.playCue(cue.id, false)
        surface.dismissRequested(false)
    }

    Connections {
        target: surface.miningModel
        function onReplyReady(requestId, payload) {
            dictionaryBridge.replyReady(requestId, payload)
        }
        function onAnkiNoteButtonRequested(entryIndex, noteId) {
            results.runJavaScript("window.hoshiShowAnkiNoteButton?.(" + entryIndex + ", '" + noteId + "');")
        }
        function onSasayakiAudioRequested(cueId, sentence, format, bitrate) {
            surface.player.requestCueAudio(cueId, sentence, format, bitrate)
        }
    }

    Connections {
        target: surface.player
        function onCueAudioReady(data) {
            surface.miningModel.sasayakiAudioReady(data)
        }
    }

    QtObject {
        id: dictionaryBridge
        WebChannel.id: "dictionaryBridge"
        signal replyReady(int requestId, var payload)

        function playWordAudio(url: string): void {
            surface.popupModel.playWordAudio(url)
        }

        function mineEntry(requestId: int, content: var): void {
            results.lastMiningRequest = Math.max(results.lastMiningRequest, requestId)
            // PopupView.mineEntry: the selection's cue goes with the mine, and
            // an empty id means this lookup has no cue audio.
            surface.miningModel.setSasayakiCue(surface.popupCue.id !== undefined ? surface.popupCue.id : "")
            surface.miningModel.mineEntry(requestId, content)
        }

        function duplicateCheck(requestId: int, word: var): void {
            results.lastMiningRequest = Math.max(results.lastMiningRequest, requestId)
            surface.miningModel.duplicateCheck(requestId, word)
        }

        function openAnkiNote(noteIds: var): void {
            surface.miningModel.openAnkiNote(noteIds)
        }

        function miningFeedback(result: var): void {
            surface.miningModel.miningFeedback(result)
        }

        // PopupWebView.Coordinator's inline lookup handlers; textSelected
        // replaces the document through popupModel.htmlChanged.
        function getEntries(requestId: int, data: var): void {
            dictionaryBridge.replyReady(requestId,
                                        surface.popupModel.getEntries(data.start, data.count))
        }

        function lookupRedirect(requestId: int, query: var): void {
            dictionaryBridge.replyReady(requestId, surface.popupModel.lookupRedirect(String(query)))
        }

        function queryTextSelected(requestId: int, data: var): void {
            dictionaryBridge.replyReady(requestId,
                                        surface.popupModel.queryTextSelected(String(data.text)))
        }

        function textSelected(data: var): void {
            surface.popupModel.textSelected(String(data.text))
        }
    }

    width: Math.max(1, fullWidth ? insetWidth : Math.min(ReaderViewModel.popupWidth, vertical
                    ? Math.min(parent.width, Math.max(leftSpace, rightSpace) - borderInset) : insetWidth))
    height: Math.max(1, Math.min(ReaderViewModel.popupHeight, insetHeight,
                     vertical || fullWidth ? insetHeight : (placeBelow ? belowSpace : aboveSpace) - borderInset))
    x: fullWidth ? Math.max(0, (parent.width - width) / 2)
       : vertical ? Math.max(0, Math.min(parent.width - width, placeRight
                             ? anchorRight + gap : selectionAnchor.x - gap - width))
       : Math.max(0, Math.min(parent.width - width, Math.max(borderInset,
                             Math.min(parent.width - width - borderInset, selectionAnchor.x))))
    y: Math.max(0, Math.min(parent.height - height, Math.max(borderInset,
                Math.min(parent.height - height - borderInset, fullWidth
                         ? parent.height : vertical ? selectionAnchor.y : placeBelow
                         ? anchorBottom + gap : selectionAnchor.y - gap - height))))
    opacity: ReaderViewModel.popupDisableTransparency ? 1.0 : 0.95

    // PopupView.sasayakiControls: replay, play/pause and jump, above the
    // action bar, only while the popup holds a cue and the player has audio.
    Rectangle {
        id: sasayakiBar

        anchors.top: parent.top
        width: parent.width
        height: visible ? 36 : 0
        visible: surface.hasPopupCue && surface.player.hasAudio
        color: Theme.popupBackground

        RowLayout {
            anchors.centerIn: parent
            spacing: 20

            ToolButton {
                text: "\u21bb"
                Accessible.name: qsTr("Replay cue")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: surface.replayCue()
            }

            ToolButton {
                text: surface.player.playing || surface.wasPaused ? "\u275A\u275A" : "\u25B6"
                Accessible.name: surface.player.playing || surface.wasPaused
                                 ? qsTr("Pause Sasayaki") : qsTr("Play Sasayaki")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: surface.toggleCuePlayback()
            }

            ToolButton {
                text: "\u23ED"
                Accessible.name: qsTr("Jump to cue")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: surface.jumpToCue()
            }
        }
    }

    // PopupView.swift places the action bar outside the scaled results.
    Rectangle {
        id: actionBar
        anchors.top: sasayakiBar.bottom
        width: parent.width
        height: visible ? Math.min(parent.height, closeButton.implicitHeight) : 0
        visible: ReaderViewModel.popupActionBar
        color: Theme.popupBackground
        clip: true

        ToolButton {
            id: closeButton
            anchors.right: parent.right
            height: parent.height
            text: "\u00d7"
            Accessible.name: qsTr("Close lookup")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: surface.dismissRequested(true)
        }
    }

    WebEngineView {
        id: results
        property bool loadingPopupDocument: false
        // A new document must not consume outstanding replies for the old one.
        property int lastMiningRequest: 0
        anchors.top: actionBar.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        backgroundColor: Theme.popupBackground
        property real popupScale: ReaderViewModel.popupScale
        onPopupScaleChanged: runJavaScript("document.documentElement.style.zoom = " + popupScale)

        webChannel: WebChannel {
            registeredObjects: [dictionaryBridge]
        }
        onNavigationRequested: function(request) {
            // The generated shell is the only document that may own the bridge.
            const popupDocument = results.loadingPopupDocument && request.isMainFrame
                && request.url.toString().startsWith("data:text/html;charset=UTF-8,")
            if (popupDocument)
                results.loadingPopupDocument = false
            else if (request.url.toString() !== "about:blank")
                request.reject()
        }
        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineView.LoadSucceededStatus && visible) {
                runJavaScript("document.documentElement.style.zoom = " + popupScale
                    + "; window.hoshiMiningRequestBase = " + lastMiningRequest
                    + "; window.hoshiMiningOptions = " + JSON.stringify(surface.miningModel.popupMiningOptions())
                    + ";\n" + surface.popupModel.bridgeScript)
            }
        }

        DragHandler {
            id: swipe
            enabled: surface.visible && ReaderViewModel.popupSwipeToDismiss
            target: null
            xAxis.enabled: false
            onActiveChanged: {
                if (!active) {
                    surface.swipeDismissed = false
                    return
                }
                if (!surface.swipeDismissed &&
                    Math.abs(persistentTranslation.y) > ReaderViewModel.popupSwipeThreshold) {
                    surface.swipeDismissed = true
                    surface.dismissRequested(true)
                }
            }
        }
    }
}
