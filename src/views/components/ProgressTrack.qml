// SPDX-License-Identifier: GPL-3.0-or-later
//
// A determinate progress bar, following SwiftUI ProgressView(value:) with
// controlSize(.small) as ReaderGoToView.bookSummary and
// ReaderStatisticsContentView.bookProgress draw it: a thin capsule track
// with the accent fill from the leading edge. `value` is clamped to 0...1.
// `onMedia` draws the track in translucent white for use over artwork, as
// VideoLibraryProgressTrack does on a poster's scrim.

import QtQuick
import Niratan

Item {
    id: track

    property real value: 0
    property bool onMedia: false

    readonly property real clampedValue: Math.min(Math.max(value, 0), 1)

    implicitHeight: Theme.progressTrackHeight

    Accessible.role: Accessible.ProgressBar

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: track.onMedia ? Qt.alpha(Theme.overlayForeground, Theme.mediaTrackOpacity)
                             : Theme.trackBackground
    }

    Rectangle {
        width: track.width * track.clampedValue
        height: track.height
        radius: height / 2
        color: Theme.accent
        visible: track.clampedValue > 0
    }
}
