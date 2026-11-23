// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader window's page. The chapter web view fills the whole content
// rectangle on the reader page colour, and the open tools float above it
// following NativeMac/NativeReaderView.swift (NativeReaderView.body,
// NativeReaderLoader.body): onTapOutside and focus mode, the side-panel
// dismiss layer, nativeTopInfoOverlay, nativeBottomInfoOverlay,
// nativeBottomControls, toggleSidePanel and updateReaderContentCoverage, the
// .gallery overlay with GalleryView.swift, the Jump to alert of
// Features/Reader/Search/ReaderGoToView.swift, and the Sasayaki inspector of
// Features/Sasayaki/SasayakiSheet.swift.
//
// The page owns the single active tool: Go to opens on the leading edge, the
// Appearance, Statistics and Sasayaki inspectors on the trailing edge, and the
// Gallery covers everything above the panels. Opening another tool replaces
// the open one; pressing the same Go to, Appearance or Statistics button again
// closes it, while Sasayaki always opens its inspector. A click outside an open
// side panel closes only the panel, so focus mode stays off. Escape closes a
// side panel even while the chapter has focus; in the gallery it first closes
// the preview and then the gallery.
//
// Swift's ReaderLyricsModeView, the history back/forward capsules, cloud
// sync-on-open and the Fushi conflict sheet have no Linux backing and are not
// drawn. The web view keeps the live chapter, restore revision, selection and
// highlight menu, page turns, lookup anchoring and cue callbacks.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import QtWebChannel
import QtWebEngine
import Niratan

Item {
    id: page
    focus: true

    signal backRequested()

    // The one open tool: "", "goTo", "appearance", "statistics", "sasayaki" or
    // "gallery".
    property string activeTool: ""
    property bool focusMode: false
    property SettingsViewModel appearanceSettingsModel

    readonly property bool readerWindowActive: Window.window ? Window.window.active : true
    readonly property bool leadingPanelRequested: activeTool === "goTo"
    readonly property bool trailingPanelRequested: activeTool === "appearance"
                                                   || activeTool === "statistics"
                                                   || activeTool === "sasayaki"
    readonly property bool sidePanelOpen: leadingPanelRequested || trailingPanelRequested
    // updateReaderContentCoverage: every tool but Statistics covers the reader.
    readonly property bool statisticsContextActive: visible && readerWindowActive
        && (activeTool === "" || activeTool === "statistics")
    readonly property bool fontSizeShortcutsEnabled: visible && ReaderViewModel.hasBook
        && appearanceSettingsModel !== null && !lookupHasFocus()

    function lookupHasFocus(): bool {
        let item = page.Window.activeFocusItem
        while (item) {
            if (item === lookupSurface)
                return true
            item = item.parent
        }
        return false
    }

    function refreshAppearanceSettings() {
        const previous = appearanceSettingsModel
        appearanceSettingsModel = settingsComponent.createObject(page)
        if (previous)
            previous.destroy()
    }

    function dismissLookup(resumePausedPlayback) {
        lookupSurface.dismiss(resumePausedPlayback !== false)
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
    // does; anything else is dropped rather than evaluated. The display of the
    // restored cue follows through restoreCompleted and activeCueChanged.
    function applySasayakiCues() {
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
        web.runJavaScript("window.hoshiReader.applySasayakiCues(" + cues + ");")
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

    function openTool(tool) {
        if (tool === "gallery") {
            ReaderViewModel.loadGallery()
            activeTool = "gallery"
            return
        }
        if (tool === "sasayaki") {
            activeTool = "sasayaki"
            return
        }
        activeTool = activeTool === tool ? "" : tool
    }

    function closeTool() {
        activeTool = ""
    }

    Component.onCompleted: {
        ReaderViewModel.setReaderActive(statisticsContextActive)
        ReaderViewModel.setDark(ThemeState.systemDark)
        if (!appearanceSettingsModel)
            refreshAppearanceSettings()
    }
    onStatisticsContextActiveChanged: ReaderViewModel.setReaderActive(statisticsContextActive)
    onReaderWindowActiveChanged: {
        if (readerWindowActive) {
            sasayakiSettings.reload()
            // Settings may have changed in the main window since the last visit.
            refreshAppearanceSettings()
        }
    }

    // NativeReaderEscapeDismissal: Escape closes an open side panel even while
    // the web view holds focus, and does not close the reader.
    Shortcut {
        sequence: "Esc"
        context: Qt.WindowShortcut
        enabled: page.sidePanelOpen
        onActivated: page.closeTool()
    }

    Shortcut {
        sequences: ["Ctrl++", "Ctrl+="]
        enabled: page.fontSizeShortcutsEnabled
        onActivated: page.appearanceSettingsModel.stepFontSize(true)
    }

    Shortcut {
        sequence: "Ctrl+-"
        enabled: page.fontSizeShortcutsEnabled
        onActivated: page.appearanceSettingsModel.stepFontSize(false)
    }

    Shortcut {
        sequence: "Ctrl+0"
        enabled: page.fontSizeShortcutsEnabled
        onActivated: {}
    }

    Component {
        id: settingsComponent
        SettingsViewModel {}
    }

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
        if (sasayakiShortcuts.handleKey(event))
            return
        var forward = false
        var handled = true
        if (event.key === Qt.Key_Right || event.key === Qt.Key_Down
                || event.key === Qt.Key_Space || event.key === Qt.Key_PageDown) {
            forward = true
        } else if (event.key === Qt.Key_Left || event.key === Qt.Key_Up
                   || event.key === Qt.Key_PageUp) {
            forward = false
        } else if (event.key === Qt.Key_T && event.modifiers === Qt.NoModifier
                   && ReaderViewModel.statisticsEnabled && !page.sidePanelOpen
                   && page.activeTool === "") {
            ReaderViewModel.toggleTracking()
            event.accepted = true
            return
        } else if (event.key === Qt.Key_Escape) {
            if (lookupModel.hasResults) {
                dismissLookup()
            } else if (page.activeTool === "gallery") {
                gallery.handleEscape()
            } else if (page.sidePanelOpen) {
                page.closeTool()
            } else {
                ReaderViewModel.setReaderActive(false)
                backRequested()
            }
            event.accepted = true
            return
        } else {
            handled = false
        }
        if (handled && !page.sidePanelOpen && page.activeTool === "") {
            turnPage(forward)
            event.accepted = true
        } else if (handled) {
            event.accepted = true
        }
    }

    Connections {
        target: ReaderViewModel
        function onPageTurnRequested(direction) {
            page.turnPage(direction === "forward")
        }
        function onDocumentRestored(revealCue) {
            page.applySasayakiCues()
            sasayaki.restoreCompleted(ReaderViewModel.chapterIndex)
        }
        function onChapterTransitionStarted() {
            sasayaki.prepareTransition()
        }
        function onLookupRequested(text, sentence, anchor) {
            lookupSurface.selectionAnchor = anchor
            lookupSurface.captureCue()
            mining.setContext(sentence, ReaderViewModel.documentTitle, ReaderViewModel.coverUrl, ReaderViewModel.coverPath)
            lookupModel.lookup(text)
        }
        function onReaderTappedOutside() {
            if (lookupModel.hasResults)
                page.dismissLookup()
            else
                page.focusMode = !page.focusMode
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
                lookupSurface.showResults(html)
        }
    }

    // PopupView.swift topToast belongs to the owning page, not the lookup frame.
    MiningViewModel {
        id: mining
    }

    AnkiMiningToast {
        status: mining.toastStatus
        message: mining.toastMessage
    }

    SasayakiViewModel {
        id: sasayaki
    }

    SasayakiSettingsViewModel {
        id: sasayakiSettings
    }

    SasayakiShortcuts {
        id: sasayakiShortcuts
        surface: lookupSurface
        active: sasayakiSettings.enable && page.activeTool === ""
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
        function onActiveCueChanged(cueId, reveal) {
            page.highlightSasayakiCue(cueId, reveal)
        }
        function onMatchApplied() {
            page.applySasayakiCues()
        }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open EPUB")
        nameFilters: [qsTr("EPUB books (*.epub)")]
        onAccepted: ReaderViewModel.importAndOpen(selectedFile)
    }

    Rectangle {
        anchors.fill: parent
        color: ReaderViewModel.hasBook ? ReaderViewModel.pageBackground
                                       : Theme.windowBackground
    }

    WebEngineView {
        id: web
        objectName: "readerWeb"
        readonly property size viewportSize: Qt.size(Math.round(width), Math.round(height))
        onViewportSizeChanged: ReaderViewModel.setPageSize(viewportSize.width, viewportSize.height)
        anchors.fill: parent
        visible: ReaderViewModel.hasBook
        opacity: ReaderViewModel.restoring ? 0 : 1
        url: ReaderViewModel.chapterUrl
        backgroundColor: ReaderViewModel.pageBackground
        zoomFactor: 1.0
        onZoomFactorChanged: {
            if (zoomFactor !== 1.0)
                zoomFactor = 1.0
        }
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

    // Handle modified wheel input before it reaches Chromium. The lookup
    // surface is separate and layered above the chapter and this handler.
    Item {
        anchors.fill: web
        visible: web.visible

        WheelHandler {
            enabled: page.appearanceSettingsModel !== null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedModifiers: Qt.ControlModifier
            target: null
            onWheel: event => page.appearanceSettingsModel.adjustFontSizeFromWheel(event.angleDelta.y)
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

    ReaderLookupSurface {
        id: lookupSurface
        objectName: "readerLookupSurface"
        popupModel: lookupModel
        miningModel: mining
        player: sasayaki
        z: 1
        visible: ReaderViewModel.hasBook && lookupModel.hasResults
        onDismissRequested: resume => page.dismissLookup(resume)
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

    // isSidePanelOpen: a click on the page closes the panel and never reaches
    // the chapter. It sits below the bottom controls and the panels, so their
    // controls stay reachable.
    MouseArea {
        anchors.fill: parent
        visible: page.sidePanelOpen
        onClicked: page.closeTool()
    }

    ReaderTopInfo {
        visible: !page.focusMode && (ReaderViewModel.showTitle
                                     || (ReaderViewModel.showProgressTop
                                         && ReaderViewModel.progressText.length > 0))
    }

    ReaderBottomControls {
        anchors.fill: parent
        visible: !page.focusMode
        player: sasayaki
        sasayakiEnabled: sasayakiSettings.enable
        pausedByLookup: lookupSurface.wasPaused
        onToolRequested: tool => page.openTool(tool)
        onSasayakiToggleRequested: lookupSurface.togglePlayback()
    }

    ReaderSidePanel {
        id: leadingPanel
        objectName: "readerLeadingPanel"
        edge: Qt.LeftEdge
        width: 440
        height: page.height - 24
        y: 12
        x: page.leadingPanelRequested ? 12 : -452
        opacity: page.leadingPanelRequested ? 1 : 0
        visible: page.leadingPanelRequested || opacity > 0

        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 220 } }

        Loader {
            anchors.fill: parent
            active: page.leadingPanelRequested
            sourceComponent: goToComponent
        }
    }

    ReaderSidePanel {
        id: trailingPanel
        objectName: "readerTrailingPanel"
        edge: Qt.RightEdge
        width: 440
        height: page.height - 24
        y: 12
        x: page.trailingPanelRequested ? page.width - 12 - 440 : page.width + 12
        opacity: page.trailingPanelRequested ? 1 : 0
        visible: page.trailingPanelRequested || opacity > 0

        Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 220 } }

        Loader {
            anchors.fill: parent
            active: page.trailingPanelRequested
            sourceComponent: page.activeTool === "appearance" ? appearanceComponent
                           : page.activeTool === "statistics" ? statisticsComponent
                           : sasayakiComponent
        }
    }

    ReaderGallery {
        id: gallery
        anchors.fill: parent
        opened: page.activeTool === "gallery"
        onCloseRequested: page.closeTool()
    }

    component GoToPanel: ReaderGoToPanel {
        anchors.fill: parent
        onCloseRequested: page.closeTool()
    }

    component AppearancePanel: ReaderAppearancePanel {
        anchors.fill: parent
        settingsModel: page.appearanceSettingsModel
        onCloseRequested: page.closeTool()
    }

    component StatisticsPanel: ReaderStatisticsInspector {
        anchors.fill: parent
        onCloseRequested: page.closeTool()
    }

    component SasayakiInspector: SasayakiPanel {
        anchors.fill: parent
        opened: page.activeTool === "sasayaki"
        player: sasayaki
        onCloseRequested: page.closeTool()
    }

    Component { id: goToComponent; GoToPanel {} }
    Component { id: appearanceComponent; AppearancePanel {} }
    Component { id: statisticsComponent; StatisticsPanel {} }
    Component { id: sasayakiComponent; SasayakiInspector {} }
}
