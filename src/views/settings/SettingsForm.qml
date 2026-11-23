// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsForm.cardsBody.
//
// The cards scroll in a Flickable that stops at its bounds, as
// PanelScaffold does, so the form never rebounds past its top or bottom.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: form

    default property alias cards: cards.data
    readonly property real cardWidth: Math.min(840, Math.max(0, scroll.width - 48))

    Flickable {
        id: scroll

        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: cards.implicitHeight + 36
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            visible: scroll.contentHeight > scroll.height
        }

        ColumnLayout {
            id: cards

            y: 18
            width: form.cardWidth
            x: (scroll.width - width) / 2
            spacing: 22
        }
    }
}
