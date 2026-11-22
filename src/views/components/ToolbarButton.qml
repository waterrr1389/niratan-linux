// SPDX-License-Identifier: GPL-3.0-or-later
//
// A toolbar item after Mac Niratan's window toolbar buttons
// (NativeMac/NativeReuseViews.swift NativeBookshelfReuseView.toolbarContent):
// the style's ToolButton drawing an SF Symbol from the icon table, with the
// item's `.help` text as tool tip and accessible name.
import QtQuick
import QtQuick.Controls
import Niratan
import "../icons/icon_map.js" as IconMap

ToolButton {
    id: button

    property string symbol
    property string help

    padding: 7
    // macOS toolbar items do not take focus from the list being used.
    focusPolicy: Qt.TabFocus
    display: AbstractButton.IconOnly
    icon.source: IconMap.source(button.symbol)
    icon.width: 17
    icon.height: 17

    Accessible.name: button.help
    ToolTip.visible: button.hovered && button.help.length > 0
    ToolTip.text: button.help
    ToolTip.delay: 700
}
