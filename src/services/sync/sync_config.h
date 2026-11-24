// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveAuth.swift: clientId(for:) and bundledClientSecret(for:).
// The environment overrides settings (googleClientId and googleClientSecret)
// via NIRATAN_GDRIVE_CLIENT_ID and NIRATAN_GDRIVE_CLIENT_SECRET. Values are
// trimmed; empty values are absent.

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace niratan::sync {

struct SyncOAuthConfig {
    std::string client_id;
    std::optional<std::string> client_secret;
};

SyncOAuthConfig load_sync_oauth_config(const std::filesystem::path& settings_file);
std::string resolved_client_id(const SyncOAuthConfig& config);
std::optional<std::string> resolved_client_secret(const SyncOAuthConfig& config);

}  // namespace niratan::sync
