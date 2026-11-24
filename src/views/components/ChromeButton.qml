// SPDX-License-Identifier: GPL-3.0-or-later
//
// A round glyph button of the video playback chrome, following the circular
// buttons of Features/Video/VideoControlsView.swift (`episodeControls`,
// `volumeControl`, `utilityControlGroup`): a glyph on the floating surface
// that tints on hover and press, and an accent glyph with a hairline ring
// while `active`. `bordered` keeps the ring at rest, as the play and mute
// buttons draw it. `tip` is the tooltip and the accessible name.
import QtQuick
import QtQuick.Controls
import Niratan

Button {
    id: button

    property string tip
    property string symbol
    property real symbolSize: Theme.chromeGlyphSize
    property real diameter: Theme.chromeButtonDiameter
    property bool active: false
    property bool bordered: false
    // The object name of the glyph, for tests that read it.
    property string glyphName

    implicitWidth: diameter
    implicitHeight: diameter
    padding: 0
    horizontalPadding: 0
    focusPolicy: Qt.NoFocus

    ToolTip.visible: button.hovered && button.tip.length > 0
    ToolTip.text: button.tip
    ToolTip.delay: Theme.tooltipDelay
    Accessible.name: button.tip

    contentItem: Icon {
        objectName: button.glyphName
        name: button.symbol
        size: button.symbolSize
        color: button.active ? Theme.accent : Theme.textPrimary
    }

    background: Rectangle {
        radius: height / 2
        color: button.active || button.down || button.hovered
            ? Theme.cardStroke : Theme.floatingBackground
        border.width: button.active || button.bordered ? Theme.cardBorderWidth : 0
        border.color: Theme.cardStroke
    }
}
