// SPDX-License-Identifier: GPL-3.0-or-later
//
// The round glass button used for sheet close controls and reader tools,
// following NativeMac/NativeReuseViews.swift `NativeGlassCircleButton`. It is a
// circle of `diameter` filled with the card tint, a hairline primary-alpha
// stroke, and the named SF Symbol centred at `iconSize`; hover and press use
// the flat tones PageHeader.qml already uses. PLAN.md keeps glass solid, so
// the Swift shadow is not reproduced.
//
// The button is keyboard reachable: Tab focuses it and Space, Return or Enter
// activate it. `accessibleName` names the Button role for screen readers.

import QtQuick
import Niratan

Item {
    id: button

    property string icon
    property int diameter: 38
    property real iconSize: 16
    property string accessibleName

    signal clicked()

    implicitWidth: diameter
    implicitHeight: diameter

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.focusable: true
    Accessible.onPressAction: button.clicked()

    readonly property bool hovered: hoverHandler.hovered
    readonly property bool pressed: tapHandler.pressed

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter) {
            event.accepted = true
            button.clicked()
        }
    }

    HoverHandler {
        id: hoverHandler
    }

    TapHandler {
        id: tapHandler
        onTapped: button.clicked()
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: button.pressed
            ? Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.16)
            : button.hovered ? Theme.cardStroke : Theme.cardBackground
        border.width: 1
        border.color: Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, 0.14)
    }

    Icon {
        anchors.centerIn: parent
        name: button.icon
        size: button.iconSize
        color: Theme.textPrimary
    }
}
