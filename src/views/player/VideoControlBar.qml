// SPDX-License-Identifier: GPL-3.0-or-later
//
// Playback chrome in two layouts, following Features/Video/VideoControlsView.swift:
// `VideoControlsMetrics`, `metrics(for:)`, `chromeSize(for:availableWidth:)`,
// `ControlDensity`, `controlDensity`, `body`, `floatingControls`,
// `compactBottomControls`, `compactBottomScrim`, `primaryControlGroup`,
// `responsivePrimaryControlGroup`, `responsiveCompactControlGroup`,
// `condensedControlGroup`, `minimalControlGroup`, `speedControlButton` and
// `speedControlPanel`, with the visibility rule of
// Features/Video/VideoPlayerScreen.swift `shouldShowPlaybackChrome` and the
// fade of `VideoPlayerScreen+Chrome.swift`.
//
// The bar is one surface with two layouts: "floating" is the bottom-centred
// panel with the progress strip above the control row, "compactBottom" spans
// the stage with the timeline above one row on a black scrim. It is drawn only
// while `videoFileSet` and `chromeShown` hold; while hidden it ignores pointer
// input, so the stage keeps its click and double-click handlers.
//
// Density follows the bar's own width: floating is full at 680 and above,
// condensed from 390 to 679 and minimal below; compact is full at 720 and
// above, condensed from 390 to 719 and minimal below. Crossing into minimal
// closes the speed panel (`speedPanelCloseRequested`).
//
// The floating row is volume leading, the episode group centred, and the speed
// pill with the utility group trailing; the compact full row is the episode
// group, volume, the elapsed/total time, then the pill and tools. Only the
// active row is instantiated, so each control exists once.
//
// The bar owns no playback state: the page supplies the snapshot through the
// properties below and receives intents as signals. Swift's drag-to-move
// surface and the screenshot and gap fast-forward controls are not ported
// (Decisions); `inspectorOpen` marks the Inspector button active.
//
// `density` is "full", "condensed" or "minimal"; `interactive` is true only
// while the bar accepts pointer input; `trackWidth` is the timeline's drawable
// width for mapping a stage x to a time; `barHoverChanged` and
// `speedPanelHoverChanged` suspend the page's auto-hide while the bar or the
// speed panel is hovered. The compact full row's time label is compactTime.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Item {
    id: bar

    objectName: "videoControlBar"

    property string layoutMode: "floating"
    // The stage width; chromeSize() shrinks the floating bar inside it.
    property real availableWidth: 0
    property bool videoFileSet: false
    property bool chromeShown: false
    property bool isPlaying: false
    property bool isPaused: true
    property bool isMuted: false
    property real volume: 100
    property real currentTime: 0
    property real duration: 0
    property real speed: 1
    property bool scrubbing: false
    property real scrubTime: 0
    property bool canMine: false
    property bool studyOpen: false
    property bool isFullScreen: false
    property bool speedPanelOpen: false
    property bool inspectorOpen: false
    property var chapters: []

    readonly property bool compactLayout: bar.layoutMode === "compactBottom"
    readonly property real chromeWidth: bar.compactLayout
        ? Math.max(0, bar.availableWidth)
        : Math.min(840, Math.max(1, bar.availableWidth - 32))
    readonly property string density: {
        const fullThreshold = bar.compactLayout ? 720 : 680
        if (bar.chromeWidth >= fullThreshold)
            return "full"
        if (bar.chromeWidth >= 390)
            return "condensed"
        return "minimal"
    }
    readonly property bool interactive: bar.videoFileSet && bar.chromeShown
    readonly property real activeTime: bar.scrubbing ? bar.scrubTime : bar.currentTime
    readonly property real trackWidth: progress.timelineWidth
    readonly property string compactTimeText: bar.formatTime(bar.activeTime)
        + " / " + bar.formatTime(bar.duration)

    width: chromeWidth
    height: bar.compactLayout ? 88 : 94
    opacity: bar.interactive ? 1 : 0
    visible: bar.interactive || bar.opacity > 0

    signal togglePlaybackRequested()
    signal previousRequested()
    signal nextRequested()
    signal seekRequested(real seconds)
    signal scrubStarted(real seconds)
    signal scrubUpdated(real seconds)
    signal scrubFinished(real seconds)
    signal volumeChangeRequested(real value)
    signal toggleMuteRequested()
    signal speedChangeRequested(real value)
    signal speedPanelToggleRequested()
    signal speedPanelCloseRequested()
    signal toggleStudyRequested()
    signal openVideoRequested()
    signal mineSubtitleRequested()
    signal toggleInspectorRequested()
    signal toggleFullScreenRequested()
    signal barHoverChanged(bool hovered)
    signal speedPanelHoverChanged(bool hovered)

    Behavior on opacity {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    onDensityChanged: {
        if (bar.density === "minimal" && bar.speedPanelOpen)
            bar.speedPanelCloseRequested()
    }

    // VideoTimeFormatter.string(from:).
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

    // VideoPlaybackSpeed.label with the "x" suffix the pill shows.
    function speedLabel(value) {
        const normalized = bar.normalizedSpeed(value)
        if (normalized === 0.25)
            return "0.25x"
        if (normalized === Math.trunc(normalized))
            return String(Math.trunc(normalized)) + "x"
        return normalized.toFixed(1) + "x"
    }

    function normalizedSpeed(value) {
        if (!isFinite(value))
            return 1
        if (value <= 0.25)
            return 0.25
        return Math.min(5, Math.max(0.3, Math.round(value * 10) / 10))
    }

    HoverHandler {
        onHoveredChanged: bar.barHoverChanged(hovered)
    }

    // Swallows clicks only during the fade-out, when the bar is still visible
    // but no longer accepts input.
    MouseArea {
        anchors.fill: parent
        enabled: !bar.interactive
        acceptedButtons: Qt.AllButtons
    }

    Rectangle {
        anchors.fill: parent
        visible: !bar.compactLayout
        radius: 24
        color: Theme.floatingBackground
        border.width: Theme.cardBorderWidth
        border.color: Theme.floatingStroke
    }

    // VideoControlsView.compactBottomScrim: black 0.62 at the bottom fading
    // out 45% of the way up.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.height + 48
        visible: bar.compactLayout
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(0, 0, 0, 0)
            }
            GradientStop {
                position: 0.55
                color: Qt.rgba(0, 0, 0, 0.38)
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(0, 0, 0, 0.62)
            }
        }
    }

    Column {
        id: layoutColumn

        anchors.fill: parent
        anchors.leftMargin: bar.compactLayout ? 22 : 16
        anchors.rightMargin: bar.compactLayout ? 22 : 16
        anchors.topMargin: bar.compactLayout ? 0 : 12
        anchors.bottomMargin: bar.compactLayout ? 12 : 8
        spacing: bar.compactLayout ? 8 : 6

        VideoProgressStrip {
            id: progress

            width: layoutColumn.width
            height: 18
            showLabels: !bar.compactLayout
            currentTime: bar.currentTime
            duration: bar.duration
            scrubbing: bar.scrubbing
            scrubTime: bar.scrubTime
            emphasized: false
            chapters: bar.chapters
            onSeekRequested: function (seconds) {
                bar.seekRequested(seconds)
            }
            onScrubStarted: function (seconds) {
                bar.scrubStarted(seconds)
            }
            onScrubUpdated: function (seconds) {
                bar.scrubUpdated(seconds)
            }
            onScrubFinished: function (seconds) {
                bar.scrubFinished(seconds)
            }
        }

        Loader {
            width: layoutColumn.width
            height: 46
            sourceComponent: bar.compactLayout ? compactRowComponent : floatingRowComponent
        }
    }

    HoverHandler {
        id: barHover
    }

    Component {
        id: floatingRowComponent

        // Three clusters over the same row: volume leading, the episode group,
        // and the speed pill with the tools trailing. As in
        // primaryControlGroup's HStack, the episode group is centred while the
        // outer clusters fit beside it; when the trailing tools need more than
        // half the row, they keep their width and the group moves left.
        Item {
            id: floatingRow

            VideoVolumeControl {
                id: floatingVolume

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: bar.density === "full"
                isMuted: bar.isMuted
                volume: bar.volume
                onToggleMuteRequested: bar.toggleMuteRequested()
                onVolumeChangeRequested: function (value) {
                    bar.volumeChangeRequested(value)
                }
            }

            VideoTransportControls {
                readonly property real leadingEdge: floatingVolume.visible
                    ? floatingVolume.width + 8 : 0
                readonly property real trailingEdge: floatingTrailing.x - 8

                x: bar.density === "condensed" ? 0
                    : Math.max(leadingEdge,
                               Math.min((floatingRow.width - width) / 2, trailingEdge - width))
                anchors.verticalCenter: parent.verticalCenter
                width: implicitWidth
                isPlaying: bar.isPlaying
                isPaused: bar.isPaused
                isMuted: bar.isMuted
                volume: bar.volume
                showVolume: false
                onTogglePlaybackRequested: bar.togglePlaybackRequested()
                onPreviousRequested: bar.previousRequested()
                onNextRequested: bar.nextRequested()
                onToggleMuteRequested: bar.toggleMuteRequested()
                onVolumeChangeRequested: function (value) {
                    bar.volumeChangeRequested(value)
                }
            }

            Row {
                id: floatingTrailing

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                SpeedPill {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: bar.density !== "minimal"
                }

                VideoUtilityControls {
                    anchors.verticalCenter: parent.verticalCenter
                    density: bar.density
                    studyOpen: bar.studyOpen
                    canMine: bar.canMine
                    isFullScreen: bar.isFullScreen
                    inspectorOpen: bar.inspectorOpen
                    onToggleStudyRequested: bar.toggleStudyRequested()
                    onOpenVideoRequested: bar.openVideoRequested()
                    onMineSubtitleRequested: bar.mineSubtitleRequested()
                    onToggleInspectorRequested: bar.toggleInspectorRequested()
                    onToggleFullScreenRequested: bar.toggleFullScreenRequested()
                }
            }
        }
    }

    Component {
        id: compactRowComponent

        RowLayout {
            spacing: 6

            Item {
                visible: bar.density === "minimal"
                Layout.fillWidth: true
            }

            VideoTransportControls {
                isPlaying: bar.isPlaying
                isPaused: bar.isPaused
                isMuted: bar.isMuted
                volume: bar.volume
                volumeLeads: false
                showVolume: false
                onTogglePlaybackRequested: bar.togglePlaybackRequested()
                onPreviousRequested: bar.previousRequested()
                onNextRequested: bar.nextRequested()
                onToggleMuteRequested: bar.toggleMuteRequested()
                onVolumeChangeRequested: function (value) {
                    bar.volumeChangeRequested(value)
                }
            }

            VideoVolumeControl {
                visible: bar.density === "full"
                isMuted: bar.isMuted
                volume: bar.volume
                onToggleMuteRequested: bar.toggleMuteRequested()
                onVolumeChangeRequested: function (value) {
                    bar.volumeChangeRequested(value)
                }
            }

            Label {
                objectName: "compactTime"
                Layout.alignment: Qt.AlignVCenter
                visible: bar.density === "full"
                text: bar.compactTimeText
                color: Theme.overlayForeground
                opacity: 0.92
                font.pixelSize: 12
                font.features: ({ "tnum": 1 })
            }

            Item {
                Layout.fillWidth: true
            }

            SpeedPill {
                visible: bar.density !== "minimal"
            }

            VideoUtilityControls {
                Layout.alignment: Qt.AlignVCenter
                density: bar.density
                studyOpen: bar.studyOpen
                canMine: bar.canMine
                isFullScreen: bar.isFullScreen
                inspectorOpen: bar.inspectorOpen
                onToggleStudyRequested: bar.toggleStudyRequested()
                onOpenVideoRequested: bar.openVideoRequested()
                onMineSubtitleRequested: bar.mineSubtitleRequested()
                onToggleInspectorRequested: bar.toggleInspectorRequested()
                onToggleFullScreenRequested: bar.toggleFullScreenRequested()
            }
        }
    }

    component SpeedPill: Button {
        id: speedPill

        objectName: "speedButton"
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: 52
        implicitHeight: 28
        padding: 0
        horizontalPadding: 0
        focusPolicy: Qt.NoFocus
        text: bar.speedLabel(bar.speed)
        onClicked: bar.speedPanelToggleRequested()

        ToolTip.visible: speedPill.hovered
        ToolTip.text: qsTr("Playback Speed")
        ToolTip.delay: 700
        Accessible.name: qsTr("Playback Speed")

        contentItem: Text {
            text: speedPill.text
            color: bar.compactLayout ? Theme.overlayForeground : Theme.textPrimary
            font.pixelSize: 13
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: 14
            color: bar.speedPanelOpen
                ? Theme.cardStroke
                : (bar.compactLayout ? Qt.rgba(0, 0, 0, 0.2) : Theme.floatingBackground)
            border.width: Theme.cardBorderWidth
            border.color: Theme.cardStroke
        }
    }

    VideoSpeedPanel {
        id: speedPanel

        visible: bar.speedPanelOpen
        anchors.right: parent.right
        anchors.rightMargin: bar.compactLayout ? 22 : 8
        anchors.bottom: parent.top
        anchors.bottomMargin: 8
        speed: bar.speed
        onSpeedChangeRequested: function (value) {
            bar.speedChangeRequested(value)
        }

        HoverHandler {
            id: speedPanelHover

            onHoveredChanged: bar.speedPanelHoverChanged(hovered)
        }
    }
}
