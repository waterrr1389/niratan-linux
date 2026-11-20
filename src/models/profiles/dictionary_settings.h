// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary lookup settings stored per profile, following
// DictionaryProfileSettings in Models/Profile.swift, the JSON persistence in
// Core/ProfileSettingsStore.swift (dictionary_settings.json) and the fallbacks
// applied by UserConfig.apply(dictionaryProfileSettings:) in Core/UserConfig.swift.
//
// The struct mirrors the Swift property names and defaults exactly. Properties
// that are optional in Swift stay optional here: documents written before a
// setting existed omit them, and the applied value then falls back in
// resolved_dictionary_settings(). The codec requires the non-optional keys and
// accepts a missing optional key as empty, which is what synthesized Codable
// does.
//
// Keys and JSON types, in Swift declaration order:
//
//   dictionaryTabDefault        Bool
//   scanNonJapaneseText         Bool
//   maxResults                  Int
//   scanLength                  Int
//   collapseMode                String, one of "Expand All", "Collapse All",
//                               "Custom"; another string decodes unchanged
//   expandFirstDictionary       Bool
//   twoColumnLayout             Bool, optional
//   compactGlossaries           Bool
//   showExpressionTags          Bool
//   harmonicFrequency           Bool
//   deduplicatePitchAccents     Bool
//   compactPitchAccents         Bool
//   customCSS                   String
//   frequencySortOrder          String, optional, one of "Auto", "Ascending",
//                               "Descending"; another string decodes unchanged
//   frequencySortDictionary     String, optional
//   searchTextSize              Int, optional
//
// The default values are DictionaryProfileSettings.defaults: the dictionary
// tab not the default tab, non-Japanese text scanned, 16 results over a 16
// character scan, "Expand All", the first dictionary not expanded, one column,
// compact glossaries, no expression tags, plain frequency ordering, dedupe and
// compact pitch accents off, empty custom CSS. defaults also fills the optional
// properties it sets explicitly (twoColumnLayout false, frequencySortOrder
// "Auto", frequencySortDictionary "", searchTextSize 26), so an empty optional
// here means the key was absent from a stored document, not that the default is
// empty. Values pass through without trimming, clamping or range checks; the
// view's stepper ranges are not part of the stored shape. An Int key must be a
// JSON number, but a fractional value or one outside the int range decodes as 0
// because QJsonValue::toInt() rejects it, as in the reader_settings codec;
// Swift's Int decode rejects the document instead.

#pragma once

#include <QJsonDocument>

#include <optional>
#include <string>

namespace niratan::profiles {

struct DictionaryProfileSettings {
    bool dictionary_tab_default = false;
    bool scan_non_japanese_text = true;
    int max_results = 16;
    int scan_length = 16;
    std::string collapse_mode = "Expand All";
    bool expand_first_dictionary = false;
    std::optional<bool> two_column_layout;
    bool compact_glossaries = true;
    bool show_expression_tags = false;
    bool harmonic_frequency = false;
    bool deduplicate_pitch_accents = false;
    bool compact_pitch_accents = true;
    std::string custom_css;
    std::optional<std::string> frequency_sort_order;
    std::optional<std::string> frequency_sort_dictionary;
    std::optional<int> search_text_size;

    bool operator==(const DictionaryProfileSettings&) const = default;
};

// DictionaryProfileSettings.defaults, which the store writes when a profile has
// no dictionary_settings.json yet. The optional properties defaults sets are
// engaged here.
DictionaryProfileSettings default_dictionary_settings();

// The settings as UserConfig.apply(dictionaryProfileSettings:) leaves them: every
// optional filled, and an unrecognised collapseMode or frequencySortOrder
// replaced by its default because both are enums in UserConfig. A present
// optional keeps its value even when false, 0 or empty; only an empty one is
// replaced, matching the ?? operator in apply. The stored document is unchanged;
// only the applied value is derived.
DictionaryProfileSettings resolved_dictionary_settings(const DictionaryProfileSettings& settings);

// nullopt when a required key is missing or wrongly typed, or when the document
// is not an object. Missing or null optional keys stay empty.
std::optional<DictionaryProfileSettings> dictionary_settings_from_json(const QJsonDocument& document);

QJsonDocument dictionary_settings_to_json(const DictionaryProfileSettings& settings);

}  // namespace niratan::profiles
