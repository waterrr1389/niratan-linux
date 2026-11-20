// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

namespace niratan::dictionary {

// Profiles carry exactly two languages (Models/Profile.swift,
// ContentLanguageProfile). Anything else falls back to Japanese, matching
// LookupEngine.buildQuery's normalizedLanguageID.
inline std::string normalize_language_id(std::string_view id) {
    if (id == "en") return "en";
    return "ja";
}

}  // namespace niratan::dictionary
