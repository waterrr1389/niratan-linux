// SPDX-License-Identifier: GPL-3.0-or-later
//
// The profile content language and its character counting, following
// Models/Profile.swift (ContentLanguageProfile: normalize(_:),
// displayCount(forRawCharacters:), rawCharacters(forDisplayCount:)).
//
// Only Japanese ("ja") and English ("en") exist. normalize_language trims
// Unicode whitespace and newlines, lowercases, then keeps the text before the
// first "-" or "_" as the primary tag; leading separators are skipped, and an
// empty or unknown tag has no language. It returns nullopt for a missing
// value, exactly as normalize(_:) does.
//
// display_count is the Japanese raw count, or ceil(count / 5) for English;
// negative input clamps to zero. raw_characters is the inverse: the Japanese
// count, or five times the English display count, also clamped at zero.
// Lowercasing follows Qt's Unicode mapping; Swift differs only for rare
// scalars such as a word-final capital sigma.

#pragma once

#include <optional>
#include <string_view>

namespace niratan::profiles {

enum class ContentLanguage { japanese, english };

// "ja" or "en", the Swift raw values.
std::string_view language_id(ContentLanguage language);

// nullopt when the tag is missing, empty or not ja/en.
std::optional<ContentLanguage> normalize_language(std::string_view language);

int display_count(ContentLanguage language, int raw_characters);

int raw_characters(ContentLanguage language, int display_count);

}  // namespace niratan::profiles
