// SPDX-License-Identifier: GPL-3.0-or-later
//
// Profile and installed-catalog I/O for DictionarySettingsViewModel, following
// Core/DictionaryManager.swift loadDictionaries and
// applyActiveProfileDictionaryConfiguration. The returned context owns the
// resolved lists for every type. These functions create no QObject and are
// used both by the synchronous commands and by the initial worker load; the
// view model applies their results on the GUI thread.

#pragma once

#include <optional>
#include <vector>

#include "models/dictionary/dictionary_config.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"

namespace niratan::viewmodels::dictionary_settings {

struct Lists {
    std::vector<niratan::dictionary::DictionaryInfo> term_dictionaries;
    std::vector<niratan::dictionary::DictionaryInfo> frequency_dictionaries;
    std::vector<niratan::dictionary::DictionaryInfo> pitch_dictionaries;
    std::vector<niratan::dictionary::DictionaryInfo> kanji_dictionaries;
};

const std::vector<niratan::dictionary::DictionaryInfo>&
list_for(const Lists& lists, niratan::dictionary::DictionaryType type);
std::vector<niratan::dictionary::DictionaryInfo>&
mutable_list_for(Lists& lists, niratan::dictionary::DictionaryType type);

struct Context {
    niratan::profiles::ProfileRepository repository;
    niratan::profiles::HoshiProfile profile;
    bool default_profile = false;
    std::optional<niratan::dictionary::DictionaryConfig> config;
    Lists lists;
};

// The active profile and all four resolved lists; nothing when the process has
// no usable data root.
std::optional<Context> load_context();

}  // namespace niratan::viewmodels::dictionary_settings
