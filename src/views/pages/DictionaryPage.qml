// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary page: a search field and the popup rendered by the
// Yomitan-derived popup script, following Features/Dictionary/
// DictionarySearchView.swift (performLookup and constructHtml). The model
// builds the shell. Features/Popup/PopupWebView.swift and PopupView.swift
// supply the mining bridge and the toast displayed above the search results.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebChannel
import QtWebEngine
import Niratan

Item {
    id: page

    signal backRequested()

    DictionaryPopupModel {
        id: model
        dark: Theme.dark
        background: Theme.popupBackground
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
    }

    TextMetrics {
        id: titleMetrics
        text: dictionaryHeader.title
        font.pixelSize: 16
        font.weight: Font.DemiBold
    }

    Column {
        anchors.fill: parent
        spacing: 0

        PageHeader {
            id: dictionaryHeader
            width: parent.width
            title: qsTr("Dictionary")
            onBackRequested: page.backRequested()

            TextField {
                id: search
                // The trailing row sizes to its controls; leave room for the
                // shared header's back target, title and horizontal spacing.
                Layout.fillWidth: true
                Layout.preferredWidth: Math.max(0, dictionaryHeader.width
                    - 40 - Math.ceil(titleMetrics.advanceWidth)
                    - 2 * (Theme.spacingMd + Theme.spacingSm))
                placeholderText: qsTr("Look up a word")
                selectByMouse: true
                onAccepted: model.lookup(search.text)

                Component.onCompleted: search.forceActiveFocus()
            }
        }

        Item {
            width: parent.width
            height: parent.height - dictionaryHeader.height

            WebEngineView {
                id: view
                property bool loadingPopupDocument: false
                // A new document must not consume outstanding replies for the old one.
                property int lastMiningRequest: 0
                anchors.fill: parent
                visible: model.hasResults
                backgroundColor: Theme.popupBackground

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
                background: Rectangle { color: Theme.popupBackground }
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

            Text {
                anchors.centerIn: parent
                visible: !model.hasResults && model.error.length > 0
                width: parent.width - 48
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
    }

    AnkiMiningToast {
        status: mining.toastStatus
        message: mining.toastMessage
        topInset: dictionaryHeader.height
    }
}
