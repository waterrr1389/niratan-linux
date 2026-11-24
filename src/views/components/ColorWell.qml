// SPDX-License-Identifier: GPL-3.0-or-later
//
// A colour swatch that opens a colour dialog with alpha on the current colour.
// Follows SwiftUI ColorPicker with labels hidden as used in
// Features/Settings/AppearanceView.swift (Background, Text and Info Color rows)
// and Features/Video/VideoInspectorView.swift colorRow. The well is 44 by 24
// at radius 6 with a hairline; a translucent colour is drawn over a
// checkerboard so its alpha shows. picked is emitted when the dialog is
// accepted; the page writes `color` back. The dialog is created on the first
// open, so a page of wells costs no dialogs until one is used; each open
// starts from the well's colour at that moment. close(), or the well being
// hidden, dismisses an open dialog without emitting picked.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import Niratan

Item {
    id: well

    property color color: "transparent"

    // The colour dialog's title.
    property string dialogTitle

    signal picked(color color)
    // A click on the well; it opens the dialog.
    signal clicked()

    implicitWidth: Theme.colorWellWidth
    implicitHeight: Theme.colorWellHeight

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: color.toString()
    Accessible.onPressAction: open()
    Keys.onSpacePressed: open()

    onClicked: open()

    function open() {
        dialogLoader.active = true
        const dialog = dialogLoader.item as ColorDialog
        dialog.selectedColor = well.color
        dialog.open()
    }

    function close() {
        (dialogLoader.item as ColorDialog)?.reject()
    }

    onColorChanged: swatch.requestPaint()

    // A well hidden with its page (another section shown) does not leave its
    // dialog up; this is not an acceptance, so nothing is picked.
    onVisibleChanged: {
        if (!well.visible)
            well.close()
    }

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
        onTapped: well.clicked()
    }

    Loader {
        id: dialogLoader

        active: false
        sourceComponent: ColorDialog {
            id: dialog

            objectName: "colorDialog"
            title: well.dialogTitle
            options: ColorDialog.ShowAlphaChannel
            onAccepted: well.picked(dialog.selectedColor)
        }
    }
}
