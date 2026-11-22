// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionaries Niratan recommends for a content language, following
// DictionaryRecommendation in Models/Dictionary.swift (all, forLanguage).
//
// The table is Swift's declaration order and values: twelve entries, five for
// Japanese and seven for English. Every entry has either an index URL (a
// remote Yomitan index that names a download) or a direct download URL; the
// two Leipzig frequency dictionaries are the direct-download ones. In-app
// downloads are out of scope (Decision O3): the settings page shows the table
// but imports local zips only, so the URLs are data, not actions.
//
// dictionary_recommendations() returns the full table; forLanguage keeps the
// declaration order of the entries whose language matches.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "models/dictionary/dictionary_config.h"
#include "models/profiles/content_language.h"

namespace niratan::dictionary {

struct DictionaryRecommendation {
    std::string id;
    std::string name;
    DictionaryType type = DictionaryType::term;
    std::optional<std::string> index_url;
    std::optional<std::string> download_url;
    profiles::ContentLanguage language = profiles::ContentLanguage::japanese;

    bool operator==(const DictionaryRecommendation&) const = default;
};

const std::vector<DictionaryRecommendation>& dictionary_recommendations();

std::vector<DictionaryRecommendation>
dictionary_recommendations_for(profiles::ContentLanguage language);

}  // namespace niratan::dictionary
