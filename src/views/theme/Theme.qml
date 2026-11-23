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
// The font* sizes follow the macOS text styles in points, drawn as logical
// pixels: largeTitle 26, title 22, title2 17, title3 15, body and headline 13,
// callout 12, subheadline 11, caption 10. The interface font is Inter with Noto
// Sans CJK JP for Japanese, the open stand-ins for SF Pro and Hiragino Sans.
//
// The control kit (src/views/components) draws with the *Font text styles in
// style C: macOS structure and hierarchy, with each SwiftUI text style about
// 8% larger so it reads well with Linux font rendering. largeTitle 28, title1
// 24, title2 19, title3 16, headline 14 semibold, body 14, callout 13,
// subheadline 12, footnote 11, caption 11, caption2 11 medium; every other
// style regular. Kit row heights are Swift's plus 2 and glyphs plus 1, while
// paddings, insets, spacing between groups and radii stay the Swift values.
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
    // The video Inspector: a panel one tone below its grouped cards, as
    // VideoInspectorGlassSurface and VideoInspectorSectionGlassSurface layer
    // a white-0.7 (light) or white-0.065 (dark) card over the panel wash.
    readonly property color inspectorBackground: dark ? "#242426" : "#F2F2F4"
    readonly property color inspectorCardBackground: dark ? "#11FFFFFF" : "#FFFFFF"
    // VideoInspectorChipButtonStyle and the filled icon buttons: primary at
    // 6-9% opacity.
    readonly property color inspectorChipBackground: dark ? "#17FFFFFF" : "#10000000"
    // VideoInspectorFieldSurface: black at 25% (dark) or 5% (light).
    readonly property color inspectorFieldBackground: dark ? "#40000000" : "#0D000000"

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
    // The bookshelf progress strip, ShelfProgressStrip's secondary style at
    // 18% for the track and 42% for the fill.
    readonly property color shelfProgressTrack: dark ? "#1AFFFFFF" : "#17000000"
    readonly property color shelfProgressFill: dark ? "#3BFFFFFF" : "#36000000"
    // The checked segment of a segmented control, raised above its track.
    readonly property color segmentSelected: dark ? "#636366" : "#FFFFFF"
    readonly property color trackBackground: dark ? "#33FFFFFF" : "#1F000000"
    readonly property color knob: "#FFFFFF"
    // macOS systemGray, the opaque grey of filled status glyphs.
    readonly property color systemGray: "#8E8E93"
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

    // Text sizes, in pixels, at the macOS text style sizes. Pages drawn
    // before the control kit use them; a page moves to the text style tokens
    // below when it is next changed.
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

    // Font weights. Kit components and pages name weights only through these,
    // so a weight that reads too heavy in Inter changes in one place.
    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium: Font.Medium
    readonly property int weightSemibold: Font.DemiBold
    readonly property int weightBold: Font.Bold

    // Text styles of the control kit, one per SwiftUI text style, each with
    // its default weight. StyledLabel copies only size and weight, so the
    // family stays the application font.
    readonly property font largeTitleFont: Qt.font({ pixelSize: 28, weight: weightRegular })
    readonly property font title1Font: Qt.font({ pixelSize: 24, weight: weightRegular })
    readonly property font title2Font: Qt.font({ pixelSize: 19, weight: weightRegular })
    readonly property font title3Font: Qt.font({ pixelSize: 16, weight: weightRegular })
    readonly property font headlineFont: Qt.font({ pixelSize: 14, weight: weightSemibold })
    readonly property font bodyFont: Qt.font({ pixelSize: 14, weight: weightRegular })
    readonly property font calloutFont: Qt.font({ pixelSize: 13, weight: weightRegular })
    readonly property font subheadlineFont: Qt.font({ pixelSize: 12, weight: weightRegular })
    readonly property font footnoteFont: Qt.font({ pixelSize: 11, weight: weightRegular })
    readonly property font captionFont: Qt.font({ pixelSize: 11, weight: weightRegular })
    readonly property font caption2Font: Qt.font({ pixelSize: 11, weight: weightMedium })

    // Form rows. Cards follow NativeSettingsRow and NativeSettingsSubtitledLabel;
    // compact follows VideoInspectorView.labeledRow and rowLabel. Minimum
    // heights are the Swift values plus 2, glyphs plus 1 in a 20 px column.
    readonly property int rowMinHeight: 48
    readonly property int rowPadding: 16
    readonly property int rowSpacing: 12
    readonly property int rowMinGap: 20
    readonly property int rowWrappedPadding: 8
    readonly property int rowSubtitleSpacing: 2
    readonly property int compactRowMinHeight: 42
    readonly property int compactRowPadding: 12
    readonly property int compactRowSpacing: 10
    readonly property int compactRowMinGap: 8
    readonly property int compactLabelMaximumLines: 2
    readonly property int glyphSize: 13
    readonly property int glyphColumnWidth: 20
    readonly property int glyphSpacing: 8
    // A row whose control spans it, AppearanceView's Theme card picker.
    readonly property int fillRowPaddingHorizontal: 12
    readonly property int fillRowPaddingVertical: 10
    readonly property real disabledOpacity: 0.45
    readonly property real hairlineWidth: 1
    readonly property real focusRingWidth: 2

    // Sections: NativeSettingsSectionCard.cardsBody, and
    // VideoInspectorView.inspectorSection with VideoInspectorSectionGlassSurface
    // (white 6.5% over dark, white 70% over light, with a white 8% or black 6%
    // hairline).
    readonly property int sectionHeaderSpacing: 8
    readonly property int compactSectionHeaderSpacing: 7
    readonly property int sectionHeaderInset: 12
    readonly property int compactSectionHeaderInset: 4
    readonly property int sectionRadius: 18
    readonly property int compactSectionRadius: 14
    readonly property color compactSectionBackground: dark ? "#11FFFFFF" : "#B3FFFFFF"
    readonly property color compactSectionStroke: dark ? "#14FFFFFF" : "#0F000000"

    // Scrolling forms and panels: NativeSettingsForm and VideoInspectorView.body.
    readonly property int formPaddingHorizontal: 24
    readonly property int formPaddingVertical: 18
    readonly property int formSpacing: 22
    readonly property int formMaxContentWidth: 840
    readonly property int panelPaddingHorizontal: 14
    readonly property int panelPaddingTop: 14
    readonly property int panelPaddingBottom: 18
    readonly property int panelSpacing: 20

    // NativeReaderInspectorHeader with its NativeGlassCircleButton close control.
    readonly property int panelHeaderPaddingHorizontal: 18
    readonly property int panelHeaderPaddingTop: 16
    readonly property int panelHeaderPaddingBottom: 10
    readonly property int panelHeaderSpacing: 8
    readonly property int panelHeaderTitleSpacing: 2
    readonly property int closeButtonDiameter: 28
    readonly property int closeButtonGlyph: 11

    // NativeSettingsSliderRow and VideoInspectorView.sliderContent.
    readonly property int sliderRowPaddingVertical: 10
    readonly property int sliderRowSpacing: 8
    readonly property int compactSliderRowSpacing: 6

    // Capsule controls: NativeGlassMenuPicker, NativeSettingsActionButtonStyle
    // and NativeSettingsButtonRow, 2 px taller than Swift's 30 point minimum.
    readonly property int capsuleHeight: 32
    readonly property int capsuleSpacing: 8

    // Inspector chips, round glyph buttons and the icon tab strip:
    // VideoInspectorChipButtonStyle, VideoInspectorIconButtonStyle and
    // VideoInspectorTabBar.
    readonly property int chipHeight: 24
    readonly property int chipPaddingHorizontal: 8
    readonly property int chipRadius: 8
    readonly property int chipGlyphSpacing: 5
    readonly property int chipGlyphSize: 12
    readonly property int chipSpacing: 6
    readonly property int largeChipHeight: 28
    // The inset of a single control that fills a compact card.
    readonly property int compactSectionPadding: 10
    readonly property int glyphButtonDiameter: 30
    readonly property int smallGlyphButtonDiameter: 26
    readonly property int tinyGlyphButtonDiameter: 20
    readonly property int tinyGlyphSize: 10
    readonly property int tabTrackRadius: 13
    readonly property int tabTrackPadding: 3
    readonly property int tabTrackSpacing: 2
    readonly property int tabItemRadius: 10
    readonly property int tabItemPaddingVertical: 6
    readonly property int tabItemSpacing: 3
    readonly property int tabGlyphBox: 17
    readonly property int tabGlyphSize: 14
    // VideoVolumeTrack: a 64 px VideoTimelineTrack, 3 px at rest and 5 px
    // emphasized in an 18 px hit area. Swift's 11 px knob reads too small
    // with Linux scaling, so it is drawn at 14.
    readonly property int trackSliderWidth: 64
    readonly property int trackSliderHitHeight: 18
    readonly property int trackSliderHeight: 3
    readonly property int trackSliderEmphasizedHeight: 5
    readonly property int trackSliderKnob: 14
    readonly property real trackSliderFillOpacity: 0.85
    readonly property int tooltipDelay: 600
    readonly property int inlineFieldWidth: 36
    readonly property int wideInlineFieldWidth: 64
    // An empty list's centred placeholder row.
    readonly property int emptyRowHeight: 56
    // VideoInspectorEpisodeRow's number badge and playing glyph.
    readonly property int episodeBadgeMinWidth: 26
    readonly property int episodeBadgeHeight: 22
    readonly property int episodeBadgeRadius: 6
    readonly property int episodeBadgePadding: 8
    readonly property int episodeGlyphSize: 11
    // VideoInspectorView.header and the panel shell.
    readonly property int inspectorWidth: 340
    // VideoPlayerScreen.inspectorOverlayTrailingInset and VerticalInset, the
    // gap above the playback bar, and the floating bar's extra bottom inset.
    readonly property int inspectorOverlayInset: 16
    readonly property int inspectorOverlayBarGap: 8
    readonly property int inspectorFloatingBarInset: 24
    readonly property int inspectorRadius: 26
    readonly property int inspectorHeaderPadding: 14
    readonly property int inspectorHeaderPaddingBottom: 12
    readonly property int inspectorHeaderSpacing: 11
    readonly property int inspectorHeaderIconSize: 34
    readonly property int inspectorHeaderIconRadius: 9
    readonly property int inspectorHeaderGlyphSize: 15
    readonly property int inspectorCloseDiameter: 24
    readonly property int inspectorCloseGlyphSize: 11
    readonly property int inspectorTabBarInset: 12
    readonly property int inspectorTabBarPaddingBottom: 10
    // The reader's top title and progress capsule.
    readonly property int readerInfoCapsulePaddingHorizontal: 16
    readonly property int readerInfoCapsulePaddingVertical: 6
    readonly property int pickerPaddingHorizontal: 14
    readonly property int pickerGlyphSize: 12
    readonly property int pickerMinimumWidth: 96
    readonly property int buttonPaddingHorizontal: 16
    readonly property int buttonRowMinHeight: 48
    readonly property int compactButtonRowMinHeight: 36
    readonly property real buttonDisabledOpacity: 0.58
    readonly property real buttonPressedScale: 0.97

    // The toggle row's switch at the regular and small control sizes.
    readonly property int switchWidth: 34
    readonly property int switchHeight: 20
    readonly property int smallSwitchWidth: 28
    readonly property int smallSwitchHeight: 16
    readonly property int switchKnobInset: 2

    // NativeGlassSegmentedPicker.
    readonly property int segmentMinimumWidth: 76
    readonly property int segmentPaddingHorizontal: 10
    readonly property int segmentPaddingVertical: 4
    readonly property int segmentTrackPadding: 2
    readonly property int segmentDividerHeight: 16
    readonly property real segmentDividerOpacity: 0.42

    // The stepper next to a value, and the colour well of a labels-hidden
    // ColorPicker drawn over a checkerboard when the colour is translucent.
    readonly property int stepperWidth: 20
    readonly property int stepperGlyphSize: 10
    readonly property int colorWellWidth: 44
    readonly property int colorWellHeight: 24
    readonly property int colorWellRadius: 6
    readonly property int checkerboardCell: 6
    readonly property color checkerboardLight: "#FFFFFF"
    readonly property color checkerboardDark: "#C8C8C8"

    // WCAG relative luminance of an sRGB colour.
    function luminance(colour) {
        function linear(channel) {
            return channel <= 0.04045 ? channel / 12.92 : Math.pow((channel + 0.055) / 1.055, 2.4)
        }
        return 0.2126 * linear(colour.r) + 0.7152 * linear(colour.g) + 0.0722 * linear(colour.b)
    }
}
