// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_config_store.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QString>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace niratan::dictionary {
namespace {

std::optional<QByteArray> read_all(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return std::nullopt;
    }
    return bytes;
}

// Swift writes these files with .atomic: the bytes go to a temporary file in
// the target directory and replace the target only on commit.
bool write_atomic(const std::filesystem::path& path, const QByteArray& bytes) {
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    QSaveFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(bytes) != static_cast<qint64>(bytes.size())) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

}  // namespace

std::optional<DictionaryConfig> load_dictionary_config(const std::filesystem::path& path) {
    const std::optional<QByteArray> bytes = read_all(path);
    if (!bytes) {
        return std::nullopt;
    }
    return decode_dictionary_config(*bytes);
}

bool save_dictionary_config(const std::filesystem::path& path, const DictionaryConfig& config) {
    return write_atomic(path, encode_dictionary_config(config));
}

std::set<std::string> load_collapsed_dictionaries(const std::filesystem::path& path) {
    const std::optional<QByteArray> bytes = read_all(path);
    if (!bytes) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(*bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        return {};
    }

    std::set<std::string> titles;
    for (const QJsonValue& value : document.array()) {
        if (!value.isString()) {
            return {};
        }
        titles.insert(value.toString().toStdString());
    }
    return titles;
}

bool save_collapsed_dictionaries(const std::filesystem::path& path,
                                 const std::set<std::string>& titles) {
    QJsonArray array;
    for (const std::string& title : titles) {
        array.append(QString::fromStdString(title));
    }
    return write_atomic(path, QJsonDocument(array).toJson(QJsonDocument::Indented));
}

std::set<std::string> toggle_collapsed_dictionary(const std::filesystem::path& path,
                                                  std::string_view title) {
    std::set<std::string> titles = load_collapsed_dictionaries(path);
    if (titles.contains(std::string(title))) {
        titles.erase(std::string(title));
    } else {
        titles.insert(std::string(title));
    }
    save_collapsed_dictionaries(path, titles);
    return titles;
}

}  // namespace niratan::dictionary
