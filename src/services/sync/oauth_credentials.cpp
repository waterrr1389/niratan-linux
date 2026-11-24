// SPDX-License-Identifier: GPL-3.0-or-later

#include "oauth_credentials.h"

#include "services/storage/json_file.h"

#include <QFile>
#include <QJsonObject>
#include <QTemporaryFile>

#include <utility>

namespace niratan::sync {
namespace {

bool fail(std::string* error, std::string reason) {
    if (error) {
        *error = std::move(reason);
    }
    return false;
}

}  // namespace

CredentialsStore::CredentialsStore(std::filesystem::path file) : file_(std::move(file)) {}

std::optional<GoogleDriveCredentials> CredentialsStore::load(std::string* error) const {
    if (error) {
        error->clear();
    }
    const auto read = storage::read_json_file(file_);
    if (read.status != storage::JsonReadStatus::success) {
        fail(error, read.error);
        return std::nullopt;
    }
    const auto object = read.document.object();
    const auto access = object.value("accessToken");
    const auto refresh = object.value("refreshToken");
    const auto client = object.value("clientId");
    const auto secret = object.value("clientSecret");
    if (!access.isString() || !refresh.isString() || !client.isString() ||
        (!secret.isUndefined() && !secret.isNull() && !secret.isString())) {
        fail(error, "Invalid credentials document");
        return std::nullopt;
    }
    return GoogleDriveCredentials{access.toString().toStdString(), refresh.toString().toStdString(),
                                  client.toString().toStdString(),
                                  secret.isString() ? std::optional{secret.toString().toStdString()}
                                                    : std::nullopt};
}

bool CredentialsStore::save(const GoogleDriveCredentials& credentials, std::string* error) const {
    if (error) {
        error->clear();
    }
    if (file_.empty()) {
        return fail(error, "Credentials path is empty");
    }
    auto directory = file_.parent_path();
    if (directory.empty()) {
        directory = ".";
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(directory, filesystem_error);
    if (filesystem_error) {
        return fail(error, filesystem_error.message());
    }
    QJsonObject object{{"accessToken", QString::fromStdString(credentials.access_token)},
                       {"refreshToken", QString::fromStdString(credentials.refresh_token)},
                       {"clientId", QString::fromStdString(credentials.client_id)}};
    if (credentials.client_secret) {
        object.insert("clientSecret", QString::fromStdString(*credentials.client_secret));
    }
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QTemporaryFile temporary(QString::fromStdString((directory / ".credentials-XXXXXX").string()));
    if (!temporary.open() ||
        !temporary.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        temporary.write(bytes) != bytes.size() || !temporary.flush()) {
        return fail(error, temporary.errorString().toStdString());
    }
    temporary.close();
    std::filesystem::rename(temporary.fileName().toStdString(), file_, filesystem_error);
    return filesystem_error ? fail(error, filesystem_error.message()) : true;
}

bool CredentialsStore::clear() {
    std::error_code error;
    std::filesystem::remove(file_, error);
    return !error;
}

}  // namespace niratan::sync
