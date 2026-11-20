// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace niratan::dictionary {

// Mirrors Core/SelectionLookup/TextSelectionResolver.swift. Offsets count
// UTF-16 code units, because that is what the web selection bridge reports.
struct TextLookupCandidate {
    std::string text;
    int utf16_start = 0;

    bool operator==(const TextLookupCandidate&) const = default;
};

struct HighlightRange {
    int utf16_start = 0;
    int utf16_length = 0;

    bool operator==(const HighlightRange&) const = default;
};

// Japanese starts at the click. English walks back to the word start, where
// ' ’ ` - and the hyphen variants stay inside a word only when both
// neighbours are ASCII alphanumeric. Empty after trimming returns nullopt.
std::optional<TextLookupCandidate> lookup_candidate(std::string_view sentence, int utf16_offset,
                                                     int scan_length, std::string_view language_id);

// Length is the shorter of the candidate and the matched text, in UTF-16
// code units. A zero length returns nullopt.
std::optional<HighlightRange> highlight_range(const TextLookupCandidate& candidate,
                                               std::string_view matched_text);

}  // namespace niratan::dictionary
