// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video subtitle overlay, following Features/Video/Subtitles/
// SubtitleOverlayView.swift (SubtitleOverlayView.body, SubtitleCueMaskRow),
// SubtitleVerticalPositionLayout.swift (placeSubviews),
// Core/VideoSubtitlePositionPolicy.swift (normalized, originY),
// InteractiveSubtitleTextView.swift (ClickableSubtitleTextView and its hit
// testing), VideoPlayerScreen.swift (the overlay instance: frame, position,
// zIndex 2) and the appearance keys of Core/UserConfig.swift.
//
// Rows: one VideoSubtitleText per current cue in cue order, 8 px between
// rows and 24 px at both sides of the picture area. The block's top is
// max(0, viewportHeight - blockHeight) * clamp(verticalPositionFraction,
// 0, 1); a non-finite or negative height yields 0. While the playback bar
// covers `bottomObstruction` at the bottom, the block is kept above it. With no cues the column is
// empty and takes no clicks; the page hides the overlay when subtitles are
// hidden or no text document is loaded.
//
// Mask: with the mask enabled, the Blur style applies the clamped blur radius
// and Transparent applies the clamped hidden opacity while no row is hovered,
// no popup is open and playback is running; otherwise the rows are sharp and
// fully opaque.
//
// Lookup: a row click or Shift-hover emits lookupRequested with the cue id,
// the UTF-16 offset and the clicked text rectangle in overlay coordinates;
// the page performs the lookup, owns the popup and sets highlightedCueId and
// lookupHighlight from the result.

pragma ComponentBehavior: Bound

import QtQuick
import Niratan

Item {
    id: overlay

    required property MpvItem engine
    required property PlayerLookupViewModel lookupModel
    property bool paused: false
    // Height at the bottom that the visible playback bar covers. A block that
    // would reach into it is lifted just above it; one already clear stays
    // put, so the subtitles do not jump each time the bar shows. Swift's
    // translucent bar has no such rule; it hides on its own.
    property real bottomObstruction: 0
    property bool shiftPressed: false
    property string highlightedCueId: ""
    // The lookupAt result: highlightStart and highlightLength, or null.
    property var lookupHighlight: null

    readonly property var cues: overlay.lookupModel.currentCues
    readonly property var settings: overlay.engine.videoSettings
    readonly property real verticalPosition: Math.min(1,
        Math.max(0, overlay.settings.videoSubtitleVerticalPositionFraction))

    signal lookupRequested(string cueId, int utf16Offset, rect anchor)

    Column {
        id: column
        x: Theme.subtitleOverlayInset
        width: Math.max(0, overlay.width - column.x - Theme.subtitleOverlayInset)
        readonly property real placedY: Math.max(0, overlay.height - column.implicitHeight)
                                        * overlay.verticalPosition
        y: Math.max(0, Math.min(placedY,
                                overlay.height - overlay.bottomObstruction - column.implicitHeight))
        spacing: Theme.subtitleCueSpacing

        Repeater {
            model: overlay.cues

            delegate: VideoSubtitleText {
                required property var modelData

                cue: modelData
                engine: overlay.engine
                lookupModel: overlay.lookupModel
                maxWidth: column.width
                highlightedCueId: overlay.highlightedCueId
                lookupHighlight: overlay.lookupHighlight
                maskEnabled: overlay.settings.videoSubtitleMaskEnabled === true
                maskMode: overlay.settings.videoSubtitleMaskMode
                maskBlurRadius: overlay.settings.videoSubtitleMaskBlurRadius
                maskHiddenOpacity: overlay.settings.videoSubtitleMaskHiddenOpacity
                lookupVisible: overlay.lookupModel.hasPopup
                paused: overlay.paused
                shiftPressed: overlay.shiftPressed
                width: column.width

                onClicked: function(utf16Offset, characterRect) {
                    const point = mapToItem(overlay, characterRect.x, characterRect.y)
                    overlay.lookupRequested(cue.id, utf16Offset,
                        Qt.rect(point.x, point.y, characterRect.width, characterRect.height))
                }
            }
        }
    }
}
