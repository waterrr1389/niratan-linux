// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Settings section rail, extracted from SettingsPage.qml. It follows the
// settings sidebar of NativeMac/NativeReuseViews.swift
// NativeSettingsReuseView.settingsSidebar and Features/Settings/SettingsHomeView.swift:
// one selectable row per section over the page background, with the selected
// row carrying an accent bar. Profiles opens its own page first, as the
// sidebar's Library group orders it; the repo keeps its Reader, Dictionary and
// Statistics destinations, adds Sasayaki (Audiobooks) after Statistics as the
// sidebar does, adds the Dictionaries management page after Dictionary, and
// appends Anki, which opens AnkiView's own page.
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
    signal dictionariesRequested()
    signal profilesRequested()

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

        // Profiles opens its own page first, as NativeReuseViews.swift puts the
        // Profiles screen at the top of the sidebar's Library group (O1).
        RailRow {
            text: qsTr("Profiles")
            onActivated: rail.profilesRequested()
        }

        RailRow {
            text: qsTr("Reader")
            current: rail.selected === 0
            onActivated: rail.sectionSelected(0)
        }

        RailRow {
            text: qsTr("Dictionary")
            current: rail.selected === 1
            onActivated: rail.sectionSelected(1)
        }

        // The managed dictionary library is its own page, reached from the
        // settings rail the way Anki is, and sits after the Dictionary
        // behaviour settings.
        RailRow {
            text: qsTr("Dictionaries")
            onActivated: rail.dictionariesRequested()
        }

        RailRow {
            text: qsTr("Statistics")
            current: rail.selected === 2
            onActivated: rail.sectionSelected(2)
        }

        RailRow {
            text: qsTr("Sasayaki (Audiobooks)")
            current: rail.selected === 3
            onActivated: rail.sectionSelected(3)
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
