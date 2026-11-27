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
// the current height clamped by the screen, the width is that height times the
// ratio plus the visible study sidebar, and a live resize keeps
// (width - sidebar) / height on the ratio with one frozen driver for the drag.
// Maximized, full screen and a window with no video keep their geometry. The Linux title
// frame is the desktop's and is outside the content size (Decision).
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
    readonly property real studySidebarWidth: player.studySidebarLayoutWidth
    // The frame at the start of the current live resize, the frozen sidebar
    // width and the frozen driver (VideoWindowChromeController.LiveResizeSession).
    property size layoutReference: Qt.size(width, height)
    property bool applyingLayout: false
    property bool resizing: false
    property size resizeReference: Qt.size(width, height)
    property real resizeSidebarWidth: 0
    property string resizeDriver: ""

    onMinimumHeightChanged: {
        if (videoWindow.height < videoWindow.minimumHeight)
            videoWindow.height = videoWindow.minimumHeight
    }

    onVideoAspectRatioChanged: Qt.callLater(videoWindow.applyVideoAspectFit)
    onStudySidebarWidthChanged: {
        if (!videoWindow.resizing)
            Qt.callLater(videoWindow.applyVideoAspectFit)
    }
    onWidthChanged: videoWindow.handleWindowResize()
    onHeightChanged: videoWindow.handleWindowResize()

    onVisibleChanged: {
        if (videoWindow.visible && !videoWindow.placed) {
            videoWindow.placed = true
            Qt.callLater(videoWindow.applyDefaultGeometry)
        }
    }

    // The available desktop area for this window's screen, as the fit and the
    // live-resize clamp use it.
    function availableArea() {
        const target = videoWindow.ownerScreen || videoWindow.screen
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
    // Only a normal window follows the ratio. Maximized and full screen
    // belong to the window manager: resizing them back to the ratio leaves a
    // maximized window at the wrong size, and the video letterboxes instead.
    function followsVideoAspect() {
        return videoWindow.visibility === Window.Windowed
    }

    function fitVideoWindow(ratio, sidebarWidth) {
        if (!(ratio > 0) || !isFinite(ratio) || !videoWindow.followsVideoAspect())
            return
        const area = videoWindow.availableArea()
        if (!area)
            return
        const fitted = VideoWindowAspect.fittedContentSize(
                           Qt.size(videoWindow.width, videoWindow.height), ratio,
                           Math.max(0, sidebarWidth), Qt.size(area.width, area.height))
        const targetHeight = Math.max(Theme.videoWindowMinimumHeight, Math.round(fitted.height))
        const targetWidth = Math.max(Theme.videoWindowMinimumWidth,
                                     Math.round(targetHeight * ratio + Math.max(0, sidebarWidth)))
        videoWindow.applyingLayout = true
        videoWindow.width = Math.min(targetWidth, area.width)
        videoWindow.height = Math.min(targetHeight, area.height)
        videoWindow.x = Math.round(Math.max(area.x,
            Math.min(videoWindow.x, area.x + area.width - videoWindow.width)))
        videoWindow.y = Math.round(Math.max(area.y,
            Math.min(videoWindow.y, area.y + area.height - videoWindow.height)))
        videoWindow.layoutReference = Qt.size(videoWindow.width, videoWindow.height)
        videoWindow.applyingLayout = false
    }

    function applyVideoAspectFit() {
        videoWindow.fitVideoWindow(videoWindow.videoAspectRatio, videoWindow.studySidebarWidth)
    }

    // VideoWindowChromeController.constrainedFrameSize: one driver decided
    // when the drag starts and reused until the resize settles, so a corner
    // drag cannot alternate between width and height between events.
    function handleWindowResize() {
        if (videoWindow.applyingLayout || !videoWindow.followsVideoAspect())
            return
        const ratio = videoWindow.videoAspectRatio
        if (!(ratio > 0) || !isFinite(ratio))
            return
        if (!videoWindow.resizing) {
            videoWindow.resizing = true
            videoWindow.resizeReference = videoWindow.layoutReference
            videoWindow.resizeSidebarWidth = videoWindow.studySidebarWidth
            videoWindow.resizeDriver = ""
        }
        resizeSettle.restart()
        const proposed = Qt.size(videoWindow.width, videoWindow.height)
        if (videoWindow.resizeDriver === "") {
            videoWindow.resizeDriver = VideoWindowAspect.resizeDriver(
                        videoWindow.resizeReference, proposed, ratio)
        }
        if (videoWindow.resizeDriver === "")
            return
        const constrained = VideoWindowAspect.constrainedFrameSize(
                                videoWindow.resizeReference, proposed, Qt.size(0, 0), ratio,
                                videoWindow.resizeSidebarWidth,
                                Qt.size(Theme.videoWindowMinimumWidth,
                                        Theme.videoWindowMinimumHeight),
                                videoWindow.resizeDriver)
        videoWindow.applyingLayout = true
        videoWindow.width = Math.round(constrained.width)
        videoWindow.height = Math.round(constrained.height)
        videoWindow.applyingLayout = false
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
        videoWindow.applyingLayout = true
        videoWindow.width = Math.round(1132 * scale)
        videoWindow.height = Math.round(637 * scale)
        videoWindow.x = Math.round(area.x + (area.width - videoWindow.width) / 2)
        videoWindow.y = Math.round(area.y + (area.height - videoWindow.height) / 2)
        videoWindow.layoutReference = Qt.size(videoWindow.width, videoWindow.height)
        videoWindow.applyingLayout = false
        if (videoWindow.videoAspectRatio > 0)
            Qt.callLater(videoWindow.applyVideoAspectFit)
    }

    Component.onCompleted: Qt.callLater(applyDefaultGeometry)

    // VideoWindowChromeController.endLiveResize: the driver and the reference
    // frame are released once the drag has been quiet for a moment.
    Timer {
        id: resizeSettle

        interval: 300
        onTriggered: {
            videoWindow.resizing = false
            videoWindow.resizeDriver = ""
            videoWindow.layoutReference = Qt.size(videoWindow.width, videoWindow.height)
        }
    }

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
    }
}
