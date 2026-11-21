// SPDX-License-Identifier: GPL-3.0-or-later
//
// Chapter selection for Sasayaki matching, following
// Features/Sasayaki/SasayakiMatcher.swift (SasayakiMatcher.match(rootURL:):
// guideTocPaths and the document.spine.items loop) and EPUBKit's spine,
// manifest and guide parsers.
//
// The spine is walked in order and each item keeps its vector position as the
// chapter index, which is also the reader's index (Decision D11 in
// notes/specs/sasayaki-gap.md). An item is skipped when it is not linear
// ((linear ?? "yes") == "yes"), when its raw manifest properties contain the
// substring "nav", when its raw manifest href is in the guide-toc set, or when
// its entry cannot be read. Positions are never renumbered.
//
// The guide-toc set holds the href of every guide reference whose type
// lowercased equals "toc", cut at the first '#'. Swift's split drops empty
// subsequences, so a leading '#' yields the text after it, while an empty href
// or a lone "#" falls back to the whole href. Membership compares the raw
// manifest href exactly, without percent-decoding, "./" removal or path
// resolution; other reference types skip nothing.
//
// The selected text is reader::filtered_text of the entry's bytes. The
// optional callback is polled before each chapter read; a cancelled call
// returns the chapters selected so far, which the caller discards. Invalid
// UTF-8 is decoded with U+FFFD and the chapter is kept, unlike Swift's
// String(contentsOf:encoding:.utf8) which skips it (Decision D12).

#pragma once

#include <functional>
#include <vector>

#include "services/novels/epub_book.h"
#include "services/sasayaki/matcher.h"

namespace niratan::sasayaki {

std::vector<MatchChapter> match_chapters(const novels::EpubBook& book,
                                         const std::function<bool()>& cancelled = {});

}  // namespace niratan::sasayaki
