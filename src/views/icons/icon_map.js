// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one mapping from Niratan's SF Symbol names to Phosphor Icons 2.1.1
// assets. Every entry is keyed by the name Swift passes to
// `Image(systemName:)` and holds the Phosphor weight and basename; a filled SF
// symbol maps to the Phosphor fill weight, whose files carry a `-fill` suffix,
// so `xmark.circle.fill` names `x-circle` and resolves to `x-circle-fill.svg`.
// Page modules add the rows their Swift views draw.

.pragma library

var icons = {
    "books.vertical": { weight: "regular", name: "books" },
    "character.book.closed": { weight: "regular", name: "book-open-text" },
    "play.rectangle": { weight: "regular", name: "video" },
    "gearshape": { weight: "regular", name: "gear" },
    "magnifyingglass": { weight: "regular", name: "magnifying-glass" },
    "xmark.circle.fill": { weight: "fill", name: "x-circle" },
    "checklist": { weight: "regular", name: "list-checks" },
    "chart.xyaxis.line": { weight: "regular", name: "chart-line" },
    "plus": { weight: "regular", name: "plus" },
    "sidebar.leading": { weight: "regular", name: "sidebar-simple" },
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
}
