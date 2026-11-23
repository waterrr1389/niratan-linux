// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one mapping from Niratan's SF Symbol names to Phosphor Icons 2.1.1
// assets. Every entry is keyed by the name Swift passes to
// `Image(systemName:)` and holds the Phosphor weight and basename; a filled SF
// symbol maps to the Phosphor fill weight, and a glyph macOS draws heavy (the
// check mark) to the bold weight. Files of a weight other than regular carry
// it as a suffix, so `xmark.circle.fill` names `x-circle` and resolves to
// `fill/x-circle-fill.svg`.
// Page modules add the rows their Swift views draw.

.pragma library

var icons = {
    "books.vertical": { weight: "regular", name: "books" },
    "character.book.closed": { weight: "regular", name: "book-open-text" },
    "character.book.closed.ja": { weight: "regular", name: "translate" },
    "person.crop.circle.badge.checkmark": { weight: "regular", name: "user-circle-check" },
    "exclamationmark.bubble": { weight: "regular", name: "chat-exclamation" },
    "curlybraces": { weight: "regular", name: "brackets-curly" },
    "circle": { weight: "regular", name: "circle" },
    "play.rectangle": { weight: "regular", name: "video" },
    "play.rectangle.fill": { weight: "fill", name: "video" },
    "list.number": { weight: "regular", name: "list-numbers" },
    "captions.bubble": { weight: "regular", name: "subtitles" },
    "gearshape": { weight: "regular", name: "gear" },
    "magnifyingglass": { weight: "regular", name: "magnifying-glass" },
    "xmark.circle.fill": { weight: "fill", name: "x-circle" },
    "checklist": { weight: "regular", name: "list-checks" },
    "chart.xyaxis.line": { weight: "regular", name: "chart-line" },
    "plus": { weight: "regular", name: "plus" },
    "chevron.up.chevron.down": { weight: "regular", name: "caret-up-down" },
    "arrow.up.arrow.down": { weight: "regular", name: "arrows-down-up" },
    "square.resize": { weight: "regular", name: "arrows-out-simple" },
    "photo": { weight: "regular", name: "image" },
    "clock": { weight: "regular", name: "clock" },
    "clock.arrow.circlepath": { weight: "regular", name: "clock-counter-clockwise" },
    "textformat.size.larger.ja": { weight: "regular", name: "text-aa" },
    "line.3.horizontal": { weight: "regular", name: "list" },
    "folder": { weight: "regular", name: "folder" },
    "tray": { weight: "regular", name: "tray" },
    "tray.full": { weight: "regular", name: "tray" },
    "pencil": { weight: "regular", name: "pencil-simple" },
    "character.cursor.ibeam.ja": { weight: "regular", name: "cursor-text" },
    "checkmark.circle.fill": { weight: "fill", name: "check-circle" },
    "chevron.down": { weight: "regular", name: "caret-down" },
    "chevron.up": { weight: "regular", name: "caret-up" },
    "checkmark": { weight: "bold", name: "check" },
    // AboutView.linkRow draws it at footnote semibold.
    "arrow.up.right": { weight: "bold", name: "arrow-up-right" },
    "sidebar.leading": { weight: "regular", name: "sidebar-simple" },
    "sidebar.trailing": { weight: "regular", name: "sidebar-simple", mirror: true },
    "list.bullet": { weight: "regular", name: "list-bullets" },
    "paintpalette": { weight: "regular", name: "palette" },
    "chart.bar.xaxis": { weight: "regular", name: "chart-bar" },
    "photo.on.rectangle": { weight: "regular", name: "images" },
    "headphones": { weight: "regular", name: "headphones" },
    "waveform": { weight: "regular", name: "waveform" },
    "pause.fill": { weight: "fill", name: "pause" },
    "play.fill": { weight: "fill", name: "play" },
    "timer": { weight: "regular", name: "timer" },
    "xmark": { weight: "regular", name: "x" },
    "arrow.clockwise": { weight: "regular", name: "arrow-clockwise" },
    "highlighter": { weight: "regular", name: "highlighter" },
    "arrow.right.to.line": { weight: "regular", name: "arrow-line-right" },
    "trash": { weight: "regular", name: "trash" },
    "tray.and.arrow.down": { weight: "regular", name: "tray-arrow-down" },
    "chevron.left": { weight: "regular", name: "caret-left" },
    "chevron.right": { weight: "regular", name: "caret-right" },
    "backward.fill": { weight: "fill", name: "rewind" },
    "forward.fill": { weight: "fill", name: "fast-forward" },
    "15.arrow.trianglehead.counterclockwise": { weight: "regular", name: "clock-counter-clockwise" },
    "15.arrow.trianglehead.clockwise": { weight: "regular", name: "clock-clockwise" },
    "book": { weight: "regular", name: "book" },
    "bookmark": { weight: "regular", name: "bookmark-simple" },
    "eye.slash.fill": { weight: "fill", name: "eye-slash" },
    "play.circle": { weight: "regular", name: "play-circle" },
    "circle.dashed": { weight: "regular", name: "circle-dashed" },
    "checkmark.circle": { weight: "regular", name: "check-circle" },
    "exclamationmark.triangle": { weight: "regular", name: "warning" },
    "film.stack": { weight: "regular", name: "film-strip" },
    "backward.end.fill": { weight: "fill", name: "skip-back" },
    "forward.end.fill": { weight: "fill", name: "skip-forward" },
    "speaker.slash.fill": { weight: "fill", name: "speaker-slash" },
    "speaker.wave.1.fill": { weight: "fill", name: "speaker-low" },
    "speaker.wave.2.fill": { weight: "fill", name: "speaker-high" },
    // Phosphor draws at most two arcs; wave 2 and 3 share the loud glyph.
    "speaker.wave.3.fill": { weight: "fill", name: "speaker-high" },
    "arrows-out": { weight: "regular", name: "arrows-out" },
    "arrows-in": { weight: "regular", name: "arrows-in" },
    "film": { weight: "regular", name: "film-script" },
    "star": { weight: "regular", name: "star" },
    "star.fill": { weight: "fill", name: "star" },
    "rectangle.stack": { weight: "regular", name: "stack" },
    "square.stack": { weight: "regular", name: "stack-simple" },
    "square.grid.2x2": { weight: "regular", name: "squares-four" },
    "textformat": { weight: "regular", name: "text-t" },
    "calendar": { weight: "regular", name: "calendar-blank" },
    "folder.badge.plus": { weight: "regular", name: "folder-plus" },
    "info.circle": { weight: "regular", name: "info" },
    "backward.end": { weight: "regular", name: "skip-back" },
    "xmark.circle": { weight: "regular", name: "x-circle" },
    "rectangle.inset.filled": { weight: "regular", name: "picture-in-picture" },
    "rotate.right": { weight: "regular", name: "arrow-clockwise" },
    "repeat": { weight: "regular", name: "repeat" },
    "point.forward.to.point.capsulepath": { weight: "regular", name: "arrows-left-right" },
    "cpu": { weight: "regular", name: "cpu" },
    "sun.max.trianglebadge.exclamationmark": { weight: "regular", name: "sun-horizon" },
    "sun.max": { weight: "regular", name: "sun" },
    "circle.lefthalf.filled": { weight: "regular", name: "circle-half" },
    "drop": { weight: "regular", name: "drop" },
    "camera.filters": { weight: "regular", name: "faders" },
    "keyboard": { weight: "regular", name: "keyboard" },
    "textformat.alt": { weight: "regular", name: "text-t" },
    "textformat.size": { weight: "regular", name: "text-aa" },
    "bold": { weight: "regular", name: "text-b" },
    "character.textbox": { weight: "regular", name: "textbox" },
    "rectangle.dashed": { weight: "regular", name: "rectangle-dashed" },
    "rectangle.fill": { weight: "regular", name: "rectangle" },
    "arrow.up.and.down": { weight: "regular", name: "arrows-down-up" },
    "text.alignleft": { weight: "regular", name: "text-align-left" },
    "minus": { weight: "regular", name: "minus" },
    "chevron.left.2": { weight: "regular", name: "caret-double-left" },
    "chevron.right.2": { weight: "regular", name: "caret-double-right" },
    "arrow.counterclockwise": { weight: "regular", name: "arrow-counter-clockwise" },
    "rectangle.topthird.inset.filled": { weight: "regular", name: "align-top-simple" },
    "rectangle.bottomthird.inset.filled": { weight: "regular", name: "align-bottom-simple" },
    "ellipsis": { weight: "regular", name: "dots-three" },
}

// The bundled asset for an SF Symbol name, or "" when the table has no row.
function source(name) {
    var entry = icons[name]
    if (entry === undefined)
        return ""
    var suffix = entry.weight === "regular" ? "" : "-" + entry.weight
    return "qrc:/qt/qml/Niratan/src/views/icons/phosphor/" + entry.weight + "/"
        + entry.name + suffix + ".svg"
}
