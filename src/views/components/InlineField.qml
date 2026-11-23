// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small text field on a rounded field surface, following
// Features/Video/VideoInspectorView.swift `VideoInspectorFieldSurface`: the
// value in caption semibold tabular digits, then a secondary `suffix` or a
// tertiary `glyph` on the trailing side (the speed field's `x`, the offset
// field's keyboard).
//
// `edited(text)` follows each keystroke and `accepted(text)` a Return; the
// page owns the value and writes `text` back.

import QtQuick
import QtQuick.Controls
import Niratan

Rectangle {
    id: surface

    property string text
    property string suffix
    // An SF Symbol name drawn after the field.
    property string glyph
    property real fieldWidth: Theme.inlineFieldWidth
    property alias fieldName: input.objectName
    property alias horizontalAlignment: input.horizontalAlignment
    readonly property Item field: input

    signal edited(string text)
    signal accepted(string text)

    implicitWidth: surface.fieldWidth + (trailing.visible ? trailing.width + Theme.spacingXs : 0)
                   + 2 * Theme.chipPaddingHorizontal
    implicitHeight: Theme.chipHeight
    radius: Theme.chipRadius
    color: Theme.inspectorFieldBackground
    border.width: Theme.hairlineWidth
    border.color: Theme.controlStroke

    TextField {
        id: input

        x: Theme.chipPaddingHorizontal
        anchors.verticalCenter: parent.verticalCenter
        width: surface.width - 2 * Theme.chipPaddingHorizontal
               - (trailing.visible ? trailing.width + Theme.spacingXs : 0)
        implicitWidth: surface.fieldWidth
        padding: 0
        background: null
        text: surface.text
        horizontalAlignment: TextInput.AlignRight
        font.pixelSize: Theme.captionFont.pixelSize
        font.weight: Theme.weightSemibold
        font.features: { "tnum": 1 }
        onTextEdited: surface.edited(text)
        onAccepted: surface.accepted(text)
    }

    Item {
        id: trailing

        visible: surface.suffix !== "" || surface.glyph !== ""
        x: surface.width - Theme.chipPaddingHorizontal - width
        anchors.verticalCenter: parent.verticalCenter
        width: surface.glyph !== "" ? glyphIcon.width : suffixLabel.implicitWidth
        height: Math.max(glyphIcon.height, suffixLabel.implicitHeight)

        StyledLabel {
            id: suffixLabel

            visible: surface.suffix !== ""
            anchors.verticalCenter: parent.verticalCenter
            text: surface.suffix
            textStyle: Theme.captionFont
            weight: Theme.weightSemibold
            secondary: true
        }

        Icon {
            id: glyphIcon

            visible: surface.glyph !== ""
            anchors.verticalCenter: parent.verticalCenter
            name: surface.glyph
            size: Theme.chipGlyphSize
            color: Theme.textTertiary
        }
    }
}
