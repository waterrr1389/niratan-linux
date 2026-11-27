// SPDX-License-Identifier: GPL-3.0-or-later
//
// Dictionary types, the per-profile dictionary configuration and the resolved
// dictionary list, following Models/Dictionary.swift (DictionaryType,
// DictionaryConfig, DictionaryConfig.DictionaryEntry, DictionaryCategory,
// DictionaryInfo) and Core/DictionaryManager.swift (collectDictionaries,
// applyActiveProfileDictionaryConfiguration, loadDictionaryConfig,
// saveDictionaryConfig).
//
// DictionaryType maps to the directories "Term", "Frequency", "Pitch" and
// "Kanji" below the dictionaries root.
//
// DictionaryConfig is the stored shape of a profile's dictionary_config.json.
// term, frequency and pitch are required arrays and kanji is optional: an
// absent or null key decodes to nothing, an empty array to an engaged empty
// vector. Every entry needs a string fileName, a bool isEnabled and a whole
// JSON number order; category is optional and accepts "none", "monolingual",
// "bilingual" or "exclude", with an absent or null value meaning none. Extra
// keys are ignored at every level. A document that is not an object, a missing
// required array, a wrongly typed value or an unknown category rejects the
// whole document, as synthesized Codable decoding does.
//
// encode_dictionary_config writes the arrays in declaration order. Every entry
// carries fileName, isEnabled and order; category is written only when it is
// not none, and a kanji entry never carries one. An absent kanji array is
// omitted, as Codable omits a nil optional. Orders are written as given, not
// renumbered; callers that reorder the list set them first.
//
// collect_dictionaries mirrors collectDictionaries(storedDicts:configDicts:):
// configured entries come first, sorted by order ascending, and keep ties in
// config order. An entry is matched to an installed dictionary by directory
// name; an unmatched entry is skipped. The installed dictionary supplies the
// path, and the config supplies isEnabled, order and category. Installed
// dictionaries not named by the config follow in scan order with
// isEnabled = enable_unconfigured, category none and order = the number of
// entries already collected. Duplicate config entries naming one installed
// dictionary therefore produce one result per entry.
//
// resolved_dictionary_list is the per-type branch of
// applyActiveProfileDictionaryConfiguration: with a usable config it is
// collect_dictionaries(stored, config's array for type, false), where an
// absent kanji array behaves like an empty one; without one (missing or
// damaged file) every installed dictionary of that type is listed in scan
// order with isEnabled = default_profile, order 0 and category none. A valid
// config with an empty array is not the fallback branch.
//
// DictionaryInfo::title is the installed directory name, not index.json's
// title. The importer names the directory after index.json, so both agree for
// every dictionary this port can install; only a hand-renamed directory
// differs.

#pragma once

#include <QByteArray>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/dictionary/term_dictionary.h"

namespace niratan::dictionary {

enum class DictionaryType { term, frequency, pitch, kanji };

// DictionaryType.rawValue: "Term", "Frequency", "Pitch" or "Kanji".
std::string_view dictionary_type_name(DictionaryType type);

struct DictionaryEntry {
    std::string file_name;
    bool is_enabled = true;
    std::int64_t order = 0;
    anki::DictionaryCategory category = anki::DictionaryCategory::none;

    bool operator==(const DictionaryEntry&) const = default;
};

struct DictionaryConfig {
    std::vector<DictionaryEntry> term_dictionaries;
    std::vector<DictionaryEntry> frequency_dictionaries;
    std::vector<DictionaryEntry> pitch_dictionaries;
    std::optional<std::vector<DictionaryEntry>> kanji_dictionaries;

    bool operator==(const DictionaryConfig&) const = default;
};

struct DictionaryInfo {
    std::filesystem::path path;
    std::string title;
    bool is_enabled = true;
    std::int64_t order = 0;
    anki::DictionaryCategory category = anki::DictionaryCategory::none;

    bool operator==(const DictionaryInfo&) const = default;
};

// nullopt when the bytes are not a JSON object or do not decode as
// DictionaryConfig.
std::optional<DictionaryConfig> decode_dictionary_config(const QByteArray& bytes);

// The config as a JSON document; see the header comment for the entry shape.
QByteArray encode_dictionary_config(const DictionaryConfig& config);

std::vector<DictionaryInfo> collect_dictionaries(std::vector<DictionaryInfo> stored,
                                                 const std::vector<DictionaryEntry>& configured,
                                                 bool enable_unconfigured);

std::vector<DictionaryInfo> resolved_dictionary_list(std::vector<DictionaryInfo> stored,
                                                     const std::optional<DictionaryConfig>& config,
                                                     DictionaryType type, bool default_profile);

}  // namespace niratan::dictionary
