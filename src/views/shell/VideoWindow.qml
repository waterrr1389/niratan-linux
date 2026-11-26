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
// Inspector restores the 120 minimum without shrinking the window. The Linux
// title bar and window-manager
// behaviour are the desktop's; the transparent titlebar, traffic-light fading
// and aspect-locked live resizing are not ported (Decisions).
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

    onMinimumHeightChanged: {
        if (videoWindow.height < videoWindow.minimumHeight)
            videoWindow.height = videoWindow.minimumHeight
    }

    onVisibleChanged: {
        if (videoWindow.visible && !videoWindow.placed) {
            videoWindow.placed = true
            Qt.callLater(videoWindow.applyDefaultGeometry)
        }
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
        videoWindow.width = Math.round(1132 * scale)
        videoWindow.height = Math.round(637 * scale)
        videoWindow.x = Math.round(area.x + (area.width - videoWindow.width) / 2)
        videoWindow.y = Math.round(area.y + (area.height - videoWindow.height) / 2)
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
    }
}
