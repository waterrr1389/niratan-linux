// SPDX-License-Identifier: GPL-3.0-or-later

#include "json_file.h"

#include <QByteArray>
#include <QFile>
#include <QJsonParseError>
#include <QSaveFile>

#include <system_error>
#include <utility>

namespace niratan::storage {
namespace {

bool fail(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
    return false;
}

}  // namespace

JsonReadResult read_json_file(const std::filesystem::path& path) {
    JsonReadResult result;

    // A failed status query is left to the open below, which reports the
    // reason; only a confirmed absence is "missing".
    std::error_code status_error;
    const bool present = std::filesystem::exists(path, status_error);
    if (!status_error && !present) {
        return result;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.status = JsonReadStatus::invalid;
        result.error = file.errorString().toStdString();
        return result;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        result.status = JsonReadStatus::invalid;
        result.error = file.errorString().toStdString();
        return result;
    }

    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        result.status = JsonReadStatus::invalid;
        result.error = parse_error.errorString().toStdString();
        return result;
    }

    result.status = JsonReadStatus::success;
    result.document = document;
    return result;
}

bool write_json_file(const std::filesystem::path& path, const QJsonDocument& document,
                     std::string* error) {
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code directory_error;
        std::filesystem::create_directories(parent, directory_error);
        if (directory_error) {
            return fail(error, "cannot create parent directory: " + directory_error.message());
        }
    }

    std::error_code status_error;
    const bool present = std::filesystem::exists(path, status_error);
    if (status_error) {
        return fail(error, "cannot inspect target file: " + status_error.message());
    }
    if (present) {
        // A normalization pass must not replace bytes it could not read back, so
        // a damaged sidecar stays byte-identical until the user fixes or removes it.
        const JsonReadResult existing = read_json_file(path);
        if (existing.status == JsonReadStatus::invalid) {
            return fail(error, "refusing to overwrite damaged file: " + existing.error);
        }
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(error, file.errorString().toStdString());
    }
    const QByteArray bytes = document.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != static_cast<qint64>(bytes.size())) {
        const QString write_error = file.errorString();
        file.cancelWriting();
        return fail(error, write_error.toStdString());
    }
    if (!file.commit()) {
        return fail(error, file.errorString().toStdString());
    }
    return true;
}

}  // namespace niratan::storage
