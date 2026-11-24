// SPDX-License-Identifier: GPL-3.0-or-later
//
// checkmark.circle.fill as Features/Bookshelf/BookView.swift `ShelfBookCard`
// marks a finished or chosen book: a white check on a filled circle of
// `tint`.
import QtQuick
import Niratan

Item {
    id: badge

    property color tint: Theme.accent

    implicitWidth: Theme.checkBadgeSize
    implicitHeight: Theme.checkBadgeSize

    Rectangle {
        anchors.centerIn: parent
        width: Theme.checkBadgeKnob
        height: Theme.checkBadgeKnob
        radius: width / 2
        color: Theme.knob
    }

    Icon {
        anchors.fill: parent
        size: Theme.checkBadgeSize
        name: "checkmark.circle.fill"
        color: badge.tint
    }
}
