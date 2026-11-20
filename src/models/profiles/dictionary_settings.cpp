// SPDX-License-Identifier: GPL-3.0-or-later
//
// Synthesized Codable conformance for DictionaryProfileSettings in
// Models/Profile.swift: every non-optional key is required and a value of the
// wrong JSON type rejects the whole document. Optional keys follow
// decodeIfPresent, so a missing or null value decodes as nil while a wrongly
// typed one still fails. Extra keys are ignored. Values pass through without
// trimming, clamping or range checks.

#include "dictionary_settings.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>

#include <optional>
#include <string>
#include <string_view>

namespace niratan::profiles {
namespace {

bool read_string(const QJsonObject& object, QLatin1StringView key, std::string& target) {
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return false;
    }
    target = value.toString().toStdString();
    return true;
}

bool read_bool(const QJsonObject& object, QLatin1StringView key, bool& target) {
    const QJsonValue value = object.value(key);
    if (!value.isBool()) {
        return false;
    }
    target = value.toBool();
    return true;
}

bool read_int(const QJsonObject& object, QLatin1StringView key, int& target) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    target = value.toInt();
    return true;
}

bool read_optional_string(const QJsonObject& object,
                          QLatin1StringView key,
                          std::optional<std::string>& target) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isString()) {
        return false;
    }
    target = value.toString().toStdString();
    return true;
}

bool read_optional_bool(const QJsonObject& object,
                        QLatin1StringView key,
                        std::optional<bool>& target) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    target = value.toBool();
    return true;
}

bool read_optional_int(const QJsonObject& object,
                       QLatin1StringView key,
                       std::optional<int>& target) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    target = value.toInt();
    return true;
}

QString text(const std::string& value) {
    return QString::fromStdString(value);
}

// The raw values of CollapseMode in Core/UserConfig.swift.
bool is_collapse_mode(std::string_view value) {
    return value == "Expand All" || value == "Collapse All" || value == "Custom";
}

// The raw values of LookupFrequencySortOrder in Core/LookupEngine.swift.
bool is_frequency_sort_order(std::string_view value) {
    return value == "Auto" || value == "Ascending" || value == "Descending";
}

}  // namespace

DictionaryProfileSettings default_dictionary_settings() {
    // DictionaryProfileSettings.defaults fills the optional properties
    // explicitly; an empty optional only means the key was absent from a stored
    // document.
    DictionaryProfileSettings settings;
    settings.dictionary_tab_default = false;
    settings.scan_non_japanese_text = true;
    settings.max_results = 16;
    settings.scan_length = 16;
    settings.collapse_mode = "Expand All";
    settings.expand_first_dictionary = false;
    settings.two_column_layout = false;
    settings.compact_glossaries = true;
    settings.show_expression_tags = false;
    settings.harmonic_frequency = false;
    settings.deduplicate_pitch_accents = false;
    settings.compact_pitch_accents = true;
    settings.custom_css.clear();
    settings.frequency_sort_order = std::string("Auto");
    settings.frequency_sort_dictionary = std::string();
    settings.search_text_size = 26;
    return settings;
}

DictionaryProfileSettings resolved_dictionary_settings(const DictionaryProfileSettings& settings) {
    // UserConfig.apply(dictionaryProfileSettings:): every optional is replaced
    // by its default when nil, and both raw strings are mapped through an enum
    // whose initializer fails for an unknown value in favour of the default.
    DictionaryProfileSettings resolved = settings;
    if (!is_collapse_mode(resolved.collapse_mode)) {
        resolved.collapse_mode = "Expand All";
    }
    if (!resolved.two_column_layout) {
        resolved.two_column_layout = false;
    }
    if (!resolved.frequency_sort_order || !is_frequency_sort_order(*resolved.frequency_sort_order)) {
        resolved.frequency_sort_order = std::string("Auto");
    }
    if (!resolved.frequency_sort_dictionary) {
        resolved.frequency_sort_dictionary = std::string();
    }
    if (!resolved.search_text_size) {
        resolved.search_text_size = 26;
    }
    return resolved;
}

std::optional<DictionaryProfileSettings> dictionary_settings_from_json(
    const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    DictionaryProfileSettings settings;
    if (!read_bool(object, QLatin1StringView("dictionaryTabDefault"),
                   settings.dictionary_tab_default) ||
        !read_bool(object, QLatin1StringView("scanNonJapaneseText"),
                   settings.scan_non_japanese_text) ||
        !read_int(object, QLatin1StringView("maxResults"), settings.max_results) ||
        !read_int(object, QLatin1StringView("scanLength"), settings.scan_length) ||
        !read_string(object, QLatin1StringView("collapseMode"), settings.collapse_mode) ||
        !read_bool(object, QLatin1StringView("expandFirstDictionary"),
                   settings.expand_first_dictionary) ||
        !read_bool(object, QLatin1StringView("compactGlossaries"), settings.compact_glossaries) ||
        !read_bool(object, QLatin1StringView("showExpressionTags"), settings.show_expression_tags) ||
        !read_bool(object, QLatin1StringView("harmonicFrequency"), settings.harmonic_frequency) ||
        !read_bool(object, QLatin1StringView("deduplicatePitchAccents"),
                   settings.deduplicate_pitch_accents) ||
        !read_bool(object, QLatin1StringView("compactPitchAccents"),
                   settings.compact_pitch_accents) ||
        !read_string(object, QLatin1StringView("customCSS"), settings.custom_css)) {
        return std::nullopt;
    }

    if (!read_optional_bool(object, QLatin1StringView("twoColumnLayout"),
                            settings.two_column_layout) ||
        !read_optional_string(object, QLatin1StringView("frequencySortOrder"),
                              settings.frequency_sort_order) ||
        !read_optional_string(object, QLatin1StringView("frequencySortDictionary"),
                              settings.frequency_sort_dictionary) ||
        !read_optional_int(object, QLatin1StringView("searchTextSize"),
                           settings.search_text_size)) {
        return std::nullopt;
    }

    return settings;
}

QJsonDocument dictionary_settings_to_json(const DictionaryProfileSettings& settings) {
    QJsonObject object;
    object.insert(QLatin1StringView("dictionaryTabDefault"), settings.dictionary_tab_default);
    object.insert(QLatin1StringView("scanNonJapaneseText"), settings.scan_non_japanese_text);
    object.insert(QLatin1StringView("maxResults"), settings.max_results);
    object.insert(QLatin1StringView("scanLength"), settings.scan_length);
    object.insert(QLatin1StringView("collapseMode"), text(settings.collapse_mode));
    object.insert(QLatin1StringView("expandFirstDictionary"), settings.expand_first_dictionary);
    if (settings.two_column_layout) {
        object.insert(QLatin1StringView("twoColumnLayout"), *settings.two_column_layout);
    }
    object.insert(QLatin1StringView("compactGlossaries"), settings.compact_glossaries);
    object.insert(QLatin1StringView("showExpressionTags"), settings.show_expression_tags);
    object.insert(QLatin1StringView("harmonicFrequency"), settings.harmonic_frequency);
    object.insert(QLatin1StringView("deduplicatePitchAccents"), settings.deduplicate_pitch_accents);
    object.insert(QLatin1StringView("compactPitchAccents"), settings.compact_pitch_accents);
    object.insert(QLatin1StringView("customCSS"), text(settings.custom_css));
    if (settings.frequency_sort_order) {
        object.insert(QLatin1StringView("frequencySortOrder"), text(*settings.frequency_sort_order));
    }
    if (settings.frequency_sort_dictionary) {
        object.insert(QLatin1StringView("frequencySortDictionary"),
                      text(*settings.frequency_sort_dictionary));
    }
    if (settings.search_text_size) {
        object.insert(QLatin1StringView("searchTextSize"), *settings.search_text_size);
    }
    return QJsonDocument(object);
}

}  // namespace niratan::profiles
