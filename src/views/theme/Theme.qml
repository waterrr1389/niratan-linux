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
// Page, sidebar, card and label colours follow the dark README screenshots of
// Mac Niratan (Pictures/bookshelf.png, setting.png, video.png), sampled, and
// macOS's semantic colours for the light set, where NativeGlassSurface.swift
// composites the card tint over windowBackgroundColor. Glass is a solid
// surface with a hairline: blur and gradients are not reproduced.
//
// Text sizes follow the macOS text styles in points, drawn as logical pixels:
// largeTitle 26, title 22, title2 17, title3 15, body and headline 13, callout
// 12, subheadline 11, caption 10. The interface font is Inter with Noto Sans
// CJK JP for Japanese, the open stand-ins for SF Pro and Hiragino Sans.
//
// The accent is the desktop's, as Mac Niratan takes the system accent. Text
// on an accent fill is white while it keeps the 3:1 contrast macOS's white on
// system blue has, and black on accents too light for that.
//
// This file is the only place in src/views that may hold color literals.
// Views reference the tokens and never write colors of their own.

pragma Singleton

import QtQuick
import Niratan

QtObject {
    readonly property bool dark: ThemeState.dark

    // The page behind content, the main sidebar and the second column a
    // section shows next to it (shelves, settings sections, video library).
    readonly property color windowBackground: dark ? "#1C1C1E" : "#F2F2F2"
    readonly property color sidebarBackground: dark ? "#2A2A2D" : "#E6E6E8"
    readonly property color columnBackground: dark ? "#444445" : "#EDEDEF"
    readonly property color separator: dark ? "#1AFFFFFF" : "#1A000000"

    // The video stage stays black in both themes: letterboxing must not follow
    // the window surface.
    readonly property color videoBackground: "#000000"
    // Glyphs drawn over a dark media overlay stay white in both themes; the
    // scrim is black at 45% for the unread-image cue and thumbnail badge.
    readonly property color overlayForeground: "#FFFFFF"
    readonly property color mediaOverlayScrim: "#73000000"

    readonly property color cardBackground: dark ? "#2C2C2E" : "#FFFFFF"
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

    // The platform accent, the Linux analogue of SwiftUI's accentColor and
    // Niratan-win's SystemAccentColor. Injected from the system palette so the
    // desktop's chosen accent is kept instead of a hard-coded blue.
    readonly property color accent: ThemeState.accent

    // macOS labelColor, secondaryLabelColor and tertiaryLabelColor.
    readonly property color textPrimary: dark ? "#D9FFFFFF" : "#D9000000"
    readonly property color textSecondary: dark ? "#8CFFFFFF" : "#80000000"
    readonly property color textTertiary: dark ? "#40FFFFFF" : "#4D000000"
    // Text and glyphs drawn on an accent fill.
    readonly property color textOnAccent: 1.05 / (luminance(accent) + 0.05) >= 3
                                          ? "#FFFFFF" : "#000000"

    // Controls: the push button and field fill, the hover and press tint laid
    // over a control, the unselected row selection, and the switch knob.
    readonly property color controlBackground: dark ? "#3A3A3C" : "#FFFFFF"
    readonly property color controlStroke: dark ? "#14FFFFFF" : "#26000000"
    readonly property color fieldBackground: dark ? "#1FFFFFFF" : "#FFFFFF"
    readonly property color hoverTint: dark ? "#14FFFFFF" : "#0F000000"
    readonly property color pressTint: dark ? "#24FFFFFF" : "#1F000000"
    readonly property color inactiveSelection: dark ? "#1FFFFFFF" : "#14000000"
    // The checked segment of a segmented control, raised above its track.
    readonly property color segmentSelected: dark ? "#636366" : "#FFFFFF"
    readonly property color trackBackground: dark ? "#33FFFFFF" : "#1F000000"
    readonly property color knob: "#FFFFFF"
    readonly property color shadow: dark ? "#66000000" : "#26000000"

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
    readonly property int fontLargeTitle: 26
    readonly property int fontTitle1: 22
    readonly property int fontTitle2: 17
    readonly property int fontTitle: 15
    readonly property int fontBody: 13
    readonly property int fontCallout: 12
    readonly property int fontSubheadline: 11
    readonly property int fontCaption: 10

    readonly property int controlRadius: 8
    readonly property int controlHeight: 28
    readonly property int rowRadius: 8

    // WCAG relative luminance of an sRGB colour.
    function luminance(colour) {
        function linear(channel) {
            return channel <= 0.04045 ? channel / 12.92 : Math.pow((channel + 0.055) / 1.055, 2.4)
        }
        return 0.2126 * linear(colour.r) + 0.7152 * linear(colour.g) + 0.0722 * linear(colour.b)
    }
}
