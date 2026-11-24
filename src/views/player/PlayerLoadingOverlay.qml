// SPDX-License-Identifier: GPL-3.0-or-later
//
// The player's loading surface, following
// Features/Video/VideoPlayerScreen.shouldShowVideoLoadingIndicator and the
// loading branch of `videoSurface`: a black cover over the stage with a large
// progress indicator and "Loading Video...". It is drawn above the video
// surface (Swift's zIndex 50), not as part of the no-video empty view.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Niratan

Rectangle {
    id: overlay

    objectName: "loadingOverlay"
    color: Theme.videoBackground

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingMd

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: overlay.visible
        }

        StyledLabel {
            text: qsTr("Loading Video...")
            textStyle: Theme.calloutFont
            weight: Theme.weightMedium
            color: Theme.overlayForeground
            opacity: Theme.chromeOverlayTextOpacity
        }
    }
}
