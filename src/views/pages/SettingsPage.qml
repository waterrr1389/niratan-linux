// SPDX-License-Identifier: GPL-3.0-or-later
// NativeMac/NativeReuseViews.swift: NativeSettingsReuseView and NativeSettingsDetailView.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

RowLayout {
    id: page

    property real toolbarInset: 0
    readonly property real columnWidth: Math.min(240, Math.max(180, width * 0.28))

    objectName: "settingsPage"
    spacing: 0

    Component.onCompleted: sidebar.forceActiveFocus()

    SettingsSectionRail {
        id: sidebar

        Layout.minimumWidth: page.columnWidth
        Layout.preferredWidth: page.columnWidth
        Layout.maximumWidth: page.columnWidth
        Layout.fillHeight: true
        toolbarInset: page.toolbarInset
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0

        // Match the leading column's toolbar; the form clips below it.
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: sidebar.toolbarHeight
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: {
                switch (sidebar.selected) {
                case 0: return profiles
                case 2: return dictionaries
                case 3: return anki
                case 4: return statistics
                case 5: return sasayaki
                case 6: return wordAudio
                case 7: return syncSettings
                default: return appearance
                }
            }
        }
    }

    Component { id: profiles; ProfilesPage {} }
    Component { id: appearance; AppearanceSettings {} }
    Component { id: dictionaries; DictionarySettingsPage {} }
    Component { id: anki; AnkiSettingsPage {} }
    Component { id: statistics; StatisticsSettings {} }
    Component { id: sasayaki; SasayakiSettingsForm {} }
    Component { id: wordAudio; WordAudioSettings {} }
    Component { id: syncSettings; SyncSettings {} }
}
