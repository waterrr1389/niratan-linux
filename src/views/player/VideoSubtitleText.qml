// SPDX-License-Identifier: GPL-3.0-or-later
//
// One subtitle row inside VideoSubtitleOverlay, following
// Features/Video/Subtitles/SubtitleOverlayView.swift (SubtitleCueMaskRow: the
// text, the edge recipe, the black rounded background and the mask) and
// InteractiveSubtitleTextView.swift (ClickableSubtitleTextView: the text
// bounds grown by 10 px horizontally and 8 px vertically, mouseDown to lookup
// and the lookup highlight range).
//
// The row draws the cue text with the stored font family, size (clamped
// 12...72), weight, colour and edge. The edge recipe repeats
// Core/VideoSubtitleEdgeStyle.swift VideoSubtitleEdgeRecipe.make because the
// service lives below QML: the strength is clamped 0...1, the scale is
// clamp(size / 36, 0.5, 2), the shadow radius is min(8, 6 * strength * scale)
// for Soft Shadow and the outline is min(4, 2.5 * strength * scale) for Clear
// Outline, min(2.5, 1.5 * outline) for High Contrast. Qt draws an outline at
// its own fixed width; the shadow is a blurred copy behind the text. The
// background is a black rounded rectangle (radius 8) at the clamped opacity
// unless the background is disabled.
//
// Lookup: a click inside the grown text bounds emits clicked with the UTF-16
// offset and the rectangle of the clicked character (a TextEdit mirrors the
// text for positionAt and positionToRectangle). Shift-hover updates the view
// model's shift/hover state and restarts one timer at hoverDelayMs; the timer
// rechecks pointerMoved and emits the same click. Pressing or releasing Shift
// while over the text starts or cancels that timer, as the flags monitor does.
// The lookup highlight is drawn as a coloured span over
// highlightStart/highlightLength of the cue text when the cue matches
// highlightedCueId. Stored colours are CSS-ordered #RRGGBBAA; QML and the
// rich-text parser both read #AARRGGBB, so they are reordered here.
//
// Mask: the row is blurred by the clamped blur radius or drawn at the clamped
// hidden opacity while the mask is enabled and not revealed.
//
// Recorded difference: rows are plain text with the cue's own line breaks;
// Swift's NSTextView measurement (glyph bounds, usedRect insets) is
// approximated by QML text metrics, so row heights and click offsets can
// differ by a pixel. The lookup text and the UTF-16 offset are the same.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import Niratan

Item {
    id: row

    required property var cue
    required property MpvItem engine
    required property PlayerLookupViewModel lookupModel
    property int maxWidth: 1000
    property string highlightedCueId: ""
    property var lookupHighlight: null
    property bool maskEnabled: false
    property string maskMode: "Blur"
    property real maskBlurRadius: 0
    property real maskHiddenOpacity: 0
    property bool lookupVisible: false
    property bool paused: false
    property bool shiftPressed: false
    property bool hovering: false

    readonly property bool maskRevealed: row.hovering || row.lookupVisible || row.paused
    readonly property real blurRadius: row.maskEnabled && !row.maskRevealed
        && row.maskMode === "Blur" ? Math.min(20, Math.max(0, row.maskBlurRadius)) : 0
    readonly property real hiddenOpacity: row.maskEnabled && !row.maskRevealed
        && row.maskMode === "Transparent"
        ? Math.min(1, Math.max(0, row.maskHiddenOpacity)) : 1

    readonly property var settings: row.engine.videoSettings
    readonly property real fontSize: Math.min(72, Math.max(12,
        row.settings.videoSubtitleFontSize))
    readonly property real weight: row.settings.videoSubtitleFontWeight
    // Core/VideoSubtitleEdgeStyle.swift: VideoSubtitleEdgeRecipe.make.
    readonly property var edge: {
        const strength = isFinite(row.settings.videoSubtitleEdgeStrength)
            ? Math.min(1, Math.max(0, row.settings.videoSubtitleEdgeStrength)) : 0.5
        const style = row.settings.videoSubtitleEdgeStyle
        const scale = Math.min(2, Math.max(0.5, row.fontSize / 36))
        const shadow = Math.min(8, 6 * strength * scale)
        const outline = Math.min(4, 2.5 * strength * scale)
        if (style === "off" || strength <= 0)
            return { shadow: 0, outline: 0 }
        if (style === "softShadow")
            return { shadow: shadow, outline: 0 }
        if (style === "clearOutline")
            return { shadow: 0, outline: outline }
        if (style === "highContrast")
            return { shadow: 0, outline: Math.min(2.5, outline * 1.5) }
        return { shadow: 0, outline: 0 }
    }
    readonly property real backgroundOpacity: row.settings.videoSubtitleBackgroundDisabled
        ? 0 : Math.min(1, Math.max(0, row.settings.videoSubtitleBackgroundOpacity))

    readonly property real bandInsetX: 14
    readonly property real bandInsetY: 6
    readonly property real bandWidth: Math.max(0, row.width - 2 * row.bandInsetX)
    // The text view spans the band; the glyph area is what Swift measures
    // for hit testing and the lookup anchor.
    readonly property real usedWidth: Math.min(row.bandWidth, naturalText.implicitWidth)
    readonly property real usedLeft: row.bandInsetX + (row.bandWidth - row.usedWidth) / 2
    // VideoSubtitleEdgeRecipe.layoutAllowance: the background and row keep the
    // room the outline or shadow needs.
    readonly property real bandHeight: textItem.height
        + Math.ceil(Math.max(row.edge.shadow * 2, row.edge.outline * 2))

    implicitWidth: row.maxWidth
    implicitHeight: row.bandHeight + 2 * row.bandInsetY
    opacity: row.hiddenOpacity
    Behavior on opacity {
        NumberAnimation { duration: 120; easing.type: Easing.InOutQuad }
    }
    layer.enabled: row.blurRadius > 0
    layer.effect: MultiEffect {
        blurEnabled: true
        blurMax: 20
        blur: row.blurRadius / 20
    }

    signal clicked(int utf16Offset, rect characterRect)

    // Swift's performLookup: the clicked glyph's rect in the text view,
    // grown to at least 1x1.
    function characterRect(offset) {
        const position = offsetter.positionToRectangle(offset)
        return Qt.rect(offsetter.x + position.x, offsetter.y + position.y,
                       Math.max(1, position.width), Math.max(1, position.height))
    }

    // InteractiveSubtitleTextView.handleModifierFlagsChanged: pressing Shift
    // over the text starts a hover lookup and releasing it cancels one.
    onShiftPressedChanged: {
        if (row.shiftPressed && mouseArea.containsMouse) {
            row.lookupModel.setShiftPressed(true)
            row.lookupModel.setHoverPointAvailable(true)
            hoverTimer.restart()
        } else if (!row.shiftPressed) {
            row.lookupModel.setShiftPressed(false)
            row.lookupModel.cancelShiftHover()
            hoverTimer.stop()
        }
    }

    // Settings store colours in CSS order (#RRGGBBAA); a QML color literal
    // needs #AARRGGBB for the alpha to land in the right byte.
    function qmlColor(hex) {
        return hex.length === 9 ? "#" + hex.substring(7) + hex.substring(1, 7) : hex
    }

    function escapeHtml(value) {
        return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
            .replace(/\n/g, "<br/>")
    }

    function displayText() {
        const plain = row.cue.text
        const highlight = row.lookupHighlight
        if (!highlight || row.highlightedCueId !== row.cue.id)
            return row.escapeHtml(plain)
        const start = highlight.highlightStart
        const length = highlight.highlightLength
        if (start === undefined || length === undefined || length <= 0)
            return row.escapeHtml(plain)
        const before = plain.substring(0, start)
        const matched = plain.substring(start, start + length)
        const after = plain.substring(start + length)
        return row.escapeHtml(before) + "<span style=\"background-color:"
            + row.qmlColor(row.settings.videoSubtitleLookupHighlightColor) + ";color:"
            + row.qmlColor(row.settings.videoSubtitleLookupHighlightTextColor) + "\">"
            + row.escapeHtml(matched) + "</span>" + row.escapeHtml(after)
    }

    Rectangle {
        x: row.bandInsetX
        y: row.bandInsetY
        width: row.bandWidth
        height: row.bandHeight
        visible: row.backgroundOpacity > 0
        radius: Theme.subtitleBandRadius
        color: Theme.videoBackground
        opacity: row.backgroundOpacity
    }

    // Unwrapped copy: measures the glyph area inside the full-width band.
    Text {
        id: naturalText
        visible: false
        text: row.cue.text
        textFormat: Text.PlainText
        font.family: row.settings.videoSubtitleFontFamily
        font.pixelSize: row.fontSize
        font.weight: row.weight
        wrapMode: Text.NoWrap
    }

    // Soft Shadow: a blurred black copy behind the text (NSShadow with a zero
    // offset and the recipe's blur radius).
    Text {
        x: row.bandInsetX
        y: row.bandInsetY
        width: row.bandWidth
        text: row.cue.text
        textFormat: Text.PlainText
        color: "black"
        visible: row.edge.shadow > 0
        font.family: row.settings.videoSubtitleFontFamily
        font.pixelSize: row.fontSize
        font.weight: row.weight
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        layer.enabled: row.edge.shadow > 0
        layer.effect: MultiEffect {
            blurEnabled: true
            blurMax: 20
            blur: Math.min(1, row.edge.shadow / 20)
        }
    }

    Text {
        id: textItem
        x: row.bandInsetX
        y: row.bandInsetY
        width: row.bandWidth
        text: row.displayText()
        textFormat: Text.RichText
        color: row.qmlColor(row.settings.videoSubtitleColor)
        font.family: row.settings.videoSubtitleFontFamily
        font.pixelSize: row.fontSize
        font.weight: row.weight
        style: row.edge.outline > 0 ? Text.Outline : Text.Normal
        styleColor: "black"
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
    }

    // Mirrors the drawn text so a point maps to a UTF-16 offset; QML text has
    // no character hit test of its own.
    TextEdit {
        id: offsetter
        x: textItem.x
        y: textItem.y
        width: textItem.width
        height: textItem.height
        visible: false
        readOnly: true
        activeFocusOnPress: false
        selectByMouse: false
        text: row.cue.text
        textFormat: TextEdit.PlainText
        wrapMode: TextEdit.Wrap
        horizontalAlignment: TextEdit.AlignHCenter
        font: textItem.font
    }

    MouseArea {
        id: mouseArea
        x: row.usedLeft - Theme.subtitleHitSlopX
        y: row.bandInsetY - Theme.subtitleHitSlopY
        width: row.usedWidth + 2 * Theme.subtitleHitSlopX
        height: textItem.height + 2 * Theme.subtitleHitSlopY
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton

        property real lastX: 0
        property real lastY: 0

        // glyphIndex(for:in:) answers the character whose box holds the
        // point; positionAt answers the nearest caret position, which is the
        // next character's for the right half of a glyph.
        // A point right of a line's end lands on that line's last character,
        // as glyphIndex answers the nearest glyph.
        function offsetAt(x, y) {
            const point = mouseArea.mapToItem(offsetter, x, y)
            let offset = Math.max(0, offsetter.positionAt(point.x, point.y))
            if (offset > 0) {
                const previous = previousScalar(offset)
                const before = offsetter.positionToRectangle(previous)
                const caret = offsetter.positionToRectangle(offset)
                const sameLine = point.y >= before.y && point.y < before.y + before.height
                if (sameLine && point.x >= before.x && (caret.y !== before.y || point.x < caret.x
                                                        || offsetter.text[offset] === "\n"))
                    offset = previous
            }
            const end = offsetter.text.length
            return offset > 0 && offset >= end ? previousScalar(end) : offset
        }

        // The UTF-16 offset of the scalar before offset, so a step back never
        // lands inside a surrogate pair.
        function previousScalar(offset) {
            return offset - (/[\uDC00-\uDFFF]/.test(offsetter.text.charAt(offset - 1)) ? 2 : 1)
        }

        Timer {
            id: hoverTimer
            interval: row.lookupModel.hoverDelayMs
            onTriggered: {
                if (!row.lookupModel.pointerMoved())
                    return
                const offset = mouseArea.offsetAt(mouseArea.lastX, mouseArea.lastY)
                row.clicked(offset, row.characterRect(offset))
            }
        }

        onClicked: function(mouse) {
            const offset = offsetAt(mouse.x, mouse.y)
            row.clicked(offset, row.characterRect(offset))
        }
        onPositionChanged: function(mouse) {
            mouseArea.lastX = mouse.x
            mouseArea.lastY = mouse.y
            row.lookupModel.setShiftPressed((mouse.modifiers & Qt.ShiftModifier) !== 0)
            row.lookupModel.setHoverPointAvailable(true)
            if (row.lookupModel.pointerMoved())
                hoverTimer.restart()
            else {
                row.lookupModel.cancelShiftHover()
                hoverTimer.stop()
            }
        }
        onExited: {
            row.lookupModel.setHoverPointAvailable(false)
            row.lookupModel.cancelShiftHover()
            hoverTimer.stop()
        }
    }

    // SubtitleCueMaskRow's .onHover covers the whole row, not just the glyphs.
    HoverHandler {
        onHoveredChanged: row.hovering = hovered
    }
}
