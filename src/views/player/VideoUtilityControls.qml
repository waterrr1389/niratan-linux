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
import QtQuick.Controls
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

    component UtilityButton: Button {
        id: button

        property string tip
        property string symbol
        property string glyphName
        property bool active: false

        implicitWidth: 32
        implicitHeight: 32
        padding: 0
        horizontalPadding: 0
        focusPolicy: Qt.NoFocus

        ToolTip.visible: button.hovered && button.tip.length > 0
        ToolTip.text: button.tip
        ToolTip.delay: 700
        Accessible.name: button.tip

        contentItem: Icon {
            objectName: button.glyphName
            name: button.symbol
            size: 18
            color: button.active ? Theme.accent : Theme.textPrimary
        }

        background: Rectangle {
            radius: button.height / 2
            color: button.active || button.down || button.hovered
                ? Theme.cardStroke : Theme.floatingBackground
            border.width: button.active ? Theme.cardBorderWidth : 0
            border.color: Theme.cardStroke
        }
    }

    Row {
        id: row

        spacing: 6

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
            onClicked: utility.toggleFullScreenRequested()

            contentItem: Icon {
                objectName: "fullScreenIcon"
                name: utility.isFullScreen ? "arrows-in" : "arrows-out"
                size: 18
                color: Theme.textPrimary
            }
        }
    }
}
