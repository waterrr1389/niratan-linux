// SPDX-License-Identifier: GPL-3.0-or-later

#include "text_selection.h"

namespace niratan::dictionary {

std::optional<TextLookupCandidate> lookup_candidate(std::string_view, int, int, std::string_view) {
    return std::nullopt;
}

std::optional<HighlightRange> highlight_range(const TextLookupCandidate&, std::string_view) {
    return std::nullopt;
}

}  // namespace niratan::dictionary
