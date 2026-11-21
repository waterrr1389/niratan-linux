// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/term_dictionaries.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

namespace niratan::dictionary {
namespace {

using niratan::anki::DictionaryCategory;

struct ConfigEntry {
    std::string file_name;
    qint64 order = 0;
    DictionaryCategory category = DictionaryCategory::none;
};

std::optional<QJsonDocument> read_json(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        return std::nullopt;
    }
    return document;
}

// Swift's Int decode: a whole JSON number inside the 64-bit range.
std::optional<qint64> integer_value(const QJsonValue& value) {
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number ||
        number < static_cast<double>(std::numeric_limits<qint64>::min()) ||
        number > static_cast<double>(std::numeric_limits<qint64>::max())) {
        return std::nullopt;
    }
    return static_cast<qint64>(number);
}

std::optional<DictionaryCategory> category_value(const QJsonValue& value) {
    if (value.isUndefined() || value.isNull()) {
        return DictionaryCategory::none;
    }
    if (!value.isString()) {
        return std::nullopt;
    }
    const QString text = value.toString();
    if (text == "none") {
        return DictionaryCategory::none;
    }
    if (text == "monolingual") {
        return DictionaryCategory::monolingual;
    }
    if (text == "bilingual") {
        return DictionaryCategory::bilingual;
    }
    if (text == "exclude") {
        return DictionaryCategory::exclude;
    }
    return std::nullopt;
}

// Validates one DictionaryConfig array; only term entries are kept.
bool decode_entries(const QJsonValue& value, std::vector<ConfigEntry>& terms, bool keep) {
    if (!value.isArray()) {
        return false;
    }
    for (const QJsonValue& item : value.toArray()) {
        if (!item.isObject()) {
            return false;
        }
        const QJsonObject entry = item.toObject();
        const QJsonValue file_name = entry.value("fileName");
        const QJsonValue is_enabled = entry.value("isEnabled");
        const auto order = integer_value(entry.value("order"));
        const auto category = category_value(entry.value("category"));
        if (!file_name.isString() || !is_enabled.isBool() || !order || !category) {
            return false;
        }
        if (keep) {
            terms.push_back({file_name.toString().toStdString(), *order, *category});
        }
    }
    return true;
}

// DictionaryConfig Codable: three required arrays and an optional kanji array.
std::optional<std::vector<ConfigEntry>> decode_config(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    std::vector<ConfigEntry> terms;
    if (!decode_entries(object.value("termDictionaries"), terms, true) ||
        !decode_entries(object.value("frequencyDictionaries"), terms, false) ||
        !decode_entries(object.value("pitchDictionaries"), terms, false)) {
        return std::nullopt;
    }
    const QJsonValue kanji = object.value("kanjiDictionaries");
    if (!kanji.isUndefined() && !kanji.isNull() && !decode_entries(kanji, terms, false)) {
        return std::nullopt;
    }
    return terms;
}

// getDictionariesFromStorage loads DictionaryIndex from the directory. Only
// readability matters here; the title still comes from the directory name.
bool readable_index(const std::filesystem::path& directory) {
    const auto document = read_json(directory / "index.json");
    if (!document || !document->isObject()) {
        return false;
    }
    const QJsonObject index = document->object();
    return index.value("title").isString() && integer_value(index.value("format")) &&
        index.value("revision").isString() && index.value("isUpdatable").isBool() &&
        index.value("indexUrl").isString() && index.value("downloadUrl").isString();
}

std::string directory_name(const std::filesystem::path& directory) {
    return directory.filename().string();
}

}  // namespace

std::vector<anki::TermDictionary>
term_dictionaries(const std::vector<std::filesystem::path>& installed,
                  const std::filesystem::path& config_path) {
    std::vector<std::filesystem::path> stored;
    stored.reserve(installed.size());
    for (const std::filesystem::path& directory : installed) {
        if (readable_index(directory)) {
            stored.push_back(directory);
        }
    }

    std::vector<ConfigEntry> configured;
    if (const auto document = read_json(config_path)) {
        if (auto decoded = decode_config(*document)) {
            configured = std::move(*decoded);
        }
    }
    std::stable_sort(
        configured.begin(), configured.end(),
        [](const ConfigEntry& left, const ConfigEntry& right) { return left.order < right.order; });

    std::vector<anki::TermDictionary> result;
    std::set<std::string> matched;
    for (const ConfigEntry& entry : configured) {
        const auto found = std::ranges::find_if(stored, [&entry](const auto& directory) {
            return directory_name(directory) == entry.file_name;
        });
        if (found == stored.end()) {
            continue;
        }
        result.push_back({directory_name(*found), entry.category});
        matched.insert(entry.file_name);
    }
    for (const std::filesystem::path& directory : stored) {
        const std::string name = directory_name(directory);
        if (!matched.contains(name)) {
            result.push_back({name, DictionaryCategory::none});
        }
    }
    return result;
}

std::vector<anki::TermDictionary>
active_term_dictionaries(const profiles::ProfileRepository& repository) {
    const auto directory = repository.profile_directory(repository.active_profile().id);
    if (!directory) {
        return {};
    }
    return term_dictionaries(DictionaryStore(storage::dictionaries_root()).installed(),
                             *directory / "dictionary_config.json");
}

}  // namespace niratan::dictionary
