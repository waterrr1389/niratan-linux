// SPDX-License-Identifier: GPL-3.0-or-later
//
// A colour swatch that opens a colour dialog with alpha on the current colour.
// Follows SwiftUI ColorPicker with labels hidden as used in
// Features/Settings/AppearanceView.swift (Background, Text and Info Color rows)
// and Features/Video/VideoInspectorView.swift colorRow. The well is 44 by 24
// at radius 6 with a hairline; a translucent colour is drawn over a
// checkerboard so its alpha shows. picked is emitted when the dialog is
// accepted; the page writes `color` back.
import QtQuick
import QtQuick.Dialogs
import Niratan

Item {
    id: well

    property color color: "transparent"

    signal picked(color color)

    implicitWidth: Theme.colorWellWidth
    implicitHeight: Theme.colorWellHeight

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: color.toString()
    Accessible.onPressAction: open()
    Keys.onSpacePressed: open()

    function open() {
        dialog.selectedColor = well.color
        dialog.open()
    }

    onColorChanged: swatch.requestPaint()

    // The software renderer has no shader to clip to a rounded shape, so the
    // swatch paints its own rounded checkerboard and colour.
    Canvas {
        id: swatch

        anchors.fill: parent

        onPaint: {
            const context = getContext("2d")
            const radius = Theme.colorWellRadius
            const cell = Theme.checkerboardCell
            context.reset()
            context.beginPath()
            context.roundedRect(0, 0, width, height, radius, radius)
            context.clip()
            if (well.color.a < 1) {
                context.fillStyle = Theme.checkerboardLight
                context.fillRect(0, 0, width, height)
                context.fillStyle = Theme.checkerboardDark
                for (let row = 0; row * cell < height; ++row) {
                    for (let column = row % 2; column * cell < width; column += 2)
                        context.fillRect(column * cell, row * cell, cell, cell)
                }
            }
            context.fillStyle = well.color
            context.fillRect(0, 0, width, height)
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.colorWellRadius
        color: "transparent"
        border.width: well.activeFocus ? Theme.focusRingWidth : Theme.hairlineWidth
        border.color: well.activeFocus ? Theme.accent : Theme.cardStroke
    }

    TapHandler {
        onTapped: well.open()
    }

    ColorDialog {
        id: dialog

        objectName: "colorDialog"
        options: ColorDialog.ShowAlphaChannel
        onAccepted: well.picked(selectedColor)
    }
}
