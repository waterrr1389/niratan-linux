// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import QtWebChannel
import QtWebEngine
import Niratan

// The web view reports through the bridge; page turning and progress polling
// run here because the model never touches QtWebEngine.
Item {
    id: page
    focus: true

    signal backRequested()
    readonly property bool readerWindowActive: Window.window ? Window.window.active : true
    readonly property bool statisticsContextActive: visible && readerWindowActive
        && (statisticsPanel.visible || (!toc.visible && !sasayakiPanel.opened))

    function dismissLookup() {
        lookupModel.clear()
        web.runJavaScript("window.hoshiSelection?.clearSelection()")
    }

    function createHighlight(color) {
        const url = ReaderViewModel.chapterUrl
        const script = ReaderViewModel.highlightScript(color)
        if (script.length)
            web.runJavaScript(script, result => ReaderViewModel.addHighlight(url, color, result))
    }

    function turnPage(forward) {
        const script = ReaderViewModel.pageTurnScript(forward)
        if (script.length === 0)
            return
        if (lookupModel.hasResults)
            dismissLookup()
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
        ReaderViewModel.setReaderActive(statisticsContextActive)
        ReaderViewModel.setDark(ThemeState.systemDark)
    }
    onStatisticsContextActiveChanged: ReaderViewModel.setReaderActive(statisticsContextActive)

    Connections {
        target: ThemeState
        function onSystemDarkChanged() {
            ReaderViewModel.setDark(ThemeState.systemDark)
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: page.statisticsContextActive && ReaderViewModel.tracking
        onTriggered: ReaderViewModel.tickTracking()
    }

    Keys.onPressed: function (event) {
        var forward = false
        var handled = true
        if (event.key === Qt.Key_Right || event.key === Qt.Key_Down
                || event.key === Qt.Key_Space || event.key === Qt.Key_PageDown) {
            forward = true
        } else if (event.key === Qt.Key_Left || event.key === Qt.Key_Up
                   || event.key === Qt.Key_PageUp) {
            forward = false
        } else if (event.key === Qt.Key_T && event.modifiers === Qt.NoModifier
                   && ReaderViewModel.statisticsEnabled
                   && !statisticsPanel.visible && !toc.visible && !sasayakiPanel.opened) {
            ReaderViewModel.toggleTracking()
            event.accepted = true
            return
        } else if (event.key === Qt.Key_Escape) {
            if (lookupModel.hasResults) {
                dismissLookup()
                event.accepted = true
                return
            }
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
        function onLookupRequested(text, anchor) {
            lookupSurface.selectionAnchor = anchor
            lookupModel.lookup(text)
        }
        function onReaderTappedOutside() {
            if (lookupModel.hasResults)
                page.dismissLookup()
        }
        function onChapterUrlChanged() {
            lookupModel.clear()
        }
    }

    DictionaryPopupModel {
        id: lookupModel
        dark: Theme.dark
        background: Theme.popupBackground
        textColor: Theme.popupText

        onHtmlChanged: {
            if (hasResults)
                lookupSurface.loadHtml(html, "about:blank")
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

    GridLayout {
        anchors.fill: parent
        columns: 1
        rowSpacing: 0
        PageHeader {
            Layout.row: 0
            Layout.column: 0
            title: ReaderViewModel.hasBook ? ReaderViewModel.chapterTitle
                                          : qsTr("No book open")
            subtitle: ReaderViewModel.hasBook && ReaderViewModel.showTitle ? ReaderViewModel.title : ""
            subtitleColor: ReaderViewModel.usesCustomInfoColor ? ReaderViewModel.infoColor : Theme.textSecondary
            onBackRequested: {
                ReaderViewModel.setReaderActive(false)
                page.backRequested()
            }

            ToolButton {
                text: "A-"
                onClicked: ReaderViewModel.fontSize -= 2
            }
            ToolButton {
                text: "A+"
                onClicked: ReaderViewModel.fontSize += 2
            }
            ToolButton {
                text: ReaderViewModel.verticalWriting ? qsTr("Vertical") : qsTr("Horizontal")
                font.pixelSize: 12
                onClicked: ReaderViewModel.verticalWriting = !ReaderViewModel.verticalWriting
            }
            ToolButton {
                text: qsTr("Go to")
                font.pixelSize: 12
                enabled: ReaderViewModel.hasBook
                ToolTip.visible: hovered
                ToolTip.text: text
                onClicked: toc.open()
            }
            BarButton {
                glyph: "▦"
                tip: qsTr("Gallery")
                Accessible.name: tip
                visible: ReaderViewModel.hasBook
                onClicked: {
                    ReaderViewModel.loadGallery()
                    gallery.open()
                }
            }
            ToolButton {
                text: qsTr("Statistics")
                visible: ReaderViewModel.statisticsEnabled
                ToolTip.visible: hovered
                ToolTip.text: text
                onClicked: statisticsPanel.open()
            }
            ReaderChrome { player: sasayaki }
            ToolButton {
                text: qsTr("Sasayaki")
                font.pixelSize: 12
                enabled: ReaderViewModel.hasBook
                onClicked: sasayakiPanel.open()
            }
        }

        Rectangle {
            Layout.row: 2
            Layout.column: 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: ReaderViewModel.hasBook ? ReaderViewModel.pageBackground : Theme.windowBackground

            WebEngineView {
                id: web
                readonly property size viewportSize: Qt.size(Math.round(width), Math.round(height))
                onViewportSizeChanged: ReaderViewModel.setPageSize(viewportSize.width, viewportSize.height)
                anchors.fill: parent
                visible: ReaderViewModel.hasBook
                opacity: ReaderViewModel.restoring ? 0 : 1
                url: ReaderViewModel.chapterUrl
                backgroundColor: ReaderViewModel.pageBackground
                onLoadingChanged: function(request) {
                    if (request.status === WebEngineView.LoadFailedStatus)
                        ReaderViewModel.revealAfterLoadFailure(request.url.toString())
                }
                onContextMenuRequested: function(request) {
                    if (request.selectedText.length > 0) {
                        request.accepted = true
                        selectionMenu.popup()
                    }
                }
                webChannel: WebChannel {
                    id: channel
                    registeredObjects: [readerBridge]
                }
            }

            Menu {
                id: selectionMenu
                MenuItem {
                    text: qsTr("Copy")
                    onTriggered: web.triggerWebAction(WebEngineView.Copy)
                }
                Menu {
                    title: qsTr("Highlights")
                    MenuItem { text: qsTr("Yellow"); onTriggered: page.createHighlight("yellow") }
                    MenuItem { text: qsTr("Green"); onTriggered: page.createHighlight("green") }
                    MenuItem { text: qsTr("Blue"); onTriggered: page.createHighlight("blue") }
                    MenuItem { text: qsTr("Pink"); onTriggered: page.createHighlight("pink") }
                    MenuItem { text: qsTr("Purple"); onTriggered: page.createHighlight("purple") }
                }
            }

            WebEngineView {
                id: lookupSurface
                z: 1
                visible: ReaderViewModel.hasBook && lookupModel.hasResults
                backgroundColor: Theme.popupBackground
                property rect selectionAnchor
                readonly property int gap: 4
                readonly property int borderInset: 6
                readonly property real leftSpace: selectionAnchor.x - gap
                readonly property real anchorRight: selectionAnchor.x + selectionAnchor.width
                readonly property real anchorBottom: selectionAnchor.y + selectionAnchor.height
                readonly property real rightSpace: web.width - anchorRight - gap
                readonly property real aboveSpace: selectionAnchor.y - gap
                readonly property real belowSpace: web.height - anchorBottom - gap
                readonly property bool placeRight: rightSpace >= leftSpace || rightSpace >= 320
                readonly property bool placeBelow: belowSpace >= aboveSpace || belowSpace >= 250

                width: Math.max(1, Math.min(320, ReaderViewModel.verticalWriting
                                ? Math.max(leftSpace, rightSpace) - borderInset
                                : web.width - 2 * borderInset))
                height: Math.max(1, Math.min(250, ReaderViewModel.verticalWriting
                                 ? web.height - 2 * borderInset
                                 : (placeBelow ? belowSpace : aboveSpace) - borderInset))
                x: ReaderViewModel.verticalWriting
                   ? Math.max(0, Math.min(web.width - width, placeRight
                              ? anchorRight + gap : selectionAnchor.x - gap - width))
                   : Math.max(borderInset, Math.min(web.width - width - borderInset, selectionAnchor.x))
                y: Math.max(borderInset, Math.min(web.height - height - borderInset,
                            ReaderViewModel.verticalWriting ? selectionAnchor.y : placeBelow
                            ? anchorBottom + gap : selectionAnchor.y - gap - height))

                onLoadingChanged: function(loadRequest) {
                    if (loadRequest.status === WebEngineView.LoadSucceededStatus && visible)
                        runJavaScript("window.renderPopup();")
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

        ReaderProgress {
            id: readerProgress
            Layout.row: ReaderViewModel.showProgressTop ? 1 : 3
            Layout.column: 0
            Layout.fillWidth: true
        }
    }

    ReaderStatisticsInspector {
        id: statisticsPanel
    }

    Drawer {
        id: toc
        edge: Qt.LeftEdge
        width: Math.min(380, page.width * 0.65)
        height: page.height
        modal: true
        padding: 0
        onAboutToShow: navigationTabs.currentIndex = 0
        onOpened: {
            if (chaptersList.visible && chaptersList.currentIndex >= 0)
                chaptersList.positionViewAtIndex(chaptersList.currentIndex, ListView.Center)
        }
        onClosed: {
            bookSearch.text = ""
            ReaderViewModel.clearBookSearch()
        }

        background: Rectangle {
            color: Theme.cardBackground
            border.width: 1
            border.color: Theme.cardStroke
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 16
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Go to")
                    color: Theme.textPrimary
                    font.pixelSize: 16
                    font.bold: true
                }
                ToolButton {
                    text: "\u00d7"
                    Accessible.name: qsTr("Close")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Close")
                    onClicked: toc.close()
                }
            }

            TabBar {
                id: navigationTabs
                Layout.fillWidth: true
                TabButton { text: qsTr("Chapters") }
                TabButton { text: qsTr("Search") }
                TabButton { text: qsTr("Highlights") }
            }

            ReaderContents {
                id: chaptersList
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: navigationTabs.currentIndex === 0
                onChapterSelected: toc.close()
            }

            ColumnLayout {
                visible: navigationTabs.currentIndex === 1
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: Theme.spacingSm
                    TextField {
                        id: bookSearch
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search in this book")
                        onAccepted: ReaderViewModel.searchBook(text)
                        onTextChanged: {
                            if (!ReaderViewModel.hasSearchableText(text))
                                ReaderViewModel.clearBookSearch()
                        }
                    }
                    ToolButton {
                        text: qsTr("Search")
                        enabled: ReaderViewModel.hasSearchableText(bookSearch.text)
                                 && ReaderViewModel.searchState !== 1
                        ToolTip.visible: hovered
                        ToolTip.text: text
                        onClicked: ReaderViewModel.searchBook(bookSearch.text)
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    BusyIndicator {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: searchStatus.top
                        running: ReaderViewModel.searchState === 1
                    }
                    Label {
                        id: searchStatus
                        anchors.centerIn: parent
                        visible: ReaderViewModel.searchResults.length === 0
                        color: Theme.textSecondary
                        text: {
                            switch (ReaderViewModel.searchState) {
                            case 1: return qsTr("Searching…")
                            case 2: return qsTr("No matches")
                            case 3: return qsTr("Could not search this book")
                            default: return qsTr("Enter text to search this book")
                            }
                        }
                    }
                    ListView {
                        anchors.fill: parent
                        clip: true
                        visible: ReaderViewModel.searchResults.length > 0
                        model: ReaderViewModel.searchResults
                        delegate: Column {
                            id: hitGroup
                            required property var modelData
                            width: ListView.view.width
                            Label {
                                visible: hitGroup.modelData.groupCount > 0
                                padding: Theme.spacingSm
                                color: Theme.textSecondary
                                text: (hitGroup.modelData.label || qsTr("Untitled Chapter"))
                                      + " · " + qsTr("%n result(s)", "", hitGroup.modelData.groupCount)
                            }
                            ItemDelegate {
                                width: hitGroup.width
                                contentItem: Label {
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 3
                                    textFormat: Text.RichText
                                    text: hitGroup.modelData.before
                                          + "<span style=\"background-color:" + Theme.accent + "\">"
                                          + hitGroup.modelData.match + "</span>" + hitGroup.modelData.after
                                    color: Theme.textPrimary
                                }
                                onClicked: {
                                    ReaderViewModel.jumpToCharacter(hitGroup.modelData.character)
                                    toc.close()
                                }
                            }
                        }
                    }
                }
            }

            Item {
                visible: navigationTabs.currentIndex === 2
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label {
                    anchors.centerIn: parent
                    visible: ReaderViewModel.highlights.length === 0
                    text: qsTr("No Highlights")
                    color: Theme.textSecondary
                }
                ListView {
                    anchors.fill: parent
                    clip: true
                    model: ReaderViewModel.highlights
                    section.property: "label"
                    section.delegate: Label {
                        required property string section
                        text: section
                        padding: Theme.spacingSm
                        color: Theme.textSecondary
                    }
                    delegate: ItemDelegate {
                        id: highlightRow
                        required property var modelData
                        width: ListView.view.width
                        contentItem: Label {
                            text: highlightRow.modelData.text
                            textFormat: Text.PlainText
                            color: Theme.textPrimary
                            wrapMode: Text.Wrap
                            maximumLineCount: 4
                        }
                        onClicked: {
                            ReaderViewModel.jumpToCharacter(modelData.character)
                            toc.close()
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: highlightMenu.popup()
                        }
                        Menu {
                            id: highlightMenu
                            MenuItem {
                                text: qsTr("Delete")
                                onTriggered: ReaderViewModel.removeHighlight(highlightRow.modelData.id)
                            }
                        }
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

    ReaderGallery {
        id: gallery
        readerPage: page
    }
}
