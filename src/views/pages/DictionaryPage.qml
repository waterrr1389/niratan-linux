// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary detail page: one capsule search bar at the top and the popup
// document below it, following Features/Dictionary/DictionarySearchView.swift
// `body` (the `safeAreaInset` search bar above `PopupWebView`),
// `DictionarySearchBar.body`, `CustomSearchField.body` and
// Features/Dictionary/DictionarySearchTextFieldBridge.swift (a borderless,
// transparent, single-line field), through
// NativeMac/NativeReuseViews.swift `NativeDictionaryReuseView.body` inside
// NativeMac/NativeMacDetailView.swift `body`.
//
// The page has no title band, back control or toolbar: the section is chosen
// in the shell sidebar and the bar is the page's first content, spanning the
// width above the results. Return submits once with the text as typed; the
// model trims it, so an empty or whitespace-only query clears the results.
//
// The page owns the shell's toolbar row, so the shell leaves no 52 px strip
// above it. A Theme.spacingSm gap above the bar stands in for the macOS window
// toolbar band that Swift's top safe-area inset sits below (Decision D1); the
// results surface and the mining toast start at the bar's bottom. While the
// main sidebar is collapsed the window passes `toolbarInset` 44 and the bar
// starts at that x, leaving the floating sidebar toggle clear of the capsule
// and field.
// `NativeGlassPageBackground` and `NativeGlassTopScrim` stay the shell's solid
// window background (PLAN.md keeps glass a solid surface), and
// `SearchResetInset` (pull down to clear or show the keyboard) is not ported:
// WebEngine reports no scroll-offset or drag callbacks.
//
// The mining toast is a Linux addition (Decision D2): Swift's `onMine` awaits
// `mineAnkiEntry` and drops the result, so the Mac page shows no feedback.
//
// Not ported: child popups (`nestedPopups`, `NativeDictionaryPopupView`), the
// swipe back/forward gesture, `initialQuery`/`shouldFocus` (the Linux app has
// no `hoshi://` open route) and the dictionary entry shortcuts (no global
// shortcut manager outside the reader).

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtWebChannel
import QtWebEngine
import Niratan

Item {
    id: page

    // Room the window asks for at the leading edge, where its sidebar toggle
    // floats while the main sidebar is collapsed.
    property real toolbarInset: 0

    DictionaryPopupModel {
        id: model
        objectName: "popupModel"
        page: true
        dark: Theme.dark
        background: Theme.windowBackground
        textColor: Theme.popupText
    }

    MiningViewModel {
        id: mining
        onReplyReady: (requestId, payload) => dictionaryBridge.replyReady(requestId, payload)
        onAnkiNoteButtonRequested: (entryIndex, noteId) => {
            view.runJavaScript("window.hoshiShowAnkiNoteButton?.(" + entryIndex + ", '" + noteId + "');")
        }
    }

    QtObject {
        id: dictionaryBridge
        WebChannel.id: "dictionaryBridge"
        signal replyReady(int requestId, var payload)

        function playWordAudio(url: string): void {
            model.playWordAudio(url)
        }

        function showAudioSourceMenu(data: var): void {
            model.showAudioSourceMenu(data)
        }

        function mineEntry(requestId: int, content: var): void {
            view.lastMiningRequest = Math.max(view.lastMiningRequest, requestId)
            mining.mineEntry(requestId, content)
        }

        function duplicateCheck(requestId: int, word: var): void {
            view.lastMiningRequest = Math.max(view.lastMiningRequest, requestId)
            mining.duplicateCheck(requestId, word)
        }

        function openAnkiNote(noteIds: var): void {
            mining.openAnkiNote(noteIds)
        }

        function miningFeedback(result: var): void {
            mining.miningFeedback(result)
        }

        // PopupWebView.Coordinator's inline lookup handlers: the page's own
        // links, selections and entry fetches answer from the popup model.
        function getEntries(requestId: int, data: var): void {
            dictionaryBridge.replyReady(requestId, model.getEntries(data.start, data.count))
        }

        function lookupRedirect(requestId: int, query: var): void {
            dictionaryBridge.replyReady(requestId, model.lookupRedirect(String(query)))
        }

        function queryTextSelected(requestId: int, data: var): void {
            dictionaryBridge.replyReady(requestId, model.queryTextSelected(String(data.text)))
        }

        function textSelected(data: var): void {
            model.textSelected(String(data.text))
        }
    }

    SearchBar {
        id: searchBar

        objectName: "searchBar"
        x: page.toolbarInset
        y: Theme.spacingSm
        width: page.width - page.toolbarInset
        onSubmitted: model.lookup(searchBar.text)
    }

    Item {
        id: resultsArea

        x: 0
        y: searchBar.y + searchBar.height
        width: page.width
        height: Math.max(0, page.height - y)

        WebEngineView {
            id: view

            objectName: "results"
            property bool loadingPopupDocument: false
            // A new document must not consume outstanding replies for the old one.
            property int lastMiningRequest: 0
            anchors.fill: parent
            visible: model.hasResults
            backgroundColor: Theme.windowBackground

            webChannel: WebChannel {
                registeredObjects: [dictionaryBridge]
            }
            onNavigationRequested: function(request) {
                // loadHtml sends the generated document as a data URL.
                const popupDocument = view.loadingPopupDocument && request.isMainFrame
                    && request.url.toString().startsWith("data:text/html;charset=UTF-8,")
                if (popupDocument)
                    view.loadingPopupDocument = false
                else if (request.url.toString() !== "about:blank")
                    request.reject()
            }
            onLoadingChanged: function(loadRequest) {
                if (loadRequest.status === WebEngineView.LoadSucceededStatus && view.visible) {
                    view.runJavaScript("window.hoshiMiningRequestBase = " + lastMiningRequest
                        + "; window.hoshiMiningOptions = " + JSON.stringify(mining.popupMiningOptions())
                        + ";\n" + model.bridgeScript)
                }
            }
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            padding: Theme.spacingSm
            visible: model.hasResults && model.audioError.length > 0
            text: model.audioError
            color: Theme.popupDanger
            background: Rectangle { color: Theme.windowBackground }
        }

        Menu {
            id: audioMenu
            parent: view
            property list<string> sourceNames
            property int selectedSource: -1
            property int entryIndex: -1

            MenuItem {
                visible: audioMenu.sourceNames.length === 0
                height: visible ? implicitHeight : 0
                enabled: false
                text: qsTr("No audio found")
            }

            Instantiator {
                model: audioMenu.sourceNames
                delegate: MenuItem {
                    id: sourceItem
                    required property string modelData
                    required property int index
                    contentItem: Label {
                        text: sourceItem.text
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                    }
                    text: modelData
                    checkable: true
                    checked: index === audioMenu.selectedSource
                    onTriggered: model.selectAudioSource(audioMenu.entryIndex, index)
                }
                onObjectAdded: (index, object) => audioMenu.insertItem(index + 1, object)
                onObjectRemoved: (index, object) => audioMenu.removeItem(object)
            }
        }

        StyledLabel {
            anchors.centerIn: parent
            visible: !model.hasResults && model.error.length > 0
            width: parent.width - 2 * Theme.spacingXl
            text: model.error
            color: Theme.popupDanger
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Connections {
            target: model

            function onHtmlChanged() {
                audioMenu.close()
                if (model.html.length > 0) {
                    view.loadingPopupDocument = true
                    view.loadHtml(model.html, "about:blank")
                }
            }

            function onAudioSourceMenuRequested(names, selected, x, y, entryIndex) {
                audioMenu.sourceNames = names
                audioMenu.selectedSource = selected
                audioMenu.entryIndex = entryIndex
                audioMenu.popup(view, Qt.point(x * view.zoomFactor, y * view.zoomFactor))
            }

            function onAudioSourceSelected(entryIndex, sourceIndex) {
                view.runJavaScript("playEntryAudio(" + entryIndex + ", " + sourceIndex + ");")
            }
        }
    }

    AnkiMiningToast {
        objectName: "miningToast"
        status: mining.toastStatus
        message: mining.toastMessage
        topInset: searchBar.y + searchBar.height
    }
}
