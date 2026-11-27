// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows Features/Popup/PopupView.swift (body's 8 px card with a hairline
// border, popupContent, actionBar, sasayakiControls, mineEntry's sasayakiCue
// and the wasPaused/onPause/onSasayakiJumpDismiss wiring),
// Core/AnkiManager.swift (needsSasayakiAudio), PopupWebView.swift
// (Coordinator.userContentController, updateUIView) and
// NativeMac/NativeReaderView.swift (handleSelection's cue and auto-pause,
// closePopup(resumePausedPlayback:)).
//
// Deliberate Linux differences: the card is the solid PLAN.md surface in both
// popupDisableTransparency states (the setting keeps its stored value but the
// popup is never translucent), the action bar draws no back/forward chevrons
// because the port keeps no nested-popup history (the popup shows only the
// close control), and the Sasayaki jump control draws forward.fill because
// the Phosphor set bundled here has no forward.frame glyph.

pragma ComponentBehavior: Bound

import QtQuick
import QtWebChannel
import QtWebEngine
import Niratan

// PopupView.body: the opaque card of Theme.popupBackground with an 8 px
// radius and a 1 px textPrimary hairline; PopupView.actionBar and
// sasayakiControls(for:includesActionBar:) share one top row above the
// results, each control a 52x32 hit area.
Rectangle {
    id: surface

    color: Theme.popupBackground
    radius: Theme.lookupRadius
    border.width: Theme.hairlineWidth
    border.color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.2)
    clip: true

    required property DictionaryPopupModel popupModel
    required property MiningViewModel miningModel
    required property SasayakiViewModel player
    property rect selectionAnchor
    // The cue captured at selection time and the pause this lookup recorded
    // (NativeReaderPopup.sasayakiCue and NativeReaderModel.wasPaused).
    property var popupCue: ({})
    property bool wasPaused: false
    readonly property bool hasPopupCue: visible && popupCue.id !== undefined
    // PopupView.popupContent's showsActionBar; the port has no history.
    readonly property bool showsActionBar: ReaderViewModel.popupActionBar
    readonly property bool showsSasayakiControls: hasPopupCue && player.hasAudio
    readonly property bool showsControls: showsActionBar || showsSasayakiControls
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
        results.url = PopupDocuments.publish(results, html)
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
        objectName: "dictionaryBridge"
        WebChannel.id: "dictionaryBridge"
        signal replyReady(int requestId, var payload)

        function playWordAudio(url: string): void {
            surface.popupModel.playWordAudio(url)
        }

        function showAudioSourceMenu(data: var): void {
            surface.popupModel.showAudioSourceMenu(data)
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
    // PopupView.body: an opaque card in both popupDisableTransparency states.
    opacity: 1.0

    // PopupView.actionBar and sasayakiControls(for:includesActionBar:): one
    // row above the results; the Sasayaki controls replace the bar and share
    // its row with Close when the action bar is on.
    Item {
        id: controlsRow

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.lookupContentInset
        height: surface.showsControls
                ? Math.min(Theme.lookupControlBarHeight,
                           Math.max(0, surface.height - Theme.lookupContentInsets))
                : 0

        Item {
            id: actionBar
            objectName: "lookupActionBar"
            anchors.fill: parent
            visible: surface.showsActionBar

            LookupControl {
                id: closeButton
                objectName: "lookupCloseButton"
                anchors.right: parent.right
                anchors.rightMargin: Theme.lookupCloseInset
                anchors.verticalCenter: parent.verticalCenter
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Close lookup")
                Accessible.onPressAction: surface.dismissRequested(true)
                symbol: "xmark"
                onActivated: surface.dismissRequested(true)
            }
        }

        Item {
            id: sasayakiControls
            objectName: "lookupSasayakiControls"
            anchors.fill: parent
            visible: surface.showsSasayakiControls

            Row {
                anchors.centerIn: parent
                spacing: Theme.lookupControlSpacing

                LookupControl {
                    objectName: "lookupSasayakiReplay"
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Replay cue")
                    Accessible.onPressAction: surface.replayCue()
                    symbol: "arrow.clockwise"
                    onActivated: surface.replayCue()
                }

                LookupControl {
                    objectName: "lookupSasayakiPlayback"
                    Accessible.role: Accessible.Button
                    Accessible.name: surface.player.playing || surface.wasPaused
                                     ? qsTr("Pause Sasayaki") : qsTr("Play Sasayaki")
                    Accessible.onPressAction: surface.toggleCuePlayback()
                    symbol: surface.player.playing || surface.wasPaused
                              ? "pause.fill" : "play.fill"
                    onActivated: surface.toggleCuePlayback()
                }

                LookupControl {
                    objectName: "lookupSasayakiJump"
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Jump to cue")
                    Accessible.onPressAction: surface.jumpToCue()
                    symbol: "forward.fill"
                    onActivated: surface.jumpToCue()
                }
            }
        }
    }

    Rectangle {
        id: controlsDivider

        anchors.top: controlsRow.bottom
        anchors.left: controlsRow.left
        anchors.right: controlsRow.right
        height: surface.showsControls ? Theme.hairlineWidth : 0
        visible: surface.showsControls
        color: Theme.separator
    }

    WebEngineView {
        id: results
        property bool loadingPopupDocument: false
        // A new document must not consume outstanding replies for the old one.
        property int lastMiningRequest: 0
        // Inside the rounded border, so the corners and the stroke show.
        // A cramped popup never gives the page a negative size.
        anchors.top: controlsDivider.bottom
        anchors.left: controlsRow.left
        width: Math.max(0, surface.width - Theme.lookupContentInsets)
        height: Math.max(0, surface.height - y - Theme.lookupContentInset)
        backgroundColor: Theme.popupBackground
        property real popupScale: ReaderViewModel.popupScale
        onPopupScaleChanged: runJavaScript("document.documentElement.style.zoom = " + popupScale)

        webChannel: WebChannel {
            registeredObjects: [dictionaryBridge]
        }
        onNavigationRequested: function(request) {
            // The generated shell is the only document that may own the bridge.
            const popupDocument = results.loadingPopupDocument && request.isMainFrame
                && PopupDocuments.isDocument(results, request.url)
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

    AudioSourceMenu {
        popupModel: surface.popupModel
        webView: results
    }

    // One lookup control: a 52x32 hit area with a secondary 16 pt glyph.
    component LookupControl: Item {
        id: control

        property string symbol

        signal activated()

        width: Theme.lookupControlWidth
        height: Theme.lookupControlHeight

        Icon {
            anchors.centerIn: parent
            name: control.symbol
            size: Theme.lookupControlGlyph
            color: Theme.textSecondary
        }

        TapHandler {
            onTapped: control.activated()
        }
    }
}
