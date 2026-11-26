// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small rounded chip, following Features/Video/VideoInspectorView.swift
// `VideoInspectorChipButtonStyle`: caption semibold text with an optional
// glyph, a faint primary fill, and the accent fill with on-accent text while
// `selected` (speed presets, A-B loop and Open Subtitles).

import QtQuick
import QtQuick.Controls
import Niratan

Button {
    id: chip

    property bool selected: false
    // An SF Symbol name, drawn before the text unless requested otherwise.
    property string glyph
    property bool glyphAfterText: false

    implicitWidth: Math.max(implicitBackgroundWidth, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.chipHeight
    leftPadding: Theme.chipPaddingHorizontal
    rightPadding: Theme.chipPaddingHorizontal
    topPadding: 0
    bottomPadding: 0
    opacity: enabled ? 1 : Theme.disabledOpacity
    font.pixelSize: Theme.captionFont.pixelSize
    font.weight: Theme.weightSemibold

    background: Rectangle {
        implicitHeight: Theme.chipHeight
        radius: Theme.chipRadius
        color: chip.selected ? Theme.accent
            : chip.down ? Theme.pressTint
            : chip.hovered ? Theme.hoverTint : Theme.inspectorChipBackground
    }

    contentItem: Item {
        implicitWidth: labelRow.implicitWidth
        implicitHeight: labelRow.implicitHeight

        Row {
            id: labelRow

            anchors.centerIn: parent
            spacing: Theme.chipGlyphSpacing
            layoutDirection: chip.glyphAfterText ? Qt.RightToLeft : Qt.LeftToRight

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                visible: chip.glyph.length > 0
                name: chip.glyph
                size: Theme.chipGlyphSize
                color: label.color
            }

            Label {
                id: label

                anchors.verticalCenter: parent.verticalCenter
                text: chip.text
                font: chip.font
                color: chip.selected ? Theme.textOnAccent : Theme.textPrimary
            }
        }
    }
}
