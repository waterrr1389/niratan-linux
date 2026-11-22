// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_config.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace niratan::dictionary {
namespace {

std::optional<QJsonObject> object_from(const QByteArray& bytes) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

// Swift's Int decode: a whole JSON number inside the 64-bit range.
std::optional<std::int64_t> integer_value(const QJsonValue& value) {
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number ||
        number < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
        number > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(number);
}

std::optional<anki::DictionaryCategory> category_value(const QJsonValue& value) {
    if (value.isUndefined() || value.isNull()) {
        return anki::DictionaryCategory::none;
    }
    if (!value.isString()) {
        return std::nullopt;
    }
    const QString text = value.toString();
    if (text == "none") {
        return anki::DictionaryCategory::none;
    }
    if (text == "monolingual") {
        return anki::DictionaryCategory::monolingual;
    }
    if (text == "bilingual") {
        return anki::DictionaryCategory::bilingual;
    }
    if (text == "exclude") {
        return anki::DictionaryCategory::exclude;
    }
    return std::nullopt;
}

std::string category_name(anki::DictionaryCategory category) {
    switch (category) {
        case anki::DictionaryCategory::none:
            return "none";
        case anki::DictionaryCategory::monolingual:
            return "monolingual";
        case anki::DictionaryCategory::bilingual:
            return "bilingual";
        case anki::DictionaryCategory::exclude:
            return "exclude";
    }
    return "none";
}

std::optional<DictionaryEntry> decode_entry(const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue file_name = object.value("fileName");
    const QJsonValue is_enabled = object.value("isEnabled");
    const auto order = integer_value(object.value("order"));
    const auto category = category_value(object.value("category"));
    if (!file_name.isString() || !is_enabled.isBool() || !order || !category) {
        return std::nullopt;
    }
    return DictionaryEntry{file_name.toString().toStdString(), is_enabled.toBool(), *order,
                           *category};
}

bool decode_required(const QJsonValue& value, std::vector<DictionaryEntry>& entries) {
    if (!value.isArray()) {
        return false;
    }
    for (const QJsonValue& item : value.toArray()) {
        std::optional<DictionaryEntry> entry = decode_entry(item);
        if (!entry) {
            return false;
        }
        entries.push_back(std::move(*entry));
    }
    return true;
}

bool decode_optional(const QJsonValue& value,
                     std::optional<std::vector<DictionaryEntry>>& entries) {
    if (value.isUndefined() || value.isNull()) {
        entries = std::nullopt;
        return true;
    }
    std::vector<DictionaryEntry> decoded;
    if (!decode_required(value, decoded)) {
        return false;
    }
    entries = std::move(decoded);
    return true;
}

QJsonObject encode_entry(const DictionaryEntry& entry, bool with_category) {
    QJsonObject object;
    object.insert("fileName", QString::fromStdString(entry.file_name));
    object.insert("isEnabled", entry.is_enabled);
    object.insert("order", static_cast<qint64>(entry.order));
    if (with_category && entry.category != anki::DictionaryCategory::none) {
        object.insert("category", QString::fromStdString(category_name(entry.category)));
    }
    return object;
}

QJsonArray encode_entries(const std::vector<DictionaryEntry>& entries, bool with_category) {
    QJsonArray array;
    for (const DictionaryEntry& entry : entries) {
        array.append(encode_entry(entry, with_category));
    }
    return array;
}

}  // namespace

std::string_view dictionary_type_name(DictionaryType type) {
    switch (type) {
        case DictionaryType::term:
            return "Term";
        case DictionaryType::frequency:
            return "Frequency";
        case DictionaryType::pitch:
            return "Pitch";
        case DictionaryType::kanji:
            return "Kanji";
    }
    return {};
}

std::optional<DictionaryConfig> decode_dictionary_config(const QByteArray& bytes) {
    const std::optional<QJsonObject> object = object_from(bytes);
    if (!object) {
        return std::nullopt;
    }

    DictionaryConfig config;
    if (!decode_required(object->value("termDictionaries"), config.term_dictionaries) ||
        !decode_required(object->value("frequencyDictionaries"), config.frequency_dictionaries) ||
        !decode_required(object->value("pitchDictionaries"), config.pitch_dictionaries) ||
        !decode_optional(object->value("kanjiDictionaries"), config.kanji_dictionaries)) {
        return std::nullopt;
    }
    return config;
}

QByteArray encode_dictionary_config(const DictionaryConfig& config) {
    QJsonObject object;
    object.insert("termDictionaries", encode_entries(config.term_dictionaries, true));
    object.insert("frequencyDictionaries", encode_entries(config.frequency_dictionaries, true));
    object.insert("pitchDictionaries", encode_entries(config.pitch_dictionaries, true));
    if (config.kanji_dictionaries) {
        object.insert("kanjiDictionaries", encode_entries(*config.kanji_dictionaries, false));
    }
    return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

std::vector<DictionaryInfo> collect_dictionaries(std::vector<DictionaryInfo> stored,
                                                 const std::vector<DictionaryEntry>& configured,
                                                 bool enable_unconfigured) {
    std::vector<DictionaryEntry> ordered = configured;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const DictionaryEntry& left, const DictionaryEntry& right) {
                         return left.order < right.order;
                     });

    std::vector<DictionaryInfo> result;
    result.reserve(ordered.size() + stored.size());
    for (const DictionaryEntry& entry : ordered) {
        const auto found = std::ranges::find_if(
            stored, [&entry](const DictionaryInfo& info) { return info.title == entry.file_name; });
        if (found == stored.end()) {
            continue;
        }
        DictionaryInfo info = *found;
        info.is_enabled = entry.is_enabled;
        info.order = entry.order;
        info.category = entry.category;
        result.push_back(std::move(info));
    }

    for (DictionaryInfo& info : stored) {
        if (std::ranges::any_of(result, [&info](const DictionaryInfo& collected) {
                return collected.title == info.title;
            })) {
            continue;
        }
        info.is_enabled = enable_unconfigured;
        info.order = static_cast<std::int64_t>(result.size());
        info.category = anki::DictionaryCategory::none;
        result.push_back(std::move(info));
    }
    return result;
}

std::vector<DictionaryInfo> resolved_dictionary_list(std::vector<DictionaryInfo> stored,
                                                     const std::optional<DictionaryConfig>& config,
                                                     DictionaryType type, bool default_profile) {
    if (config.has_value()) {
        std::vector<DictionaryEntry> empty;
        const std::vector<DictionaryEntry>* configured = &empty;
        switch (type) {
            case DictionaryType::term:
                configured = &config->term_dictionaries;
                break;
            case DictionaryType::frequency:
                configured = &config->frequency_dictionaries;
                break;
            case DictionaryType::pitch:
                configured = &config->pitch_dictionaries;
                break;
            case DictionaryType::kanji:
                if (config->kanji_dictionaries) {
                    configured = &*config->kanji_dictionaries;
                }
                break;
        }
        return collect_dictionaries(std::move(stored), *configured, false);
    }

    for (DictionaryInfo& info : stored) {
        info.is_enabled = default_profile;
        info.order = 0;
        info.category = anki::DictionaryCategory::none;
    }
    return stored;
}

}  // namespace niratan::dictionary
