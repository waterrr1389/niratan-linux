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
// Light and Dark pin the interface appearance; System follows
// QStyleHints.colorScheme, with Unknown resolving to the light set.
// NIRATAN_THEME=light|dark overrides the system scheme for local visual checks.
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

    // The video stage stays black in both themes: letterboxing must not follow
    // the window surface.
    readonly property color videoBackground: "#000000"
    // Glyphs drawn over a dark media overlay stay white in both themes; the
    // scrim is black at 45% for the unread-image cue and thumbnail badge.
    readonly property color overlayForeground: "#FFFFFF"
    readonly property color mediaOverlayScrim: "#73000000"

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
    // AnkiMiningToastView.tint uses orange for duplicates and blue for pending.
    readonly property color popupWarning: dark ? "#FF9F0A" : "#FF9500"
    readonly property color popupInfo: dark ? "#0A84FF" : "#007AFF"
    readonly property color popupOverlay: dark ? "#222222" : "#FFFFFF"

    readonly property color menuBackground: popupBackground
    readonly property color menuText: popupText
    readonly property color menuDivider: popupDivider

    // The platform accent, the Linux analogue of SwiftUI's accentColor and
    // Niratan-win's SystemAccentColor. Injected from the system palette so the
    // desktop's chosen accent is kept instead of a hard-coded blue.
    readonly property color accent: ThemeState.accent

    readonly property color textPrimary: popupText
    readonly property color textSecondary: popupTextSecondary

    // The metrics below are a platform-neutral scale, not macOS values. They
    // keep the density of the source layouts without copying any AppKit or
    // glass geometry, so views stop spelling the same spacing, duration and
    // text sizes as bare numbers.

    // Spacing steps, in pixels.
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24

    // Transition durations, in milliseconds.
    readonly property int motionFast: 120
    readonly property int motionNormal: 180
    readonly property int motionSlow: 220

    // Text sizes, in pixels.
    readonly property int fontLargeTitle: 22
    readonly property int fontTitle: 17
    readonly property int fontBody: 14
    readonly property int fontCaption: 12

    readonly property int controlRadius: 8
}
