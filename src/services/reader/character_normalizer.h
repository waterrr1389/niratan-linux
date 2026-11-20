// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's character counting, following Util/ReaderCharacterNormalizer.swift
// (filteredText, readableCharacterCount, decodedCharacterReferences).
//
// filtered_text keeps the markup inside <body>...</body> when a body exists,
// drops rt (furigana), script and style blocks, strips tags, decodes character
// references, and finally keeps only the characters the reader counts:
// ASCII letters and digits, the CJK punctuation listed in the Swift pattern,
// hiragana, katakana, full-width letters and digits, half-width katakana, and
// unified ideographs. Spaces, line breaks and general punctuation never count.
//
// Differences from the Swift runtime, none of which change ordinary text:
//   - counts Unicode scalars rather than grapheme clusters, so a combining
//     sequence counts as its code points;
//   - \p{Radical} and \p{Unified_Ideograph} are matched by their code point
//     ranges (CJK radicals U+2E80..U+2EFF, Kangxi radicals U+2F00..U+2FDF,
//     unified ideographs U+3400..U+4DBF and U+4E00..U+9FFF, compatibility
//     ideographs U+F900..U+FAFF) instead of by Unicode property lookup;
//   - an invalid numeric character reference decodes to nothing, as in Swift.
//
// The parsing is intentionally regex based, like the Swift source; this is not
// an HTML parser and malformed markup only changes the count.

#pragma once

#include <string>
#include <string_view>

namespace niratan::reader {

std::string filtered_text(std::string_view markup);

int readable_character_count(std::string_view markup);

// Numeric references (decimal and hexadecimal) and the five named references
// the Swift decoder handles; everything else passes through unchanged.
std::string decoded_character_references(std::string_view text);

}  // namespace niratan::reader
