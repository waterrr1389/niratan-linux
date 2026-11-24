// SPDX-License-Identifier: GPL-3.0-or-later

#include "sync_config.h"

#include "services/storage/json_file.h"

#include <QJsonObject>

namespace niratan::sync {
namespace {

std::optional<std::string> nonempty(QString value) {
    value = value.trimmed();
    return value.isEmpty() ? std::nullopt : std::optional{value.toStdString()};
}

}  // namespace

SyncOAuthConfig load_sync_oauth_config(const std::filesystem::path& settings_file) {
    const auto document = storage::read_json_file(settings_file);
    const auto object = document.document.object();
    return {object.value("googleClientId").toString().trimmed().toStdString(),
            nonempty(object.value("googleClientSecret").toString())};
}

std::string resolved_client_id(const SyncOAuthConfig& config) {
    return nonempty(qEnvironmentVariable("NIRATAN_GDRIVE_CLIENT_ID"))
        .value_or(QString::fromStdString(config.client_id).trimmed().toStdString());
}

std::optional<std::string> resolved_client_secret(const SyncOAuthConfig& config) {
    if (auto secret = nonempty(qEnvironmentVariable("NIRATAN_GDRIVE_CLIENT_SECRET"))) {
        return secret;
    }
    return config.client_secret ? nonempty(QString::fromStdString(*config.client_secret))
                                : std::nullopt;
}

}  // namespace niratan::sync
