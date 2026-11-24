// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import Niratan

Button {
    id: loopMarker
    property bool selected: false
    implicitWidth: Theme.studyLoopMarkerSize
    implicitHeight: Theme.studyLoopMarkerSize
    padding: 0
    horizontalPadding: 0
    focusPolicy: Qt.TabFocus
    ToolTip.visible: hovered
    contentItem: StyledLabel {
        text: loopMarker.text
        textStyle: Theme.captionFont
        weight: Theme.weightSemibold
        color: loopMarker.selected ? Theme.textOnAccent : Theme.textSecondary
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
