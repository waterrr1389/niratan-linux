// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtWebChannel
import QtWebEngine
import Niratan

// The reading surface: one chapter of the open book in a web view, with the
// chapter list, the reading controls and the progress footer. The web view
// only reports through the bridge; page turning and progress polling run here
// because the model never touches QtWebEngine.
Item {
    id: page
    focus: true

    signal backRequested()

    function turnPage(forward) {
        const script = ReaderViewModel.pageTurnScript(forward)
        if (script.length === 0)
            return
        const url = ReaderViewModel.chapterUrl
        web.runJavaScript(script, function (result) {
            if (url !== ReaderViewModel.chapterUrl)
                return
            ReaderViewModel.handlePageTurnResult(forward, result)
            if (url === ReaderViewModel.chapterUrl)
                syncProgress(url)
        })
    }

    function syncProgress(url) {
        web.runJavaScript("window.hoshiReader?.calculateProgress()",
                          function (result) {
                              if (url === ReaderViewModel.chapterUrl && typeof result === "number")
                                  ReaderViewModel.updateChapterProgress(result)
                          })
    }

    // Re-applies the current chapter's cue ranges. The model's JSON is an
    // array literal, so it is inserted as an expression like the Swift reader
    // does; anything else is dropped rather than evaluated.
    function applySasayakiCues(reveal) {
        if (!ReaderViewModel.hasBook || !sasayaki.hasMatch)
            return
        var cues = sasayaki.applyCuesJson()
        if (cues.length === 0)
            return
        try {
            JSON.parse(cues)
        } catch (error) {
            return
        }
        const url = ReaderViewModel.chapterUrl
        web.runJavaScript("window.hoshiReader.applySasayakiCues(" + cues + ");", function () {
            if (url === ReaderViewModel.chapterUrl)
                page.highlightSasayakiCue(sasayaki.currentCueId, reveal)
        })
    }

    function highlightSasayakiCue(cueId, reveal) {
        if (!ReaderViewModel.hasBook || !sasayaki.hasMatch)
            return
        if (cueId && cueId.length > 0) {
            const url = ReaderViewModel.chapterUrl
            web.runJavaScript("window.hoshiReader.highlightSasayakiCue("
                              + JSON.stringify(cueId) + ", " + JSON.stringify(reveal) + ")", function (result) {
                                  if (url === ReaderViewModel.chapterUrl && typeof result === "number")
                                      ReaderViewModel.updateChapterProgress(result)
                              })
        } else {
            web.runJavaScript("window.hoshiReader.clearSasayakiCue()")
        }
    }

    Component.onCompleted: {
        ReaderViewModel.setPageSize(Math.round(width), Math.round(height))
        ReaderViewModel.setReaderActive(true)
    }
    onWidthChanged: ReaderViewModel.setPageSize(Math.round(width), Math.round(height))
    onHeightChanged: ReaderViewModel.setPageSize(Math.round(width), Math.round(height))
    // A hidden reader page pauses reading time; becoming visible resumes it.
    onVisibleChanged: ReaderViewModel.setReaderActive(visible)

    Keys.onPressed: function (event) {
        var forward = false
        var handled = true
        if (event.key === Qt.Key_Right || event.key === Qt.Key_Down
                || event.key === Qt.Key_Space || event.key === Qt.Key_PageDown) {
            forward = true
        } else if (event.key === Qt.Key_Left || event.key === Qt.Key_Up
                   || event.key === Qt.Key_PageUp) {
            forward = false
        } else if (event.key === Qt.Key_Escape) {
            ReaderViewModel.setReaderActive(false)
            backRequested()
            event.accepted = true
            return
        } else {
            handled = false
        }
        if (handled) {
            turnPage(forward)
            event.accepted = true
        }
    }

    Connections {
        target: ReaderViewModel
        function onPageTurnRequested(direction) {
            page.turnPage(direction === "forward")
        }
        function onDocumentRestored(revealCue) {
            page.applySasayakiCues(revealCue)
        }
    }

    SasayakiViewModel {
        id: sasayaki
    }

    QtObject {
        id: readerBridge
        WebChannel.id: "readerBridge"

        function message(name: string, payload: string): void {
            ReaderViewModel.message(name, payload)
        }
    }

    Connections {
        target: sasayaki
        function onActiveCueChanged(cueId) {
            page.highlightSasayakiCue(cueId, true)
        }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open EPUB")
        nameFilters: [qsTr("EPUB books (*.epub)")]
        onAccepted: ReaderViewModel.importAndOpen(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        PageHeader {
            title: ReaderViewModel.hasBook ? ReaderViewModel.chapterTitle
                                          : qsTr("No book open")
            subtitle: ReaderViewModel.hasBook ? ReaderViewModel.title : ""
            onBackRequested: {
                ReaderViewModel.setReaderActive(false)
                page.backRequested()
            }

            ToolButton {
                text: "A-"
                onClicked: ReaderViewModel.fontSize = Math.max(10, ReaderViewModel.fontSize - 2)
            }
            ToolButton {
                text: "A+"
                onClicked: ReaderViewModel.fontSize = Math.min(72, ReaderViewModel.fontSize + 2)
            }
            ToolButton {
                text: ReaderViewModel.verticalWriting ? qsTr("Vertical") : qsTr("Horizontal")
                font.pixelSize: 12
                onClicked: ReaderViewModel.verticalWriting = !ReaderViewModel.verticalWriting
            }
            ToolButton {
                text: qsTr("Contents")
                font.pixelSize: 12
                enabled: ReaderViewModel.hasBook
                onClicked: toc.open()
            }
            ToolButton {
                text: qsTr("Sasayaki")
                font.pixelSize: 12
                enabled: ReaderViewModel.hasBook
                onClicked: sasayakiPanel.open()
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            WebEngineView {
                id: web
                anchors.fill: parent
                visible: ReaderViewModel.hasBook
                url: ReaderViewModel.chapterUrl
                backgroundColor: Theme.windowBackground
                webChannel: WebChannel {
                    id: channel
                    registeredObjects: [readerBridge]
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: !ReaderViewModel.hasBook
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Open an EPUB to start reading")
                    color: Theme.textSecondary
                }
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Open EPUB")
                    onClicked: openDialog.open()
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    visible: ReaderViewModel.error.length > 0
                    text: ReaderViewModel.error
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }

        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: Theme.cardBackground
            border.width: 1
            border.color: Theme.cardStroke
            visible: ReaderViewModel.hasBook

            ProgressBar {
                anchors.fill: parent
                anchors.margins: 6
                from: 0
                to: 1
                value: ReaderViewModel.progress
            }

            Label {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: ReaderViewModel.showsPageCount
                      ? (ReaderViewModel.pageCount > 0
                         ? qsTr("%1 / %2").arg(ReaderViewModel.pageIndex).arg(ReaderViewModel.chapterPageCount)
                         : "")
                      : Math.round(ReaderViewModel.progress * 100) + "%"
                color: Theme.textSecondary
                font.pixelSize: 11
            }
        }
    }

    Drawer {
        id: toc
        edge: Qt.RightEdge
        width: Math.min(380, page.width * 0.65)
        height: page.height
        modal: true
        padding: 0

        background: Rectangle {
            color: Theme.cardBackground
            border.width: 1
            border.color: Theme.cardStroke
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Label {
                Layout.fillWidth: true
                Layout.margins: 16
                text: qsTr("Contents")
                color: Theme.textPrimary
                font.pixelSize: 16
                font.bold: true
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: ReaderViewModel.chapters
                currentIndex: ReaderViewModel.chapters.findIndex(
                                  row => row.index === ReaderViewModel.chapterIndex)
                delegate: ItemDelegate {
                    id: chapterItem
                    required property var modelData
                    width: ListView.view.width
                    leftPadding: padding + 12 * chapterItem.modelData.level
                    text: chapterItem.modelData.title
                    highlighted: chapterItem.modelData.index === ReaderViewModel.chapterIndex
                    onClicked: {
                        ReaderViewModel.goToContentsEntry(chapterItem.modelData.index,
                                                          chapterItem.modelData.fragment)
                        toc.close()
                    }
                }
            }
        }
    }

    SasayakiPanel {
        id: sasayakiPanel
        anchors.fill: parent
        player: sasayaki
    }
}
