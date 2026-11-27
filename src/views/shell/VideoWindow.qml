// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dedicated Video window, following NativeMac/VideoWindowPresenter.swift
// `VideoWindowGeometry.minimumSize`/`.defaultSize`, `makeWindow`,
// `defaultVideoWindowFrame`, `open`, `windowWillClose`,
// `scheduleWindowTeardown` and `VideoWindowRootView.body`;
// Features/Video/VideoWindowCoordinator.swift `requestOpen`, `sessionID`,
// `windowDidAppear` and `windowDidDisappear`; and
// Features/Video/VideoPlayerScreen.swift `body`, `videoSurface`,
// `lifecycleContent`, the no-video ContentUnavailableView and
// `toggleFullScreen`.
//
// One window is declared in MainWindow and kept hidden, as ReaderWindow does.
// Opening shows or raises it and hands the absolute path, bound subtitle and
// from-beginning flag to PlayerPage; while it is open another open replaces the
// media in the same window. Closing hides it, unloads and resets the player and
// reports `videoClosed` so the library refreshes its playback history; the
// window keeps its place and size for the next open.
//
// Geometry follows Swift: default 1132x637 scaled down on both axes together
// when the screen is smaller than that, centred on the desktop's available
// area, minimum 285x120 and no maximum. While the Inspector is open Linux
// raises the minimum height to 360 and grows a shorter window, keeping its
// fixed controls and a scrolling viewport above the playback bar; closing the
// Inspector restores the 120 minimum without shrinking the window.
//
// In windowed mode the window follows the video aspect ratio, following
// VideoPlayerScreen.videoWindowAspectRatio/synchronizeVideoWindowLayout and
// VideoWindowChromeController.VideoWindowAspectLayout: the video height is
// the current height clamped by the screen, and the width is that height times
// the ratio plus the requested study sidebar width. Linux fits on video load,
// aspect or sidebar requests, and after restoring from maximized or full
// screen. Desktop resizes preserve the frame and letterbox the video.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Niratan

ApplicationWindow {
    id: videoWindow

    objectName: "videoWindow"
    title: qsTr("Video")
    minimumWidth: Theme.videoWindowMinimumWidth
    minimumHeight: player.inspectorOpen ? Theme.videoWindowInspectorMinimumHeight
                                        : Theme.videoWindowMinimumHeight
    visible: false
    color: Theme.videoBackground

    signal videoClosed()

    // The player's session flag, forwarded to the library page so poster
    // generation pauses while this window has a video loaded.
    readonly property bool videoSessionActive: player.videoSessionActive

    // The first show places the frame; later opens keep the user's size and
    // place (VideoWindowPresenter makes the window once).
    property bool placed: false
    // VideoWindowPresenter uses NSApp.keyWindow?.screen; the shell hands in
    // the main window's screen so the frame lands on the same display.
    property var ownerScreen

    // VideoPlayerScreen.videoWindowAspectRatio and the sidebar width the fit
    // reserves.
    readonly property real videoAspectRatio: player.videoAspectRatio
    readonly property real studySidebarWidth: player.studySidebarFitWidth
    property bool fitOnRestore: false

    onMinimumHeightChanged: {
        if (videoWindow.visibility === Window.Windowed
                && videoWindow.height < videoWindow.minimumHeight)
            videoWindow.height = videoWindow.minimumHeight
    }

    onVideoAspectRatioChanged: Qt.callLater(videoWindow.applyVideoAspectFit)
    onStudySidebarWidthChanged: Qt.callLater(videoWindow.applyVideoAspectFit)
    onVisibilityChanged: function(visibility) {
        restoreFit.stop()
        if (visibility === Window.Maximized || visibility === Window.FullScreen) {
            fitOnRestore = true
        } else if (visibility === Window.Windowed && fitOnRestore) {
            fitOnRestore = false
            // Qt can report Windowed before the compositor restores the size.
            restoreFit.start()
        } else if (visibility === Window.Hidden) {
            fitOnRestore = false
        }
    }
    onWidthChanged: {
        if (restoreFit.running)
            restoreFit.restart()
    }
    onHeightChanged: {
        if (restoreFit.running)
            restoreFit.restart()
    }

    Timer {
        id: restoreFit

        interval: 150
        onTriggered: Qt.callLater(videoWindow.applyVideoAspectFit)
    }

    onVisibleChanged: {
        if (videoWindow.visible && !videoWindow.placed) {
            videoWindow.placed = true
            Qt.callLater(videoWindow.applyDefaultGeometry)
        }
    }

    // The window may have moved away from the screen it was opened on.
    function availableArea() {
        const target = videoWindow.screen || videoWindow.ownerScreen
        if (!target)
            return null
        return target.availableGeometry !== undefined
               ? target.availableGeometry
               : Qt.rect(target.virtualX, target.virtualY,
                         target.desktopAvailableWidth, target.desktopAvailableHeight)
    }

    // VideoWindowAspectLayout.fittedContentSize applied to the content area:
    // keep the current height within the screen, derive the width from the
    // ratio plus the visible sidebar, clamp to the screen, keep the top edge
    // and respect the minimum frame.
    function fitVideoWindow(ratio, sidebarWidth) {
        if (!(ratio > 0) || !isFinite(ratio) || videoWindow.visibility !== Window.Windowed)
            return
        const area = videoWindow.availableArea()
        if (!area)
            return
        const fitted = VideoWindowAspect.fittedContentSize(
                           Qt.size(videoWindow.width, videoWindow.height), ratio,
                           Math.max(0, sidebarWidth), Qt.size(area.width, area.height))
        const targetHeight = Math.max(videoWindow.minimumHeight, Math.round(fitted.height))
        const targetWidth = Math.max(videoWindow.minimumWidth,
                                     Math.round(targetHeight * ratio + Math.max(0, sidebarWidth)))
        videoWindow.width = Math.min(targetWidth, area.width)
        videoWindow.height = Math.min(targetHeight, area.height)
        videoWindow.x = Math.round(Math.max(area.x,
            Math.min(videoWindow.x, area.x + area.width - videoWindow.width)))
        videoWindow.y = Math.round(Math.max(area.y,
            Math.min(videoWindow.y, area.y + area.height - videoWindow.height)))
    }

    function applyVideoAspectFit() {
        if (!restoreFit.running)
            videoWindow.fitVideoWindow(videoWindow.videoAspectRatio, videoWindow.studySidebarWidth)
    }

    // VideoWindowPresenter.open: reuse the window for every request.
    function openVideo(url, subtitleUrl, fromBeginning) {
        player.openRequest(url, subtitleUrl, fromBeginning)
        if (videoWindow.visibility === Window.Minimized)
            videoWindow.showNormal()
        videoWindow.show()
        videoWindow.raise()
        videoWindow.requestActivate()
    }

    function reloadVideoSettings() {
        player.reloadVideoSettings()
    }

    // VideoWindowPresenter.defaultVideoWindowFrame: scale both axes together
    // to fit the screen and centre the frame.
    function applyDefaultGeometry() {
        if (videoWindow.visible && videoWindow.visibility !== Window.Windowed)
            return
        const target = videoWindow.ownerScreen || videoWindow.screen
        if (!target) {
            videoWindow.width = 1132
            videoWindow.height = 637
            videoWindow.x = 0
            videoWindow.y = 0
            return
        }
        // QQuickScreenInfo exposes the available area as its desktop-available
        // size for the screen origin; QScreen's availableGeometry is used when
        // the platform hands one over.
        const area = target.availableGeometry !== undefined
                     ? target.availableGeometry
                     : Qt.rect(target.virtualX, target.virtualY,
                               target.desktopAvailableWidth, target.desktopAvailableHeight)
        const scale = Math.min(1, area.width / 1132, area.height / 637)
        videoWindow.width = Math.round(1132 * scale)
        videoWindow.height = Math.round(637 * scale)
        videoWindow.x = Math.round(area.x + (area.width - videoWindow.width) / 2)
        videoWindow.y = Math.round(area.y + (area.height - videoWindow.height) / 2)
        if (videoWindow.videoAspectRatio > 0)
            Qt.callLater(videoWindow.applyVideoAspectFit)
    }

    Component.onCompleted: Qt.callLater(applyDefaultGeometry)

    // windowWillClose: the port keeps the window but unloads the clip, clears
    // its transient state and lets the library refresh its history.
    onClosing: {
        player.unloadVideo()
        videoClosed()
    }

    PlayerPage {
        id: player

        objectName: "videoPlayerPage"
        anchors.fill: parent
        onVideoLoaded: Qt.callLater(videoWindow.applyVideoAspectFit)
    }
}
