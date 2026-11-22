// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Settings section rail, extracted from SettingsPage.qml. It follows the
// settings sidebar of NativeMac/NativeReuseViews.swift
// NativeSettingsReuseView.settingsSidebar and Features/Settings/SettingsHomeView.swift:
// one selectable row per section over the page background, with the selected
// row carrying an accent bar. The repo keeps its Reader, Dictionary and
// Statistics destinations, adds Sasayaki (Audiobooks) after Statistics as the
// sidebar does, and appends Anki, which opens AnkiView's own page.
// SettingsHomeView.swift gives each settings link an SF Symbol; the rail is
// text-only, so the `waveform` and `tray.full` icons are not reproduced.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Pane {
    id: rail

    property int selected: 0

    signal sectionSelected(int index)
    signal ankiRequested()

    Layout.fillHeight: true
    padding: Theme.spacingSm

    background: Rectangle {
        color: Theme.cardBackground

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: Theme.cardBorderWidth
            color: Theme.cardStroke
        }
    }

    component RailRow: Button {
        id: railRow

        property bool current: false
        signal activated()

        Layout.fillWidth: true
        leftPadding: Theme.spacingLg
        rightPadding: Theme.spacingLg
        Accessible.selected: railRow.current
        onClicked: railRow.activated()

        contentItem: Label {
            text: railRow.text
            color: Theme.textPrimary
            font.pixelSize: Theme.fontBody
            font.weight: railRow.current ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
        }

        background: Item {
            Rectangle {
                anchors.fill: parent
                visible: railRow.current || railRow.hovered || railRow.activeFocus
                color: Theme.windowBackground
                radius: Theme.controlRadius
                border.width: railRow.visualFocus ? 1 : 0
                border.color: Theme.accent
            }

            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingXs
                anchors.verticalCenter: parent.verticalCenter
                width: 2
                height: Theme.spacingLg
                radius: 1
                visible: railRow.current
                color: Theme.accent
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingXs

        Repeater {
            model: [qsTr("Reader"), qsTr("Dictionary"), qsTr("Statistics"),
                qsTr("Sasayaki (Audiobooks)")]

            delegate: RailRow {
                required property int index
                required property string modelData

                text: modelData
                current: rail.selected === index
                onActivated: rail.sectionSelected(index)
            }
        }

        RailRow {
            text: qsTr("Anki")
            onActivated: rail.ankiRequested()
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
