// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's Appearance side panel, following NativeMac/NativeReaderView.swift
// `body` `.appearance` branch: the NativeReaderInspectorHeader ("Appearance",
// Close) above NativeSettingsDetailView(section: .appearance), whose
// NativeMac/NativeReuseViews.swift mapping reaches
// Features/Settings/AppearanceView.swift `nativeAppearanceContent`. The host
// reuses src/views/settings/AppearanceSettings.qml, the reviewed Settings form,
// instead of keeping a second copy of the rows.
//
// Every row edits the profile-backed SettingsViewModel, which saves the value
// for the active profile; the open reader watches that file and reloads the
// chapter with the new layout.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Niratan

ColumnLayout {
    id: panel

    signal closeRequested()

    spacing: 0

    PanelHeader {
        Layout.fillWidth: true
        title: qsTr("Appearance")
        onCloseRequested: panel.closeRequested()
    }

    AppearanceSettings {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
}
