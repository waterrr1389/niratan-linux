// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Button {
    id: barButton

    property string glyph
    property string tip
    property bool active: false
    property int glyphSize: 11
    property int buttonWidth: 28
    property int buttonHeight: 28

    implicitWidth: barButton.buttonWidth
    implicitHeight: barButton.buttonHeight
    padding: 0
    horizontalPadding: 0
    focusPolicy: Qt.NoFocus
    Layout.alignment: Qt.AlignVCenter
    text: barButton.glyph

    ToolTip.visible: barButton.hovered && barButton.tip.length > 0
    ToolTip.text: barButton.tip
    ToolTip.delay: 700

    contentItem: Text {
        text: barButton.glyph
        color: barButton.active ? Theme.accent : Theme.textPrimary
        font.pixelSize: barButton.glyphSize
        font.bold: barButton.active
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: barButton.buttonHeight / 2
        color: barButton.active || barButton.down || barButton.hovered
            ? Theme.cardStroke : Theme.floatingBackground
        border.width: barButton.active ? Theme.cardBorderWidth : 0
        border.color: Theme.cardStroke
    }
}
