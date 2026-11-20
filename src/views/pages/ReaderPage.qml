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
        if (!ReaderViewModel.hasBook)
            return
        web.runJavaScript(ReaderViewModel.pageTurnScript(forward), function (result) {
            ReaderViewModel.handlePageTurnResult(forward, result)
            syncProgress()
        })
    }

    function syncProgress() {
        if (!ReaderViewModel.hasBook)
            return
        web.runJavaScript("window.hoshiReader ? window.hoshiReader.calculateProgress() : 0",
                          function (result) {
                              if (typeof result === "number")
                                  ReaderViewModel.updateChapterProgress(result)
                          })
    }

    Component.onCompleted: ReaderViewModel.setPageSize(Math.round(width), Math.round(height))
    onWidthChanged: ReaderViewModel.setPageSize(Math.round(width), Math.round(height))
    onHeightChanged: ReaderViewModel.setPageSize(Math.round(width), Math.round(height))

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
        function onChapterUrlChanged() {
            chapter.visible = false
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

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.cardBackground
            border.width: 1
            border.color: Theme.cardStroke

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                ToolButton {
                    text: "\u2039"
                    font.pixelSize: 22
                    onClicked: page.backRequested()
                }

                Label {
                    Layout.fillWidth: true
                    text: ReaderViewModel.hasBook ? ReaderViewModel.chapterTitle
                                              : qsTr("No book open")
                    elide: Text.ElideRight
                    color: Theme.textPrimary
                    font.pixelSize: 15
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
                    registeredObjects: [ReaderViewModel]
                }
                onLoadingChanged: function (request) {
                    if (request.status === WebEngineView.LoadSucceededStatus)
                        page.syncProgress()
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

            Label {
                id: chapter
                anchors.centerIn: parent
                text: qsTr("Restoring position\u2026")
                color: Theme.textSecondary
                visible: false
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
                text: Math.round(ReaderViewModel.progress * 100) + "%"
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
                currentIndex: ReaderViewModel.chapterIndex
                delegate: ItemDelegate {
                    id: chapterItem
                    required property int index
                    required property var modelData
                    width: ListView.view.width
                    text: chapterItem.modelData.title
                    highlighted: chapterItem.index === ReaderViewModel.chapterIndex
                    onClicked: {
                        ReaderViewModel.goToChapter(chapterItem.index)
                        toc.close()
                    }
                }
            }
        }
    }
}
