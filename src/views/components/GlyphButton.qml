// SPDX-License-Identifier: GPL-3.0-or-later
//
// A round glyph-only button, following Features/Video/VideoInspectorView.swift
// `VideoInspectorIconButtonStyle`: a circle of `diameter` with the faint chip
// fill when `filled`, bare otherwise, and the SF Symbol centred in the primary
// colour (stepper steps, Rotate and the per-slider reset).

import QtQuick
import QtQuick.Controls
import Niratan

Button {
    id: step

    // An SF Symbol name.
    property string glyph
    property int diameter: Theme.glyphButtonDiameter
    property real glyphSize: Theme.glyphSize
    property bool filled: true

    implicitWidth: diameter
    implicitHeight: diameter
    padding: 0
    opacity: enabled ? 1 : Theme.disabledOpacity

    background: Rectangle {
        radius: width / 2
        color: step.down ? Theme.pressTint
            : step.hovered ? Theme.hoverTint
            : step.filled ? Theme.inspectorChipBackground : "transparent"
    }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: step.glyph
            size: step.glyphSize
            color: Theme.textPrimary
        }
    }
}
