// SPDX-License-Identifier: GPL-3.0-or-later
//
// The audio source menu of Features/Popup/PopupWebView.swift
// (showAudioSourceMenu, audioSourceSelected): a right click on a word's audio
// button lists the recordings the page found, the playing one checked, and
// choosing one plays it through popup.js playEntryAudio. PopupWebView serves
// the reader, the video player and the dictionary page alike, so every lookup
// surface hosts this menu over its popup document.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtWebEngine
import Niratan

Menu {
    id: audioMenu

    objectName: "audioSourceMenu"

    required property DictionaryPopupModel popupModel
    required property WebEngineView webView

    property list<string> sourceNames
    property int selectedSource: -1
    property int entryIndex: -1

    parent: webView

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
            onTriggered: audioMenu.popupModel.selectAudioSource(audioMenu.entryIndex, index)
        }
        onObjectAdded: (index, object) => audioMenu.insertItem(index + 1, object)
        onObjectRemoved: (index, object) => audioMenu.removeItem(object)
    }

    Connections {
        target: audioMenu.popupModel

        function onHtmlChanged() {
            audioMenu.close()
        }

        // The page reports CSS pixels; the view's zoom maps them to items. A
        // request from a hidden or reloading document is stale.
        function onAudioSourceMenuRequested(names, selected, x, y, entryIndex) {
            if (!audioMenu.webView.visible || audioMenu.webView.loading)
                return
            audioMenu.sourceNames = names
            audioMenu.selectedSource = selected
            audioMenu.entryIndex = entryIndex
            audioMenu.popup(audioMenu.webView, Qt.point(x * audioMenu.webView.zoomFactor,
                                                     y * audioMenu.webView.zoomFactor))
        }

        function onAudioSourceSelected(entryIndex, sourceIndex) {
            audioMenu.webView.runJavaScript("playEntryAudio(" + entryIndex + ", " + sourceIndex + ");")
        }
    }

    // A menu stays open when only its parent hides, as when the lookup closes
    // without clearing its document.
    Connections {
        target: audioMenu.webView

        function onVisibleChanged() {
            if (!audioMenu.webView.visible)
                audioMenu.close()
        }
    }
}
