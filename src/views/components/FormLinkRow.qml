// SPDX-License-Identifier: GPL-3.0-or-later
//
// A row that opens `url` when clicked anywhere, with a trailing arrow.up.right
// glyph at footnote size in tertiary. Follows Features/Settings/AboutView.swift
// linkRow, whose whole rectangle is the button.
import QtQuick
import Niratan

FormRow {
    id: row

    property url url

    signal clicked()

    accessoryItem: arrow
    clickable: true

    Accessible.role: Accessible.Link
    Accessible.name: label

    onTapped: {
        Qt.openUrlExternally(url)
        clicked()
    }

    Icon {
        id: arrow

        name: "arrow.up.right"
        size: Theme.footnoteFont.pixelSize
        color: Theme.textTertiary
    }
}
