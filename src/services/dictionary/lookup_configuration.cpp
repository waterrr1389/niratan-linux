// SPDX-License-Identifier: GPL-3.0-or-later

#include "lookup_configuration.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models/dictionary/dictionary_config.h"
#include "models/profiles/profile_index.h"
#include "services/dictionary/dictionary_config_store.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

namespace niratan::dictionary {
namespace {

std::vector<std::filesystem::path> enabled_paths(const std::optional<DictionaryConfig>& config,
                                                 DictionaryType type, bool default_profile) {
    const auto stored = DictionaryStore(storage::dictionaries_root()).installed(type);
    const auto resolved = resolved_dictionary_list(stored, config, type, default_profile);
    std::vector<std::filesystem::path> paths;
    paths.reserve(resolved.size());
    for (const DictionaryInfo& dictionary : resolved) {
        if (dictionary.is_enabled) {
            paths.push_back(dictionary.path);
        }
    }
    return paths;
}

}  // namespace

FrequencySortOrder parse_frequency_sort_order(std::string_view stored) {
    if (stored == "Ascending") {
        return FrequencySortOrder::ascending;
    }
    if (stored == "Descending") {
        return FrequencySortOrder::descending;
    }
    return FrequencySortOrder::automatic;
}

LookupConfiguration
active_lookup_configuration(const profiles::ProfileRepository& repository,
                            const profiles::DictionaryProfileSettings& settings) {
    const profiles::HoshiProfile profile = repository.active_profile();
    const bool default_profile = profile.id == repository.index().default_profile_id;

    std::optional<DictionaryConfig> config;
    if (const std::optional<std::filesystem::path> directory =
            repository.profile_directory(profile.id)) {
        config = load_dictionary_config(*directory / "dictionary_config.json");
    }

    LookupConfiguration configuration;
    configuration.term_dictionaries = enabled_paths(config, DictionaryType::term, default_profile);
    configuration.frequency_dictionaries =
        enabled_paths(config, DictionaryType::frequency, default_profile);
    configuration.pitch_dictionaries =
        enabled_paths(config, DictionaryType::pitch, default_profile);
    configuration.kanji_dictionaries =
        enabled_paths(config, DictionaryType::kanji, default_profile);
    configuration.language_id = profiles::language_id(profile.language());
    configuration.frequency_sort_order =
        parse_frequency_sort_order(settings.frequency_sort_order.value_or(""));
    configuration.frequency_sort_dictionary = settings.frequency_sort_dictionary.value_or("");
    return configuration;
}

}  // namespace niratan::dictionary
