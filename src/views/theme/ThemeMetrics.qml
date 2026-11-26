// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plain geometry tokens that Theme extends. The reader's trailing inspector
// width rule; the video playback chrome of
// Features/Video/VideoControlsView.swift (floating and compact bar, round
// buttons, timeline, speed pill and panel), the OSD of
// VideoPlayerScreen+OSD.swift and the player's empty state; the statistics
// dashboard of Features/Bookshelf/StatisticsDashboardView.swift (breakpoints,
// goal ring, week chips, ranking rows, shelf table, reading calendar, trend
// chart and the book details editor) and the minimumScaleFactor values of
// Features/Reader/StatisticsView.swift and the dashboard metrics. Views never
// use this type directly; they read every token through the Theme singleton.
// Tokens derived from colours or from the shared spacing scale stay in
// Theme.qml.

import QtQml

QtObject {
    // Dashboard layout: the two-column breakpoint, the narrowest content
    // column, metric tiles, the goal ring, week chips and empty placeholders.
    readonly property int dashboardWideWidth: 840
    readonly property int dashboardMinimumWidth: 240
    readonly property int metricTileMinimumWidth: 96
    readonly property int goalRingSize: 108
    readonly property int goalRingLineWidth: 13
    readonly property int weekChipHeight: 50
    readonly property int weekChipSpacing: 6
    readonly property int weekChipRadius: 10
    readonly property real weekChipTodayStroke: 1.5
    readonly property real weekChipMetOpacity: 0.14
    readonly property real weekChipTodayOpacity: 0.5
    readonly property int dashboardPlaceholderHeight: 46
    readonly property int dashboardEmptyHeight: 100

    // Book ranking rows: cover, title and value columns.
    readonly property int rankingRowHeight: 54
    readonly property int rankingCoverWidth: 36
    readonly property int rankingCoverHeight: 48
    readonly property int rankingTitleWidth: 120
    readonly property int rankingValueWidth: 84

    // The book details editor: its size, two-column breakpoint, lists and
    // session fields.
    readonly property int bookDetailsWidth: 780
    readonly property int bookDetailsHeight: 560
    readonly property int bookDetailsTwoColumnWidth: 620
    readonly property int bookDetailsListWidth: 270
    readonly property int bookDetailsListHeight: 160
    readonly property int bookDetailsSessionsHeight: 120
    readonly property int bookDetailsCountFieldWidth: 120
    readonly property int bookDetailsTimeFieldWidth: 70

    // The trend chart: bar and axis label widths per time grain (day,
    // week, month), then the chart's own heights, radii and marks.
    readonly property var trendBarWidths: [12, 22, 28]
    readonly property var trendLabelWidths: [42, 66, 58]
    // The share of its slot a bar fills when the plot is too narrow for the
    // grain's bar width.
    readonly property real trendBarFill: 0.7
    // trendSection's filter row: 8 between controls, 5 from caption to picker.
    readonly property int trendControlSpacing: 8
    readonly property int trendControlLabelSpacing: 5
    readonly property int trendChartHeight: 224
    readonly property int trendAxisWidth: 56
    readonly property int trendLabelHeight: 22
    readonly property int trendEmptyHeight: 150
    readonly property int trendBarRadius: 5
    readonly property real trendBarOpacity: 0.86
    readonly property int trendMinimumBarHeight: 6
    readonly property int trendEmptyBarHeight: 2
    readonly property int trendLineWidth: 2
    readonly property int trendPointSize: 4

    // The shelf comparison table, in column order: shelf, books, total
    // characters, recorded characters, duration and average speed.
    readonly property var shelfTableColumnWidths: [160, 56, 112, 118, 82, 92]
    readonly property int shelfTableMinimumWidth: 680
    readonly property int tableCellPadding: 6

    // The reading calendar's day cells.
    readonly property int calendarCellSize: 12
    readonly property int calendarCellSpacing: 4
    readonly property int calendarCellRadius: 3
    readonly property int calendarSelectedStroke: 2
    readonly property real calendarRangeOpacity: 0.45
    readonly property int calendarScrollBarRoom: 18

    // StatisticsView.swift minimumScaleFactor: the session count, the hero
    // metrics and the history cells shrink to fit before they would be cut.
    readonly property real statisticsHeroMinimumScale: 0.6
    readonly property real metricMinimumScale: 0.7
    readonly property real statisticsHistoryMinimumScale: 0.75

    // Video playback chrome: the floating bar's width cap, side inset and
    // density thresholds, both layouts' heights, insets and row spacing, the
    // round buttons and their glyphs, the speed pill and the compact scrim.
    readonly property int chromeFloatingMaxWidth: 840
    readonly property int chromeFloatingSideInset: 32
    readonly property int chromeFloatingFullWidth: 680
    readonly property int chromeCompactFullWidth: 720
    readonly property int chromeCondensedWidth: 390
    readonly property int chromeFloatingHeight: 94
    readonly property int chromeCompactHeight: 88
    readonly property int chromeFloatingRadius: 24
    readonly property int chromeFloatingInset: 16
    readonly property int chromeFloatingTopInset: 12
    readonly property int chromeFloatingBottomInset: 8
    readonly property int chromeFloatingRowSpacing: 6
    readonly property int chromeCompactInset: 22
    readonly property int chromeCompactBottomInset: 12
    readonly property int chromeCompactRowSpacing: 8
    readonly property int chromeScrimOverhang: 48
    readonly property real chromeScrimMidStop: 0.55
    readonly property real chromeOverlayTextOpacity: 0.9
    readonly property int chromeButtonDiameter: 34
    readonly property int chromeToolDiameter: 32
    readonly property int chromePlayDiameter: 46
    readonly property int chromeGlyphSize: 18
    readonly property int chromeVolumeGlyphSize: 17
    readonly property int chromePlayGlyphSize: 25
    readonly property int chromeToolSpacing: 6
    readonly property int chromeVolumeSpacing: 2
    readonly property int chromeClusterSpacing: 10
    readonly property int speedPillWidth: 52
    readonly property int speedPillHeight: 28

    // The timeline: hit height, resting and emphasized track, chapter ticks,
    // the hover knob and the time label column.
    readonly property int timelineHitHeight: 18
    readonly property int timelineHeight: 4
    readonly property int timelineEmphasizedHeight: 7
    readonly property int timelineMarkerWidth: 2
    readonly property int timelineMarkerHeight: 9
    readonly property int timelineKnob: 13
    readonly property int timelineLabelWidth: 44

    // The playback-speed panel.
    readonly property int speedPanelWidth: 264
    readonly property int speedPanelPadding: 14
    readonly property int speedPanelSpacing: 10
    readonly property int speedPanelRadius: 20
    readonly property int speedPresetSpacing: 6

    // The OSD card and the player's empty state.
    readonly property int osdTopInset: 30
    readonly property int osdLeadingInset: 28
    readonly property int osdPadding: 10
    readonly property int osdSpacing: 7
    readonly property int osdRadius: 10
    readonly property int osdMeterWidth: 210
    readonly property int emptyStateWidth: 440
    readonly property int emptyStateInset: 64
    readonly property int emptyStateGlyphSize: 40

    // The study sidebar's width range, resize handle, search field floor,
    // timeline marker column and A-B loop markers, and the Video window's
    // minimum size.
    readonly property int studySidebarMinimumWidth: 320
    readonly property int studySidebarMaximumWidth: 560
    readonly property int studySearchMinimumWidth: 80
    readonly property int studyResizeActiveWidth: 2
    readonly property int studyResizeHitWidth: 10
    readonly property int studyMarkerColumnWidth: 48
    readonly property int studyLoopMarkerSize: 20
    readonly property int videoWindowMinimumWidth: 285
    readonly property int videoWindowMinimumHeight: 120
    // While the Inspector is open, Linux keeps enough height for its fixed
    // header and tabs, a scrolling viewport and the playback bar below it.
    readonly property int videoWindowInspectorMinimumHeight: 360

    // The trailing reader inspector scales with the window, after the
    // playerPanelWidth clamp of NativeReaderView.swift, so it stays usable in
    // ordinary Linux window sizes; NativeReaderSidePanel's fixed 440 is the cap.
    readonly property real readerSidePanelWidthFraction: 0.32
    readonly property int readerSidePanelMinWidth: 320
    readonly property int readerSidePanelMaxWidth: 440

    function readerSidePanelWidth(windowWidth) {
        return Math.round(Math.min(Math.max(windowWidth * readerSidePanelWidthFraction,
                                            readerSidePanelMinWidth),
                                   readerSidePanelMaxWidth));
    }

    // The subtitle overlay: side inset and gap between cues, the background
    // band's radius and the hit area's slop around the glyphs.
    readonly property int subtitleOverlayInset: 24
    readonly property int subtitleCueSpacing: 8
    readonly property int subtitleBandRadius: 8
    readonly property int subtitleHitSlopX: 10
    readonly property int subtitleHitSlopY: 8

    // The video Inspector's scroll area inset under the tab bar and above the
    // panel's rounded bottom edge.
    readonly property int inspectorScrollInset: 10

    // The video library, following VideoLibraryView.swift: poster cards
    // (VideoLibraryPosterCardView, VideoLibraryPosterArtworkView) and list
    // rows (VideoLibraryRowView) share one 16:9 thumbnail and overlay roles.
    readonly property real videoThumbnailAspect: 16 / 9
    readonly property int videoPosterDefaultWidth: 160
    readonly property int videoPosterRadius: 10
    readonly property int videoPosterOverlayInset: 8
    readonly property int videoPosterTitleSpacing: 9
    readonly property int videoPosterLineSpacing: 3
    readonly property int videoPosterTextInset: 2
    readonly property int videoPosterSelectionOutset: 4
    readonly property real videoPosterSelectionWidth: 2.5
    readonly property real videoPosterScrimShare: 0.55
    readonly property int videoFavoriteBadgeDiameter: 22
    readonly property int videoFavoriteGlyph: 10
    readonly property int videoPlaybackGlyph: 9
    readonly property int videoPlayCueDiameter: 46
    readonly property int videoPlayCueGlyph: 18
    readonly property int videoPlaceholderGlyph: 26
    readonly property int videoOverlayTrackHeight: 4
    readonly property real mediaTrackOpacity: 0.32
    readonly property int videoRowThumbnailWidth: 120
    readonly property int videoRowThumbnailRadius: 7
    readonly property int videoRowInset: 6
    readonly property int videoRowLeading: 12
    readonly property int videoRowTextSpacing: 14
    readonly property int videoRowTrailing: 12
    readonly property int videoRowStateGlyph: 12
    readonly property int videoRowSelectionRadius: 10
    readonly property real videoRowSelectionOpacity: 0.16
    readonly property int videoRowTrackHeight: 3
    // The width below which a row drops its folder and date.
    readonly property int videoRowDetailWidth: 520
    readonly property int videoMetaSpacing: 5
    // VideoLibraryPosterGridView: the gap between cards and rows, and how far
    // a card may grow past the chosen width to fill a row.
    readonly property int videoPosterSpacing: 20
    readonly property real videoPosterGrowth: 1.5
    readonly property int videoListSideInset: 10
    // VideoLibraryContentHeader: 14 above, a 22 line and 10 below.
    readonly property int videoContentHeaderHeight: 46
    // ContentUnavailableView's glyph and the width its text wraps at.
    readonly property int unavailableGlyph: 40
    readonly property int unavailableWidth: 360
    // VideoLibraryCollapsibleSectionHeader: its height, the plain header's,
    // the disclosure chevron and the collection actions button.
    readonly property int videoSectionHeaderHeight: 28
    readonly property int videoSectionTitleHeight: 22
    readonly property int videoSectionChevron: 11
    readonly property int videoSectionActionDiameter: 24

    // A sidebar column of source lists (the bookshelf and video library):
    // its title in the toolbar band, list insets and section captions.
    readonly property int sourceListToolbarHeight: 52
    readonly property int sourceListTitleInset: 16
    readonly property int sourceListTitleTrailing: 12
    readonly property int sourceListInset: 6
    readonly property int sourceListRowSpacing: 2
    readonly property int sourceListSectionGap: 14
    readonly property int sourceListFooterPadding: 10

    // LibraryCoverSizeButton's popover and the library toolbar it hangs from.
    readonly property int coverSizeSpacing: 10
    readonly property int coverSizeSmallGlyph: 11
    readonly property int coverSizeLargeGlyph: 17
    readonly property int coverSizeSliderWidth: 180
    readonly property int coverSizeMinimum: 110
    readonly property int coverSizeMaximum: 240
    readonly property int coverSizeStep: 10
    readonly property int popoverRadius: 10
    // The dimming behind an in-page alert.
    readonly property real alertScrimOpacity: 0.7
    readonly property int popoverPadding: 16
    readonly property int popoverOffset: 6
    readonly property int libraryToolbarInset: 12
    readonly property int librarySearchMinimumWidth: 140
    readonly property int librarySearchMaximumWidth: 260
    readonly property real librarySearchShare: 0.32

    // The bookshelf card (BookView.swift ShelfBookCard, ShelfCoverFrame,
    // ShelfProgressStrip): a 3 px frame around the portrait cover, the
    // progress strip under it and a two-line title box.
    readonly property int bookCoverDefaultWidth: 160
    readonly property int bookFrameInset: 3
    readonly property real bookCoverAspect: 0.709
    readonly property int bookFrameRadius: 7
    readonly property int bookCoverRadius: 6
    readonly property int bookStripHeight: 10
    readonly property int bookStripInset: 5
    readonly property int bookStripTrackHeight: 3
    readonly property int bookTitleSpacing: 6
    readonly property int bookTitleHeight: 40
    readonly property int bookBadgeInset: 6
    readonly property real bookShadowOpacity: 0.3
    // Covers narrower than this use the small title style.
    readonly property int bookCompactWidth: 140
    readonly property int bookTitleLineHeight: 19
    readonly property int bookCompactTitleLineHeight: 16
    readonly property int checkBadgeSize: 22
    // NativeBookshelfReuseView's shelf column and ShelfView's grid.
    readonly property int shelfColumnMinimumWidth: 176
    readonly property int shelfColumnMaximumWidth: 230
    readonly property real shelfColumnShare: 0.24
    readonly property int shelfGridSpacing: 20
    readonly property int shelfHeaderTopInset: 14
    readonly property int shelfHeaderHeight: 36
    readonly property int shelfEmptyMinimumHeight: 280
    readonly property int checkBadgeKnob: 16

    // The app shell (NativeMacSidebarView): the sidebar width, its brand
    // row of the 58 px app icon and name, and the section list's trailing
    // inset.
    readonly property int shellSidebarWidth: 240
    readonly property int shellBrandInset: 18
    readonly property int shellBrandIconSize: 58
    readonly property int shellBrandIconRadius: 13
    readonly property real shellBrandIconStroke: 0.5
    readonly property int shellSectionTrailing: 10

    // nativeTopInfoOverlay: 96 clear of each side, 10 below the top.
    readonly property int readerInfoSideInset: 96
    readonly property int readerInfoTopInset: 10

    // The lookup surfaces: their corner, the 48 high control bar and its
    // 52x32 controls with 16 pt glyphs. PopupView draws an 8 px corner; here
    // the popup takes the 20 px of the speed panel and the reader's side
    // panel, so it matches the other floating panels it sits beside.
    readonly property int lookupRadius: 20
    // The popup's content stays this far inside its rounded border, so the
    // rectangular web view leaves the corners round: a corner of an inset
    // d lies inside the radius-r arc when d >= r (1 - 1/sqrt 2), 5.9 for 20,
    // and one more pixel keeps it off the 1 px stroke.
    readonly property int lookupContentInset: 7
    // Both sides' insets, which the content's width and height give up.
    readonly property int lookupContentInsets: 2 * lookupContentInset
    readonly property int lookupControlBarHeight: 48
    readonly property int lookupControlWidth: 52
    readonly property int lookupControlHeight: 32
    readonly property int lookupControlGlyph: 16
    readonly property int lookupControlSpacing: 20
    readonly property int lookupCloseInset: 16

    // AnkiMiningToastView: 18 from the edges and the leading side, 14
    // vertical, a 42 tinted badge with a 24 glyph, 13 to the text, 3 between
    // its lines, a 420 cap and the 28 corner.
    readonly property int toastInset: 18
    readonly property int toastPaddingTrailing: 22
    readonly property int toastPaddingVertical: 14
    readonly property int toastSpacing: 13
    readonly property int toastLineSpacing: 3
    readonly property int toastBadgeDiameter: 42
    readonly property int toastGlyph: 24
    readonly property int toastMaximumWidth: 420
    readonly property int toastRadius: 28

    // The reader's floating side panels: 12 from the window edges, the Go To
    // panel at a fixed 440.
    readonly property int readerPanelInset: 12
    readonly property int readerGoToPanelWidth: 440

    // A titled message dialog with a button bar (the Anki settings alerts).
    readonly property int messageDialogWidth: 420
}
