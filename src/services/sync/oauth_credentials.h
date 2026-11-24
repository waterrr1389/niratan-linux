// SPDX-License-Identifier: GPL-3.0-or-later
// TokenStorage.swift: GoogleDriveCredentials, saveCredentials, getStoredCredentials and clear.
// Linux stores credentials in <data root>/sync/credentials.json with mode 0600.
// Writes are atomic; failure leaves the old file intact. Missing or damaged
// files mean no credentials; the next save replaces a damaged file.
// JSON keys are accessToken, refreshToken, clientId and optional clientSecret.

#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace niratan::sync {

struct GoogleDriveCredentials {
    std::string access_token;
    std::string refresh_token;
    std::string client_id;
    std::optional<std::string> client_secret;
    bool operator==(const GoogleDriveCredentials&) const = default;
};

class CredentialsStore {
   public:
    explicit CredentialsStore(std::filesystem::path file);
    std::optional<GoogleDriveCredentials> load(std::string* error = nullptr) const;
    bool save(const GoogleDriveCredentials& credentials, std::string* error = nullptr) const;
    bool clear();

   private:
    std::filesystem::path file_;
};

}  // namespace niratan::sync
