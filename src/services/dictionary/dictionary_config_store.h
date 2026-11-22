// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reading and writing a profile's dictionary files, following
// Core/DictionaryManager.swift (loadDictionaryConfig, loadCollapsedDictionaries,
// saveDictionaryConfig, saveCollapsedDictionaries, toggleCollapsedDictionary)
// and Core/ProfileRepository.swift (dictionaryConfigURL,
// collapsedDictionariesURL).
//
// A missing, unreadable or invalid file is no config (nullopt) and the empty
// collapsed set, never an error; Swift's `try?` and its catch read the same
// way. The writers create the parent directory when missing and replace the
// target atomically, as Swift writes with `.atomic`; a failed write returns
// false and leaves any previous file in place. The profile directory is what
// the caller passes, so no legacy Dictionaries/config.json mirror is written.
//
// toggle_collapsed_dictionary loads the set, inserts or erases the title and
// saves it. It returns the resulting set even when the write failed, matching
// Swift's in-memory mutation before saveCollapsedDictionaries.

#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <string_view>

#include "models/dictionary/dictionary_config.h"

namespace niratan::dictionary {

std::optional<DictionaryConfig> load_dictionary_config(const std::filesystem::path& path);

bool save_dictionary_config(const std::filesystem::path& path, const DictionaryConfig& config);

std::set<std::string> load_collapsed_dictionaries(const std::filesystem::path& path);

bool save_collapsed_dictionaries(const std::filesystem::path& path,
                                 const std::set<std::string>& titles);

// The set after the toggle, whether or not saving it succeeded.
std::set<std::string> toggle_collapsed_dictionary(const std::filesystem::path& path,
                                                  std::string_view title);

}  // namespace niratan::dictionary
