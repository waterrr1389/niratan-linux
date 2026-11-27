// SPDX-License-Identifier: GPL-3.0-or-later
//
// The active profile's ordered term dictionaries, following
// Core/DictionaryManager.swift (applyActiveProfileDictionaryConfiguration,
// collectDictionaries, loadDictionaryConfig, scanPhysicalDictionaryCatalog),
// Models/Dictionary.swift (DictionaryConfig, DictionaryInfo,
// DictionaryCategory) and Core/AnkiManager.swift (categoryGlossary,
// handlebarToValue), which consume the list by title and category.
//
// installed is the physical term dictionary scan in the order it was returned
// (dictionary_store::installed()); the title is the directory name, which the
// importer also uses as the config fileName and the popup uses as the
// singleGlossaries key. An installed directory without a readable index.json
// is left out of the list, as getDictionariesFromStorage omits it; nothing is
// deleted and the index title is not used. config_path is the active profile's
// dictionary_config.json; the caller resolves it per profile.
//
// Configured entries come first, sorted by order ascending; equal orders keep
// their config file order because Swift's order is undefined there. An entry
// whose fileName has no installed directory is skipped; installed directories
// not named by the config follow in installed order. Disabled entries stay in
// the list because the category fallback does not filter isEnabled; a disabled
// dictionary is skipped only when its popup glossary is absent. The fallback
// skips any title whose popup glossary is absent or empty and tries the next
// dictionary of that category, while {single-glossary-<title>} is empty for an
// unknown title.
//
// The whole file is decoded like Swift's `try? JSONDecoder()`.
// termDictionaries, frequencyDictionaries and pitchDictionaries are required
// arrays and kanjiDictionaries is optional; every entry needs a string
// fileName, a bool isEnabled and an integer order, and an optional category of
// "none", "monolingual", "bilingual" or "exclude". An absent or null category
// is none. Any bad JSON, wrong type or unknown category invalidates the file.
//
// A missing, unreadable or invalid file takes the no-config branch: installed
// directories in the given order, all with category none and no configured
// title. Each item carries only the title and category; isEnabled, order and
// the path stay out of TermDictionary.

#pragma once

#include <filesystem>
#include <vector>

#include "models/dictionary/term_dictionary.h"

namespace niratan::profiles {
class ProfileRepository;
}

namespace niratan::dictionary {

std::vector<anki::TermDictionary>
term_dictionaries(const std::vector<std::filesystem::path>& installed,
                  const std::filesystem::path& config_path);

// Resolve the current profile's configuration and scan the shared term
// dictionary store, as applyActiveProfileDictionaryConfiguration does.
std::vector<anki::TermDictionary>
active_term_dictionaries(const profiles::ProfileRepository& repository);

}  // namespace niratan::dictionary
