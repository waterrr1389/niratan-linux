// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bar's tool cluster, following Features/Video/VideoControlsView.swift
// `utilityControlGroup`, `miningHistoryButton`, `openVideoButton`,
// `mineCurrentSubtitleButton`, `fullScreenButton`, `condensedControlGroup` and
// `minimalControlGroup`, with `VideoPlayerScreen.toggleFullScreen`.
//
// Order at full density: Mining History, Open Video, Mine Current Subtitle,
// Inspector, Full Screen. Condensed keeps the episode group (drawn by
// VideoTransportControls), the speed pill, Open Video, Inspector and Full
// Screen; minimal drops all but the episode group and Full Screen, so the
// Inspector button is absent there, as in Swift's `minimalControlGroup`.
// Swift's screenshot and subtitle gap fast-forward buttons are not built.
// Mining History is active while the study sidebar is open; Mine Current
// Subtitle follows `canMine`; Inspector is active while its panel is open.
// The full-screen glyph enters from windowed and leaves from full screen.
//
// Object names: miningHistoryButton, openVideoButton, mineSubtitleButton,
// inspectorButton, fullScreenButton; the full-screen glyph is fullScreenIcon
// and switches between arrows-out and arrows-in. Every button carries a `tip`
// string; Mining History and Inspector expose `active`.
pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: utility

    objectName: "videoUtilityControls"

    // "full", "condensed" or "minimal" (VideoControlsView.ControlDensity).
    property string density: "full"
    property bool studyOpen: false
    property bool canMine: false
    property bool isFullScreen: false
    property bool inspectorOpen: false

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    signal toggleStudyRequested()
    signal openVideoRequested()
    signal mineSubtitleRequested()
    signal toggleInspectorRequested()
    signal toggleFullScreenRequested()

    component UtilityButton: ChromeButton {
        diameter: Theme.chromeToolDiameter
    }

    Row {
        id: row

        spacing: Theme.chromeToolSpacing

        UtilityButton {
            objectName: "miningHistoryButton"
            tip: qsTr("Mining History")
            symbol: "clock.arrow.circlepath"
            active: utility.studyOpen
            visible: utility.density === "full"
            onClicked: utility.toggleStudyRequested()
        }

        UtilityButton {
            objectName: "openVideoButton"
            tip: qsTr("Open Video")
            symbol: "film"
            visible: utility.density !== "minimal"
            onClicked: utility.openVideoRequested()
        }

        UtilityButton {
            objectName: "mineSubtitleButton"
            tip: qsTr("Mine Current Subtitle")
            symbol: "tray.and.arrow.down"
            visible: utility.density === "full"
            enabled: utility.canMine
            onClicked: utility.mineSubtitleRequested()
        }

        UtilityButton {
            objectName: "inspectorButton"
            tip: qsTr("Inspector")
            symbol: "sidebar.trailing"
            glyphName: "inspectorGlyph"
            active: utility.inspectorOpen
            visible: utility.density !== "minimal"
            onClicked: utility.toggleInspectorRequested()
        }

        UtilityButton {
            objectName: "fullScreenButton"
            tip: qsTr("Toggle Full Screen")
            symbol: utility.isFullScreen ? "arrows-in" : "arrows-out"
            glyphName: "fullScreenIcon"
            onClicked: utility.toggleFullScreenRequested()
        }
    }
}
