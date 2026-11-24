// SPDX-License-Identifier: GPL-3.0-or-later
//
// The installed font families, read once per QML engine for every font
// picker. Follows Features/Settings/AppearanceView.swift availableFonts and
// Features/Video/VideoInspectorView.swift subtitleFontFamilies: `installed`
// keeps the font database order the reader Font row lists; `sorted` is a
// separate copy in locale order with numeric comparison for the subtitle
// font row. Fonts installed while the application runs appear after restart.
pragma Singleton

import QtQuick

QtObject {
    readonly property list<string> installed: Qt.fontFamilies()
    readonly property var sorted: installed.slice().sort(function (a, b) {
        return a.localeCompare(b, undefined, { numeric: true, sensitivity: "base" })
    })
}
