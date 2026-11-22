// SPDX-License-Identifier: GPL-3.0-or-later
//
// The secondary text line of the settings forms, split out of SettingsPage.qml
// so that page stays inside its size baseline. It follows the description and
// footer text of NativeMac/NativeReuseViews.swift (NativeSettingsSectionCard,
// NativeSettingsRow) and is used inside the page's Section cards.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Niratan

Label {
    Layout.fillWidth: true
    color: Theme.textSecondary
    font.pixelSize: Theme.fontCaption
    wrapMode: Text.WordWrap
}
