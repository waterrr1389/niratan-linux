// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveAuth.swift: authenticate (loopback branch), exchangeCode,
// refreshAccessToken, credentials, storeCredentials, signOut and cancellation.
// One sign-in at a time. The application supplies the browser opener.
// The configurable token endpoint defaults to Google's; form-urlencoded POSTs
// time out after 10 s. Exchange accepts 2xx and sends the exact redirect and
// verifier; refresh accepts only 200 and prefers the saved client secret.
// Grants are checked on exchange and refresh but not persisted for ttu.
// The credentials cache only changes after a successful write or a clear.

#pragma once

#include "oauth_loopback.h"

#include <filesystem>

namespace niratan::sync {

enum class AuthErrorKind {
    invalid_client_id,
    invalid_response,
    token_exchange_failed,
    not_authenticated,
    token_refresh_failed,
    token_refresh_unavailable,
    missing_refresh_token,
    missing_library_access,
    credentials_save_failed,
    already_in_progress,
    cancelled,
    listener_unavailable,
    browser_unavailable,
    timed_out,
    authorization_denied,
};

struct AuthError {
    AuthErrorKind kind = AuthErrorKind::invalid_response;
    int status_code = 0;
    std::string description() const;
};

class GoogleDriveAuth : public QObject {
    Q_OBJECT

   public:
    struct Config {
        QUrl token_endpoint = QUrl("https://oauth2.googleapis.com/token");
        std::chrono::milliseconds timeout{10000};
        std::chrono::milliseconds loopback_timeout{180000};
        LoopbackAuthorization::BrowserOpener browser_opener;
    };

    GoogleDriveAuth(std::filesystem::path settings_file, std::filesystem::path credentials_file,
                    LoopbackAuthorization& loopback, Config config, QObject* parent = nullptr);
    ~GoogleDriveAuth() override;

    std::optional<std::string> access_token(std::string* error = nullptr) const;
    bool is_authenticated() const;
    void authenticate(std::function<void(std::optional<AuthError>)> callback);
    void
    refresh(std::function<void(std::optional<std::string>, std::optional<AuthError>)> callback);
    void cancel_authentication();
    void sign_out();

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::sync
