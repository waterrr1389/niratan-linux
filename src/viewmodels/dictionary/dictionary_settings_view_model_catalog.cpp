// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_settings_view_model_catalog.h"

#include <utility>

#include "services/dictionary/dictionary_config_store.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels::dictionary_settings {

const std::vector<niratan::dictionary::DictionaryInfo>&
list_for(const Lists& lists, niratan::dictionary::DictionaryType type) {
    using niratan::dictionary::DictionaryType;
    switch (type) {
        case DictionaryType::term:
            return lists.term_dictionaries;
        case DictionaryType::frequency:
            return lists.frequency_dictionaries;
        case DictionaryType::pitch:
            return lists.pitch_dictionaries;
        case DictionaryType::kanji:
            return lists.kanji_dictionaries;
    }
    return lists.term_dictionaries;
}

std::vector<niratan::dictionary::DictionaryInfo>&
mutable_list_for(Lists& lists, niratan::dictionary::DictionaryType type) {
    using niratan::dictionary::DictionaryType;
    switch (type) {
        case DictionaryType::term:
            return lists.term_dictionaries;
        case DictionaryType::frequency:
            return lists.frequency_dictionaries;
        case DictionaryType::pitch:
            return lists.pitch_dictionaries;
        case DictionaryType::kanji:
            return lists.kanji_dictionaries;
    }
    return lists.term_dictionaries;
}

namespace {

Lists resolve_lists(const std::optional<niratan::dictionary::DictionaryConfig>& config,
                    bool default_profile) {
    using niratan::dictionary::DictionaryStore;
    using niratan::dictionary::DictionaryType;
    const DictionaryStore store(niratan::storage::dictionaries_root());
    Lists lists;
    lists.term_dictionaries = niratan::dictionary::resolved_dictionary_list(
        store.installed(DictionaryType::term), config, DictionaryType::term, default_profile);
    lists.frequency_dictionaries = niratan::dictionary::resolved_dictionary_list(
        store.installed(DictionaryType::frequency), config, DictionaryType::frequency,
        default_profile);
    lists.pitch_dictionaries = niratan::dictionary::resolved_dictionary_list(
        store.installed(DictionaryType::pitch), config, DictionaryType::pitch, default_profile);
    lists.kanji_dictionaries = niratan::dictionary::resolved_dictionary_list(
        store.installed(DictionaryType::kanji), config, DictionaryType::kanji, default_profile);
    return lists;
}

}  // namespace

std::optional<Context> load_context() {
    std::optional<niratan::profiles::ProfileRepository> repository =
        niratan::profiles::ProfileRepository::at_data_root();
    if (!repository.has_value()) {
        return std::nullopt;
    }
    const niratan::profiles::HoshiProfile profile = repository->active_profile();
    const bool default_profile = profile.id == repository->index().default_profile_id;

    std::optional<niratan::dictionary::DictionaryConfig> config;
    if (const auto directory = repository->profile_directory(profile.id)) {
        config = niratan::dictionary::load_dictionary_config(*directory / "dictionary_config.json");
    }

    Context context{std::move(*repository), profile, default_profile, std::move(config), {}};
    context.lists = resolve_lists(context.config, context.default_profile);
    return context;
}

}  // namespace niratan::viewmodels::dictionary_settings
