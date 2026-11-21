// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows Features/Sasayaki/SasayakiMatcher.swift (SasayakiTextMatcher.match,
// Source, fold, approximate, align, confirmsJump, fillGaps and splitLongCue)
// and Util/ReaderCharacterNormalizer.swift (filteredText).
//
// Chapters are already filtered and supplied in reading order, with their
// original spine indices. Cues stay in input order; each piece retains its
// original text and timing unless split. Match spans are chapter-local reader
// offsets, never byte offsets, and cannot cross a chapter boundary.
//
// Use locale-independent case folding equivalent to en_US_POSIX and fold
// width variants without unrelated compatibility folding; then map
// U+30A1..U+30F6 to hiragana.
// Index all five-character shingles within each chapter. Folded source
// positions retain their original reader offsets even if a fold expands.
// As in the reader normalizer, the port counts Unicode scalars rather than
// grapheme clusters; it does not require Swift's canonical equivalence.
// This applies to raw cue splitting as well: an LF in a CRLF pair is still
// a sentence boundary in the port.
//
// Split cues over 120 readable characters at sentence ends (U+3002, U+FF01,
// U+FF1F, '!', '?', newline, or '.' before whitespace or end) when the part
// has at least 20 readable characters, or at 100 readable characters.
// Merge a final remainder of fewer than eight readable characters. Number
// parts from zero as "<id>-part-<n>", trim their whitespace, and apportion
// times by raw character ranges before trimming.
//
// Skip empty filtered text, non-finite times, negative starts, non-positive
// durations, and U+FF0A-prefixed text shorter than five folded characters.
// Search forward from the last match end. The local window includes the cue
// length plus max(0, search_window), with the latter capped at 30 for cues
// shorter than six. Take the first exact match when anchored or length >= 6.
// If none is found, try up to 20% edits only when anchored and length >= 10.
// If still unmatched, allow a unique forward global exact match at length
// >= 12; only with no exact occurrence, try a seeded global approximate
// match at length >= 16 with up to 14% edits. Confirm either jump with at
// least 16 matching characters from the next eight pieces within 600 source
// characters. Confirmation looks for exact matches of length >= 6, then local
// approximate matches at length >= 12; it checks text, not cue timing.
// The final piece needs no confirmation.
//
// Align with semi-global Levenshtein distance and truncate edit budgets.
// Prefer diagonal, deletion, then insertion on ties and the earliest
// minimum-cost end within a window. Across windows, prefer fewer edits then
// earlier starts; reject a global result with a non-overlapping competitor
// within one edit of its score. Global approximate search samples cue seeds
// at stride max(1, (length - 5) / 20), ignores seeds occurring more than 200
// times, and buckets estimated starts by eight. Keep at most 12 buckets with
// at least two distinct seed offsets, ordered by votes then start. Each
// window runs from the bucket start minus the edit budget and eight to the
// bucket start plus cue length, the edit budget and 16, clipped to the
// forward search bounds and separately to each chapter.
//
// Retry gaps only between matched cues of length > 2 in the same chapter,
// with an unmatched intervening piece, at most 2,000 source characters and
// 40 pieces, valid non-marker cues, monotonic starts, and an anchor time gap
// in [0, 180] seconds. Share a 30,000,000 region-length times text-length work
// budget across gaps. Align longer cues first (input order breaks ties),
// allowing up to 55% edits at length >= 3 and exact text for shorter cues.
// Infer only remaining bounded spans of at most 160 characters and eight
// pieces, with at least one character per piece, a span-to-text ratio in
// [0.3, 2.5], matching Japanese-script classification (presence of a scalar
// in U+3000..U+9FFF) and adjacent time gaps of at most ten seconds, including
// the bounding pieces. Apportion spans by cue length and set
// context_inferred only for those fills. Keep changes only if they gain matches.

#pragma once

#include "models/sasayaki/sasayaki.h"

#include <functional>
#include <string>
#include <vector>

namespace niratan::sasayaki {

struct MatchChapter {
    int index;
    std::string text;
};

// Split cues before polling the optional callback so unmatched can count all
// pieces. After a poll reports cancellation, keep completed matches in piece
// order; discard an in-progress gap proposal. Unmatched includes every piece
// without a returned match, including unprocessed pieces.
SasayakiMatchData match_cues(const std::vector<SasayakiCue>& cues,
                            const std::vector<MatchChapter>& chapters,
                            int search_window,
                            const std::function<bool()>& cancelled = {});

}  // namespace niratan::sasayaki
