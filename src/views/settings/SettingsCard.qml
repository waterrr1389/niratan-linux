// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsSectionCard.cardsBody.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: card

    property string title
    property string footer
    property alias footerFormat: footnote.textFormat
    property alias headerControls: headerControls.data
    default property alias rows: body.data

    Layout.fillWidth: true
    spacing: Theme.spacingSm

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 12
        Layout.rightMargin: 12
        visible: card.title.length > 0

        SettingsNote {
            text: card.title
            font.pixelSize: Theme.fontSubheadline
            font.weight: Font.DemiBold
        }

        RowLayout {
            id: headerControls
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: body.implicitHeight
        radius: Theme.cardRadius
        color: Theme.cardBackground
        border.color: Theme.cardStroke
        border.width: Theme.cardBorderWidth

        ColumnLayout {
            id: body

            anchors.fill: parent
            spacing: 0
        }
    }

    SettingsNote {
        id: footnote

        Layout.leftMargin: 12
        Layout.rightMargin: 12
        visible: card.footer.length > 0
        text: card.footer
        onLinkActivated: link => Qt.openUrlExternally(link)
    }
}
