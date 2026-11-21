// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import Niratan

Button {
    id: loopMarker
    property bool selected: false
    implicitWidth: 20
    implicitHeight: 20
    padding: 0
    focusPolicy: Qt.TabFocus
    ToolTip.visible: hovered
    contentItem: Text {
        text: loopMarker.text
        color: loopMarker.selected ? Theme.windowBackground : Theme.textSecondary
        font.pixelSize: Theme.fontCaption
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: width / 2
        color: loopMarker.selected ? Theme.accent : Theme.cardBackground
        border.width: loopMarker.activeFocus ? Theme.cardBorderWidth : 0
        border.color: Theme.accent
    }
}
