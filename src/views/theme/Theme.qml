// SPDX-License-Identifier: GPL-3.0-or-later
//
// The light and dark tokens every view draws with, following
// NativeMac/NativeGlassSurface.swift (NativeGlassPalette.cardTint and stroke,
// nativeGlassCardSurface's 18 point radius), Features/Popup/popup.css (the
// --background-color, --text-color, --text-color-light*, --background-color-
// dark1, --danger-color*, --success-color and dark .overlay values), and
// Core/UserConfig.swift (preferredColorScheme) with
// NativeMac/HoshiNativeMacApp.swift (the system appearance fallback).
//
// The appearance follows QStyleHints.colorScheme. Niratan pins only Light and
// Dark; every other theme follows the system, which Unknown resolves to the
// light set. NIRATAN_THEME=light|dark forces the scheme for local visual
// checks, mirroring HOSHI_DEBUG_SYSTEM_APPEARANCE.
//
// The window background is the platform window background already used by the
// reader theme. Cards and floating surfaces replace the glass effect with the
// card tint composited over that background and the original stroke alpha;
// glass, shadows and gradients are not reproduced. Menus share the popup
// tokens because both are floating overlays and popup.css is the only palette
// in the sources.
//
// This file is the only place in src/views that may hold color literals.
// Views reference the tokens and never write colors of their own.

pragma Singleton

import QtQuick
import Niratan

QtObject {
    readonly property bool dark: ThemeState.dark

    readonly property color windowBackground: dark ? "#1E1E1E" : "#ECECEC"

    readonly property color cardBackground: dark ? "#262626" : "#F1F1F1"
    readonly property color cardStroke: dark ? "#21FFFFFF" : "#17000000"
    readonly property int cardRadius: 18
    readonly property real cardBorderWidth: 1

    readonly property color floatingBackground: cardBackground
    readonly property color floatingStroke: cardStroke

    readonly property color popupBackground: dark ? "#000000" : "#FFFFFF"
    readonly property color popupText: dark ? "#FFFFFF" : "#000000"
    readonly property color popupTextSecondary: dark ? "#AAAAAA" : "#555555"
    readonly property color popupTextTertiary: dark ? "#999999" : "#666666"
    readonly property color popupTextMuted: dark ? "#888888" : "#777777"
    readonly property color popupDivider: dark ? "#333333" : "#EEEEEE"
    readonly property color popupDanger: dark ? "#DD6755" : "#C83C28"
    readonly property color popupDangerLight: dark ? "#C83C28" : "#DD6755"
    readonly property color popupSuccess: dark ? "#75CF54" : "#51AB30"
    readonly property color popupOverlay: dark ? "#222222" : "#FFFFFF"

    readonly property color menuBackground: popupBackground
    readonly property color menuText: popupText
    readonly property color menuDivider: popupDivider

    readonly property color textPrimary: popupText
    readonly property color textSecondary: popupTextSecondary
}
