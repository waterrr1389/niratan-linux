// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_store.h"

#include <hoshidicts/importer.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>

#include <cmath>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace niratan::dictionary {
namespace {

std::filesystem::path temporary_sibling(const std::filesystem::path& root) {
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<unsigned> distribution(0, 0xffffff);
    return root / (".import-" + std::to_string(distribution(generator)));
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

// BookStorage.load(DictionaryIndex.self, from:) requires every field of the
// synthesized Decodable conformance.
bool readable_index(const std::filesystem::path& directory) {
    QFile file(QString::fromStdString((directory / "index.json").string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    const QJsonObject index = document.object();
    return index.value("title").isString() && integer_value(index.value("format")) &&
        index.value("revision").isString() && index.value("isUpdatable").isBool() &&
        index.value("indexUrl").isString() && index.value("downloadUrl").isString();
}

std::string directory_name(const std::filesystem::path& directory) {
    return directory.filename().string();
}

std::vector<DictionaryType> written_types(const ::ImportResult& imported) {
    std::vector<DictionaryType> types;
    if (imported.term_count > 0) {
        types.push_back(DictionaryType::term);
    }
    if (imported.freq_count > 0) {
        types.push_back(DictionaryType::frequency);
    }
    if (imported.pitch_count > 0) {
        types.push_back(DictionaryType::pitch);
    }
    if (imported.kanji_count > 0) {
        types.push_back(DictionaryType::kanji);
    }
    return types;
}

int count_as_int(std::size_t count) {
    constexpr std::size_t max = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(count > max ? max : count);
}

}  // namespace

DictionaryStore::DictionaryStore(std::filesystem::path root) : root_(std::move(root)) {}

ImportOutcome DictionaryStore::import_zip(const std::filesystem::path& zip) const {
    ImportOutcome outcome;
    if (root_.empty()) {
        outcome.error = "no data root";
        return outcome;
    }
    std::error_code error;
    std::filesystem::create_directories(root_, error);
    if (error) {
        outcome.error = "cannot create dictionary root: " + error.message();
        return outcome;
    }

    // The importer derives the final directory name from index.title, which
    // is only known after a successful import, so the staging directory
    // cannot be predicted up front.
    const auto staging = temporary_sibling(root_);
    std::filesystem::remove_all(staging, error);

    ::ImportResult imported;
    try {
        imported = dictionary_importer::import(zip.string(), staging.string(), false);
    } catch (const std::exception& exception) {
        std::filesystem::remove_all(staging, error);
        outcome.error = exception.what();
        return outcome;
    }
    if (!imported.success) {
        std::filesystem::remove_all(staging, error);
        outcome.error = imported.errors.empty() ? "import failed" : imported.errors.front();
        return outcome;
    }

    const auto produced = staging / imported.title;
    for (const DictionaryType type : written_types(imported)) {
        const auto destination = root_ / std::string(dictionary_type_name(type)) / imported.title;
        std::filesystem::create_directories(destination.parent_path(), error);
        if (error) {
            std::filesystem::remove_all(staging, error);
            outcome.error = "cannot create type directory: " + error.message();
            return outcome;
        }
        // BookStorage.copyFile replaces the destination, so a re-import does
        // not leave files from the previous version behind.
        std::filesystem::remove_all(destination, error);
        std::filesystem::copy(produced, destination, std::filesystem::copy_options::recursive,
                              error);
        if (error) {
            std::filesystem::remove_all(staging, error);
            outcome.error = "cannot publish imported dictionary: " + error.message();
            return outcome;
        }
        outcome.directories.push_back(destination);
    }
    std::filesystem::remove_all(staging, error);

    if (imported.term_count > 0) {
        outcome.directory =
            root_ / std::string(dictionary_type_name(DictionaryType::term)) / imported.title;
    } else if (!outcome.directories.empty()) {
        outcome.directory = outcome.directories.front();
    }
    outcome.term_count = count_as_int(imported.term_count);
    outcome.freq_count = count_as_int(imported.freq_count);
    outcome.pitch_count = count_as_int(imported.pitch_count);
    outcome.kanji_count = count_as_int(imported.kanji_count);
    outcome.ok = true;
    return outcome;
}

std::vector<DictionaryInfo> DictionaryStore::installed(DictionaryType type) const {
    if (root_.empty()) {
        return {};
    }
    const std::filesystem::path directory = root_ / std::string(dictionary_type_name(type));
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error || !std::filesystem::is_directory(directory, error)) {
        return {};
    }

    std::vector<DictionaryInfo> dictionaries;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            break;
        }
        const std::string name = directory_name(entry.path());
        if (!name.empty() && name.front() == '.') {
            continue;
        }
        if (!entry.is_directory()) {
            std::error_code remove_error;
            std::filesystem::remove_all(entry.path(), remove_error);
            continue;
        }
        if (!readable_index(entry.path())) {
            std::error_code remove_error;
            std::filesystem::remove_all(entry.path(), remove_error);
            continue;
        }
        dictionaries.push_back(DictionaryInfo{entry.path(), name});
    }
    return dictionaries;
}

std::vector<std::filesystem::path> DictionaryStore::installed() const {
    std::vector<std::filesystem::path> directories;
    for (const DictionaryType type : {DictionaryType::term, DictionaryType::frequency,
                                      DictionaryType::pitch, DictionaryType::kanji}) {
        for (const DictionaryInfo& dictionary : installed(type)) {
            directories.push_back(dictionary.path);
        }
    }
    return directories;
}

bool DictionaryStore::remove(DictionaryType type, std::string_view title) const {
    if (root_.empty() || title.empty()) {
        return false;
    }
    const std::filesystem::path name(title);
    if (name.is_absolute() || name.has_parent_path() || name == "." || name == "..") {
        return false;
    }
    std::error_code error;
    std::filesystem::remove_all(root_ / std::string(dictionary_type_name(type)) / name, error);
    return !error;
}

}  // namespace niratan::dictionary
