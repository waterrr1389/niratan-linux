// SPDX-License-Identifier: GPL-3.0-or-later
//
// The floating bar's progress strip, following
// Features/Video/VideoControlsView.swift `progressControlStrip`,
// `timelineProgressControl`, `remainingTimeText` and `displayedProgress`, and
// `VideoTimeFormatter.string(from:)` in
// Features/Video/Playback/PlaybackEngine.swift.
//
// Current time at the leading edge, the timeline in the middle and the
// remaining time at the trailing edge. The active time is the scrub time while
// scrubbing, so both labels move with the drag; the timeline seeks only on
// release and keeps the chapter ticks. The remaining label always carries the
// "-" prefix, so an unknown duration reads "-0:00".
//
// Object names: currentTimeLabel, timeline, remainingTimeLabel for the strip's
// three columns in that order; the timeline draws chapterMarker delegates, a
// scrubBubble while dragging, a timelinePreviewBubble for the hover preview
// and a hoverKnob while the pointer is over it. `showLabels` is false in the
// compact layout, which draws the bare timeline above its control row.
//
// The hover preview follows VideoControlsView.swift `handleProgressHover`,
// `handleProgressExit`, `updateProgressPreview`,
// `scheduleProgressPreviewHide` and `activeTimelinePreview`: the capsule sits
// above the pointer, clamped inside the track, and shows the hovered time.
// It follows a scrub and lingers for eight seconds when a scrub ends with the
// pointer away; a plain hover exit clears at once, as in Swift.
// `timelinePreviewActive` keeps the page's chrome visible. No preview appears
// while the duration is 0.
pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: strip

    objectName: "videoProgressStrip"

    property real currentTime: 0
    property real duration: 0
    property bool scrubbing: false
    property real scrubTime: 0
    property bool emphasized: false
    property bool showLabels: true
    // Drawn on the compact layout's dark scrim: the track, fill, ticks and
    // knob use the overlay foreground so they read over the video in both
    // themes.
    property bool onOverlay: false
    property var chapters: []

    readonly property real activeTime: strip.scrubbing ? strip.scrubTime : strip.currentTime
    readonly property real progress: strip.duration > 0
        ? Math.max(0, Math.min(1, strip.activeTime / strip.duration)) : 0
    readonly property string currentText: strip.formatTime(strip.activeTime)
    readonly property string remainingText: "-" + strip.formatTime(
                                                Math.max(strip.duration - strip.activeTime, 0))
    readonly property real timelineWidth: timeline.width
    // VideoControlsView.activeTimelinePreview: the capsule shows while the
    // pointer preview is active or a scrub runs, and the page keeps its chrome
    // visible for as long as either holds.
    readonly property bool timelinePreviewActive: strip.previewActive || strip.scrubbing
    property bool previewActive: false
    property bool previewHovering: false
    property real previewTime: 0
    property real previewX: 0
    // VideoTimelineChapterMarkers.markerTimes: chapter starts inside the clip,
    // close starts collapsed to one tick.
    readonly property var markerTimes: {
        if (!strip.chapters || strip.duration <= 0)
            return []
        const sorted = strip.chapters.slice().sort(function (left, right) {
            return left.start - right.start
        })
        const times = []
        let last = -Infinity
        for (const chapter of sorted) {
            const start = Number(chapter.start)
            if (!isFinite(start) || start <= 0 || start >= strip.duration)
                continue
            if (start - last < 0.05)
                continue
            times.push(start)
            last = start
        }
        return times
    }

    signal seekRequested(real seconds)
    signal scrubStarted(real seconds)
    signal scrubUpdated(real seconds)
    signal scrubFinished(real seconds)

    implicitHeight: Theme.timelineHitHeight

    function formatTime(seconds) {
        if (!isFinite(seconds) || seconds <= 0)
            return "0:00"
        const total = Math.floor(seconds)
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const remainder = total % 60
        const secondsText = remainder < 10 ? "0" + remainder : String(remainder)
        if (hours > 0) {
            const minutesText = minutes < 10 ? "0" + minutes : String(minutes)
            return hours + ":" + minutesText + ":" + secondsText
        }
        return minutes + ":" + secondsText
    }

    function timeAt(x) {
        if (strip.duration <= 0 || timeline.width <= 0)
            return 0
        return Math.max(0, Math.min(strip.duration, strip.duration * x / timeline.width))
    }

    onDurationChanged: {
        if (strip.duration <= 0)
            strip.clearTimelinePreview()
    }

    // VideoControlsView.handleProgressHover: a hover moves the preview and
    // cancels any pending hide.
    function handleProgressHover(localX) {
        if (strip.duration <= 0 || timeline.width <= 0)
            return
        previewHideTimer.stop()
        strip.previewHovering = true
        strip.previewX = Math.max(0, Math.min(timeline.width, localX))
        strip.updateProgressPreview(strip.timeAt(strip.previewX))
    }

    // VideoControlsView.handleProgressExit: a scrub keeps the preview; an
    // already scheduled hide keeps running; otherwise the capsule clears.
    function handleProgressExit() {
        strip.previewHovering = false
        if (strip.scrubbing)
            return
        if (previewHideTimer.running)
            return
        strip.previewActive = false
        strip.previewTime = 0
    }

    // VideoControlsView.updateProgressPreview.
    function updateProgressPreview(seconds) {
        if (strip.duration <= 0 || timeline.width <= 0)
            return
        previewHideTimer.stop()
        strip.previewTime = Math.max(0, Math.min(strip.duration, seconds))
        strip.previewX = timeline.width * strip.previewTime / strip.duration
        strip.previewActive = true
    }

    // VideoControlsView.scheduleProgressPreviewHide: eight seconds after a
    // scrub ends with the pointer away, unless hover or a scrub resumes.
    function scheduleProgressPreviewHide() {
        previewHideTimer.restart()
    }

    // The page clears the preview when the clip changes or the chrome hides.
    function clearTimelinePreview() {
        previewHideTimer.stop()
        strip.previewActive = false
        strip.previewHovering = false
        strip.previewTime = 0
    }

    // Positions are explicit rather than Row-managed so a caller can map a
    // timeline point before the first polish pass.
    Item {
        id: stripRow

        anchors.fill: parent

        readonly property real spacing: strip.showLabels ? Theme.chromeClusterSpacing : 0

        StyledLabel {
            id: currentTimeLabel

            objectName: "currentTimeLabel"
            visible: strip.showLabels
            anchors.verticalCenter: parent.verticalCenter
            x: 0
            width: strip.showLabels ? Theme.timelineLabelWidth : 0
            text: strip.currentText
            textStyle: Theme.captionFont
            secondary: true
            monospacedDigits: true
            horizontalAlignment: Text.AlignLeft
        }

        Item {
            id: timeline

            objectName: "timeline"
            anchors.verticalCenter: parent.verticalCenter
            x: currentTimeLabel.width + stripRow.spacing
            width: Math.max(0, stripRow.width - stripRow.spacing * (strip.showLabels ? 2 : 0)
                            - currentTimeLabel.width - remainingTimeLabel.width)
            height: Theme.timelineHitHeight

            readonly property int trackHeight: strip.emphasized || timelineMouse.containsMouse
                                               || strip.scrubbing ? Theme.timelineEmphasizedHeight
                                                                  : Theme.timelineHeight

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: timeline.trackHeight
                radius: Math.floor(height / 2)
                color: strip.onOverlay ? Theme.chromeOverlayTrack : Theme.trackBackground
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                width: timeline.width * strip.progress
                height: timeline.trackHeight
                radius: Math.floor(height / 2)
                color: strip.onOverlay ? Theme.overlayForeground : Theme.textPrimary
                opacity: Theme.trackSliderFillOpacity
            }

            Repeater {
                model: strip.markerTimes

                delegate: Rectangle {
                    required property var modelData

                    objectName: "chapterMarker"
                    x: Math.max(0, Math.min(timeline.width - width,
                                            timeline.width * modelData
                                            / Math.max(1, strip.duration) - width / 2))
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.timelineMarkerWidth
                    height: Theme.timelineMarkerHeight
                    radius: width / 2
                    color: strip.onOverlay ? Theme.overlayForeground : Theme.textSecondary
                }
            }

            Rectangle {
                id: hoverKnob

                objectName: "hoverKnob"
                x: Math.max(0, Math.min(timeline.width - width,
                                        timeline.width * strip.progress - width / 2))
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.timelineKnob
                height: Theme.timelineKnob
                radius: width / 2
                color: strip.onOverlay ? Theme.overlayForeground : Theme.textPrimary
                visible: timelineMouse.containsMouse || strip.scrubbing
            }

            StyledLabel {
                id: scrubBubble

                objectName: "scrubBubble"
                x: Math.max(0, Math.min(timeline.width - width,
                                        timeline.width * strip.progress - width / 2))
                y: -height - Theme.spacingXs
                height: Theme.chipHeight
                leftPadding: Theme.chipPaddingHorizontal
                rightPadding: Theme.chipPaddingHorizontal
                verticalAlignment: Text.AlignVCenter
                visible: strip.scrubbing
                text: strip.formatTime(strip.scrubTime)
                textStyle: Theme.captionFont
                weight: Theme.weightSemibold
                monospacedDigits: true

                background: Rectangle {
                    radius: Theme.chipRadius
                    color: Theme.floatingBackground
                    border.width: Theme.cardBorderWidth
                    border.color: Theme.floatingStroke
                }
            }

            StyledLabel {
                id: timelinePreviewBubble

                objectName: "timelinePreviewBubble"
                x: Math.max(0, Math.min(timeline.width - width,
                                        strip.previewX - width / 2))
                y: -height - Theme.spacingXs
                height: Theme.chipHeight
                leftPadding: Theme.chipPaddingHorizontal
                rightPadding: Theme.chipPaddingHorizontal
                verticalAlignment: Text.AlignVCenter
                visible: strip.duration > 0 && strip.previewActive && !strip.scrubbing
                text: strip.formatTime(strip.previewTime)
                textStyle: Theme.captionFont
                weight: Theme.weightSemibold
                monospacedDigits: true

                background: Rectangle {
                    radius: Theme.chipRadius
                    color: Theme.floatingBackground
                    border.width: Theme.cardBorderWidth
                    border.color: Theme.floatingStroke
                }
            }

            MouseArea {
                id: timelineMouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onPressed: function (mouse) {
                    if (strip.duration > 0) {
                        strip.updateProgressPreview(strip.timeAt(mouse.x))
                        strip.scrubStarted(strip.timeAt(mouse.x))
                    }
                }
                onPositionChanged: function (mouse) {
                    if (strip.duration <= 0)
                        return
                    if (timelineMouse.pressed) {
                        strip.updateProgressPreview(strip.timeAt(mouse.x))
                        strip.scrubUpdated(strip.timeAt(mouse.x))
                    } else {
                        strip.handleProgressHover(mouse.x)
                    }
                }
                onExited: strip.handleProgressExit()
                onReleased: function (mouse) {
                    if (strip.duration <= 0)
                        return
                    strip.updateProgressPreview(strip.timeAt(mouse.x))
                    strip.seekRequested(strip.timeAt(mouse.x))
                    strip.scrubFinished(strip.timeAt(mouse.x))
                    if (!timelineMouse.containsMouse)
                        strip.scheduleProgressPreviewHide()
                }
            }
        }

        StyledLabel {
            id: remainingTimeLabel

            objectName: "remainingTimeLabel"
            visible: strip.showLabels
            anchors.verticalCenter: parent.verticalCenter
            x: timeline.x + timeline.width + stripRow.spacing
            width: strip.showLabels ? Theme.timelineLabelWidth : 0
            text: strip.remainingText
            textStyle: Theme.captionFont
            secondary: true
            monospacedDigits: true
            horizontalAlignment: Text.AlignRight
        }
    }

    Timer {
        id: previewHideTimer

        objectName: "timelinePreviewHideTimer"
        interval: 8000
        onTriggered: {
            if (strip.scrubbing || strip.previewHovering)
                return
            strip.previewActive = false
            strip.previewTime = 0
        }
    }
}
