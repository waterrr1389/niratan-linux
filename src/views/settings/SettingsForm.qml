// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsForm.cardsBody.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: form

    default property alias cards: cards.data
    readonly property real cardWidth: Math.min(840, Math.max(0, scroll.availableWidth - 48))

    ScrollView {
        id: scroll

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        contentHeight: cards.implicitHeight + 36
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: cards

            y: 18
            width: form.cardWidth
            x: (scroll.availableWidth - width) / 2
            spacing: 22
        }
    }
}
