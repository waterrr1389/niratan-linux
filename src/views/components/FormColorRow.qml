// SPDX-License-Identifier: GPL-3.0-or-later
//
// A row whose trailing control is a ColorWell. Follows
// Features/Settings/AppearanceView.swift Background, Text and Info Color rows
// and Features/Video/VideoInspectorView.swift colorRow, whose label carries
// the paintpalette glyph in the compact density. picked is forwarded from the
// well when its dialog is accepted.
import QtQuick
import Niratan

FormRow {
    id: row

    property color color: "transparent"

    signal picked(color color)

    glyph: compact ? "paintpalette" : ""
    accessoryItem: well

    ColorWell {
        id: well

        color: row.color
        Accessible.name: row.label
        onPicked: color => row.picked(color)
    }
}
