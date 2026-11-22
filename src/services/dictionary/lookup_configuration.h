// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary set and settings a lookup runs with, following
// Core/LookupEngine.swift (QueryConfiguration, buildQuery) and
// Core/DictionaryManager.swift (applyActiveProfileDictionaryConfiguration,
// refreshLookupQueryIfNeeded, activeLanguage).
//
// active_lookup_configuration reads the active profile, its
// dictionary_config.json and the installed typed dictionaries through
// services/dictionary. For every type it resolves the stored list with
// resolved_dictionary_list(stored, config, type, default_profile) and keeps
// only the enabled entries, in that order; nothing is sorted again. The
// fallback branch (no usable config) therefore lists every installed
// dictionary of the default profile as enabled and of any other profile as
// disabled, exactly as applyActiveProfileDictionaryConfiguration does.
//
// language_id is the profile's normalized language tag ("ja" or "en").
// frequency_sort_order is parse_frequency_sort_order of the stored string and
// frequency_sort_dictionary is the stored name or empty; both come from the
// profile's resolved dictionary settings, so an absent optional is the empty
// string and therefore automatic order.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "models/profiles/dictionary_settings.h"
#include "services/dictionary/frequency_rank.h"

namespace niratan::profiles {
class ProfileRepository;
}

namespace niratan::dictionary {

struct LookupConfiguration {
    std::vector<std::filesystem::path> term_dictionaries;
    std::vector<std::filesystem::path> frequency_dictionaries;
    std::vector<std::filesystem::path> pitch_dictionaries;
    std::vector<std::filesystem::path> kanji_dictionaries;
    std::string language_id = "ja";
    FrequencySortOrder frequency_sort_order = FrequencySortOrder::automatic;
    std::string frequency_sort_dictionary;

    bool operator==(const LookupConfiguration&) const = default;
};

// LookupFrequencySortOrder(rawValue:) ?? .auto: only "Ascending" and
// "Descending" select an order, every other string (including "Auto" and an
// empty one) is automatic.
FrequencySortOrder parse_frequency_sort_order(std::string_view stored);

LookupConfiguration
active_lookup_configuration(const profiles::ProfileRepository& repository,
                            const profiles::DictionaryProfileSettings& settings);

}  // namespace niratan::dictionary
