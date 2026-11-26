// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video lookup popup surface, following Features/Popup/PopupView.swift
// (popupContent, the action bar, onTapOutside, onSwipeDismiss and the
// drag/close gestures), Features/Popup/PopupModels.swift (PopupLayout:
// popupPadding 4, screenBorderPadding 6, showOnRight, showBelow, width,
// height, position), VideoPlayerScreen.swift (popupView: isVertical false,
// isFullWidth false, bottomInset from VideoControlsMetrics and the mining
// context provider) and VideoControlsView.swift (VideoControlsMetrics:
// popupBottomInset 56 floating, 28 compactBottom).
//
// The WebEngineView, the dictionary bridge and the mining connections are
// the reader surface's (src/views/reader/ReaderLookupSurface.qml); only the
// placement differs: width is min(viewportWidth - 12, popupWidth), the space
// is below or above the cue rectangle by 4 px, showBelow is
// spaceBelow >= spaceAbove || spaceBelow >= popupHeight, the height is
// min(max(0, space - 6), popupHeight), x starts at the cue rectangle's left
// edge clamped to the viewport with a 6 px border, and y starts at the cue's
// bottom when shown below (else its top) and is clamped with bottomInset.
// Scale, transparency, action bar and swipe dismissal come from the reader
// popup settings (Decision D5).
//
// prepareMining runs before each mine so the page sets the video mining
// context on the mining view model first (Decision D4 keeps one popup).

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtWebChannel
import QtWebEngine
import Niratan

// PopupView.body: the opaque card of Theme.popupBackground with an 8 px
// radius and a 1 px textPrimary hairline, as the reader's surface draws it.
Rectangle {
    id: surface

    color: Theme.popupBackground
    radius: Theme.lookupRadius
    border.width: Theme.hairlineWidth
    border.color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.2)

    required property DictionaryPopupModel popupModel
    required property MiningViewModel miningModel
    property rect selectionAnchor
    // Floating control bar 56; compact bottom bar 28.
    property real bottomInset: 56
    // Called before the popup's mine command reaches mineEntry.
    property var prepareMining: null

    signal dismissRequested()

    readonly property int gap: 4
    readonly property int borderInset: 6
    readonly property real viewportWidth: surface.parent ? surface.parent.width : 0
    readonly property real viewportHeight: surface.parent ? surface.parent.height : 0
    readonly property real spaceAbove: surface.selectionAnchor.y - surface.gap
    readonly property real spaceBelow: surface.viewportHeight - surface.bottomInset
        - (surface.selectionAnchor.y + surface.selectionAnchor.height) - surface.gap
    readonly property bool showBelow: surface.spaceBelow >= surface.spaceAbove
        || surface.spaceBelow >= ReaderViewModel.popupHeight
    property bool swipeDismissed: false
    property int lastMiningRequest: 0

    width: Math.max(1, Math.min(surface.viewportWidth - 2 * surface.borderInset,
                                ReaderViewModel.popupWidth))
    height: Math.max(1, Math.min(Math.max(0, (surface.showBelow ? surface.spaceBelow
                                                              : surface.spaceAbove)
                                          - surface.borderInset),
                                 ReaderViewModel.popupHeight))
    x: Math.max(surface.borderInset, Math.min(surface.viewportWidth - surface.width
                                              - surface.borderInset, surface.selectionAnchor.x))
    y: {
        const top = surface.showBelow
            ? surface.selectionAnchor.y + surface.selectionAnchor.height + surface.gap
            : surface.selectionAnchor.y - surface.gap - surface.height
        return Math.max(surface.borderInset,
                        Math.min(surface.viewportHeight - surface.bottomInset
                                 - surface.borderInset - surface.height, top))
    }
    // PopupView.body: an opaque card in both popupDisableTransparency states,
    // as on the reader. The web view does not take an item opacity, so a
    // translucent frame would only tint the ring around it.
    opacity: 1.0

    function showResults(html: string): void {
        results.loadingPopupDocument = true
        results.loadHtml(html, "about:blank")
    }

    Connections {
        target: surface.miningModel
        function onReplyReady(requestId, payload) {
            dictionaryBridge.replyReady(requestId, payload)
        }
        function onAnkiNoteButtonRequested(entryIndex, noteId) {
            results.runJavaScript("window.hoshiShowAnkiNoteButton?.(" + entryIndex + ", '" + noteId + "');")
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
            surface.lastMiningRequest = Math.max(surface.lastMiningRequest, requestId)
            if (surface.prepareMining)
                surface.prepareMining()
            surface.miningModel.mineEntry(requestId, content)
        }

        function duplicateCheck(requestId: int, word: var): void {
            surface.lastMiningRequest = Math.max(surface.lastMiningRequest, requestId)
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

    // PopupView.swift places the action bar outside the scaled results.
    Rectangle {
        id: actionBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.lookupContentInset
        height: visible ? Math.min(closeButton.implicitHeight,
                                   Math.max(0, parent.height - Theme.lookupContentInsets)) : 0
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
            onClicked: surface.dismissRequested()
        }
    }

    WebEngineView {
        id: results
        property bool loadingPopupDocument: false
        // Inside the rounded border, so the corners and the stroke show.
        // A cramped popup never gives the page a negative size.
        anchors.top: actionBar.bottom
        anchors.left: actionBar.left
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
                && request.url.toString().startsWith("data:text/html;charset=UTF-8,")
            if (popupDocument)
                results.loadingPopupDocument = false
            else if (request.url.toString() !== "about:blank")
                request.reject()
        }
        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineView.LoadSucceededStatus && visible) {
                runJavaScript("document.documentElement.style.zoom = " + popupScale
                    + "; window.hoshiMiningRequestBase = " + surface.lastMiningRequest
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
                    surface.dismissRequested()
                }
            }
        }
    }

    AudioSourceMenu {
        popupModel: surface.popupModel
        webView: results
    }
}
