// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary page: a search field and the popup rendered by the
// Yomitan-derived popup script, following Features/Dictionary/
// DictionarySearchView.swift (performLookup and constructHtml). The model
// builds the shell; this page only shows it and asks the script to render
// after the document loads, as PopupWebView does.

import QtQuick
import QtQuick.Controls
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

    Column {
        anchors.fill: parent
        spacing: 0

        Row {
            width: parent.width
            height: 48
            spacing: 4

            Button {
                width: 40
                height: parent.height
                text: "\u2039"
                onClicked: page.backRequested()
            }

            TextField {
                id: search
                width: parent.width - 44
                height: parent.height
                placeholderText: qsTr("Look up a word")
                selectByMouse: true
                onAccepted: model.lookup(text)

                Component.onCompleted: forceActiveFocus()
            }
        }

        Item {
            width: parent.width
            height: parent.height - search.height

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
