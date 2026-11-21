// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary page: a search field and the popup rendered by the
// Yomitan-derived popup script, following Features/Dictionary/
// DictionarySearchView.swift (performLookup and constructHtml). The model
// builds the shell; this page only shows it and asks the script to render
// after the document loads, as PopupWebView does.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
                anchors.fill: parent
                visible: model.hasResults
                backgroundColor: Theme.popupBackground

                onLoadingChanged: function(loadRequest) {
                    if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                        runJavaScript("window.renderPopup();")
                    }
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
                    if (model.html.length > 0) {
                        view.loadHtml(model.html, "about:blank")
                    }
                }
            }
        }
    }
}
