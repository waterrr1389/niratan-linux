// SPDX-License-Identifier: GPL-3.0-or-later
//
// One interface glyph, addressed by the SF Symbol name Niratan draws, so a
// view names the Swift symbol and gets the bundled Phosphor Icons 2.1.1 asset
// for it. The table in src/views/icons/icon_map.js is the only place that maps
// a symbol name; views never name a Phosphor file.
//
// Follows NativeMac/NativeMacSection.swift `NativeMacSection.systemImage` and
// Features/Dictionary/DictionarySearchView.swift `DictionarySearchBar.body`.
// The glyph keeps the SVG alpha and takes its colour from `color`, which
// defaults to the primary text token; a name the table does not know has
// `known: false`, no `source` and draws nothing, so a missing row is visible
// rather than replaced by an unrelated glyph.
//
// Tint: the bundled SVGs paint with `fill="currentColor"`, which the Qt SVG
// renderer resolves to black. MultiEffect needs a shader, and the software
// scene graph (the VNC renderer reviews use, and any machine without a GPU
// context) does not draw shader effects. IconImage from Qt's own control
// implementation colourises the loaded pixmap on the CPU with source-in
// compositing, so the glyph is exactly `color`, its alpha and antialiasing are
// the SVG's own, and the same code draws in hardware and software renderers.

import QtQuick
import QtQuick.Controls.impl
import Niratan
import "../icons/icon_map.js" as IconMap

Item {
    id: icon

    property string name
    property real size: 16
    property color color: Theme.textPrimary

    readonly property var entry: IconMap.icons[name]
    readonly property bool known: entry !== undefined
    readonly property url source: {
        if (!known)
            return ""
        const weight = entry.weight
        const suffix = weight === "fill" ? "-fill" : ""
        return "qrc:/qt/qml/Niratan/src/views/icons/phosphor/" + weight + "/"
            + entry.name + suffix + ".svg"
    }

    implicitWidth: size
    implicitHeight: size

    IconImage {
        anchors.fill: parent
        visible: icon.known
        source: icon.source
        color: icon.color
        sourceSize.width: icon.size
        sourceSize.height: icon.size
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
}
