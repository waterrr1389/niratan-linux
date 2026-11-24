// SPDX-License-Identifier: GPL-3.0-or-later
//
// An accent-washed card, following Features/Reader/StatisticsView.swift
// `sessionHero`: padding 16, radius 16, filled with the accent at 12% while
// `active` and 6% otherwise. Children stack in a column 10 apart and take
// the card's inner width.

import QtQuick
import Niratan

Rectangle {
    id: card

    property bool active: false
    default property alias content: body.data

    implicitHeight: body.implicitHeight + 2 * Theme.heroCardPadding
    radius: Theme.heroCardRadius
    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                   active ? Theme.heroCardActiveOpacity : Theme.heroCardIdleOpacity)

    Column {
        id: body

        x: Theme.heroCardPadding
        y: Theme.heroCardPadding
        width: card.width - 2 * Theme.heroCardPadding
        spacing: Theme.heroCardSpacing

        onChildrenChanged: {
            for (const child of children)
                child.width = Qt.binding(() => body.width)
        }
    }
}
