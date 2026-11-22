// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/term_dictionaries.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QJsonValue>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "models/dictionary/dictionary_config.h"
#include "services/dictionary/dictionary_config_store.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

namespace niratan::dictionary {
namespace {

// getDictionariesFromStorage loads DictionaryIndex from the directory. Only
// readability matters here; the title still comes from the directory name.
bool readable_index(const std::filesystem::path& directory) {
    QFile file(QString::fromStdString((directory / "index.json").string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    const QJsonObject index = document.object();
    const QJsonValue format = index.value("format");
    const double number = format.toDouble();
    const bool whole = format.isDouble() && std::isfinite(number) && std::trunc(number) == number &&
        number >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
        number <= static_cast<double>(std::numeric_limits<std::int64_t>::max());
    return index.value("title").isString() && whole && index.value("revision").isString() &&
        index.value("isUpdatable").isBool() && index.value("indexUrl").isString() &&
        index.value("downloadUrl").isString();
}

std::string directory_name(const std::filesystem::path& directory) {
    return directory.filename().string();
}

std::vector<anki::TermDictionary> as_term_dictionaries(const std::vector<DictionaryInfo>& stored) {
    std::vector<anki::TermDictionary> result;
    result.reserve(stored.size());
    for (const DictionaryInfo& dictionary : stored) {
        result.push_back({dictionary.title, dictionary.category});
    }
    return result;
}

}  // namespace

std::vector<anki::TermDictionary>
term_dictionaries(const std::vector<std::filesystem::path>& installed,
                  const std::filesystem::path& config_path) {
    std::vector<DictionaryInfo> stored;
    stored.reserve(installed.size());
    for (const std::filesystem::path& directory : installed) {
        if (readable_index(directory)) {
            stored.push_back(DictionaryInfo{directory, directory_name(directory)});
        }
    }

    const std::optional<DictionaryConfig> config = load_dictionary_config(config_path);
    if (config.has_value()) {
        return as_term_dictionaries(
            collect_dictionaries(std::move(stored), config->term_dictionaries, false));
    }
    return as_term_dictionaries(
        resolved_dictionary_list(std::move(stored), std::nullopt, DictionaryType::term, false));
}

std::vector<anki::TermDictionary>
active_term_dictionaries(const profiles::ProfileRepository& repository) {
    const profiles::HoshiProfile profile = repository.active_profile();
    const std::optional<std::filesystem::path> directory = repository.profile_directory(profile.id);
    if (!directory.has_value()) {
        return {};
    }

    const std::optional<DictionaryConfig> config =
        load_dictionary_config(*directory / "dictionary_config.json");
    const bool default_profile = profile.id == repository.index().default_profile_id;
    return as_term_dictionaries(resolved_dictionary_list(
        DictionaryStore(storage::dictionaries_root()).installed(DictionaryType::term), config,
        DictionaryType::term, default_profile));
}

}  // namespace niratan::dictionary
