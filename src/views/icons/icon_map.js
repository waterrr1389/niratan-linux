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
}
