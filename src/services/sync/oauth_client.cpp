// SPDX-License-Identifier: GPL-3.0-or-later

#include "oauth_client.h"

#include "oauth_credentials.h"
#include "oauth_policy.h"
#include "sync_config.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>

#include <initializer_list>
#include <utility>

namespace niratan::sync {
namespace {

using AuthCallback = std::function<void(std::optional<AuthError>)>;
using RefreshCallback = std::function<void(std::optional<std::string>, std::optional<AuthError>)>;

struct TokenResponse {
    std::string access_token;
    std::optional<std::string> refresh_token;
    std::optional<std::string> scope;
};

struct HttpResult {
    std::optional<int> status;
    QByteArray body;
    bool network_error;
};

std::optional<TokenResponse> decode_tokens(const QByteArray& body) {
    const auto object = QJsonDocument::fromJson(body).object();
    const auto access = object.value("access_token");
    const auto refresh = object.value("refresh_token");
    const auto scope = object.value("scope");
    const auto valid_optional = [](const QJsonValue& value) {
        return value.isString() || value.isNull() || value.isUndefined();
    };
    if (!access.isString() || !valid_optional(refresh) || !valid_optional(scope)) {
        return std::nullopt;
    }
    const auto as_optional = [](const QJsonValue& value) -> std::optional<std::string> {
        return value.isString() ? std::optional{value.toString().toStdString()} : std::nullopt;
    };
    return TokenResponse{access.toString().toStdString(), as_optional(refresh), as_optional(scope)};
}

void append_form(QByteArray& form, const char* key, const std::string& value) {
    if (!form.isEmpty()) {
        form += '&';
    }
    form += key;
    form += '=';
    // URLComponents leaves URI punctuation such as ':' and '/' visible in query values.
    form += QUrl::toPercentEncoding(QString::fromStdString(value), ":/?@");
}

QByteArray encode_form(std::initializer_list<std::pair<const char*, std::string>> fields,
                       const std::optional<std::string>& secret) {
    QByteArray form;
    for (const auto& [key, value] : fields) {
        append_form(form, key, value);
    }
    if (secret) {
        append_form(form, "client_secret", *secret);
    }
    return form;
}

AuthErrorKind auth_error(LoopbackErrorKind kind) {
    switch (kind) {
        case LoopbackErrorKind::already_in_progress:
            return AuthErrorKind::already_in_progress;
        case LoopbackErrorKind::listener_unavailable:
            return AuthErrorKind::listener_unavailable;
        case LoopbackErrorKind::browser_unavailable:
            return AuthErrorKind::browser_unavailable;
        case LoopbackErrorKind::timed_out:
            return AuthErrorKind::timed_out;
        case LoopbackErrorKind::authorization_denied:
            return AuthErrorKind::authorization_denied;
        case LoopbackErrorKind::cancelled:
            return AuthErrorKind::cancelled;
    }
    return AuthErrorKind::invalid_response;
}

}  // namespace

std::string AuthError::description() const {
    switch (kind) {
        case AuthErrorKind::invalid_client_id:
            return "Invalid Client ID format";
        case AuthErrorKind::invalid_response:
            return "Invalid token response";
        case AuthErrorKind::token_exchange_failed:
            return "Token exchange failed: " + std::to_string(status_code);
        case AuthErrorKind::not_authenticated:
            return "Not authenticated\nPlease sign in";
        case AuthErrorKind::token_refresh_failed:
            return "Failed to refresh token\nPlease sign in again";
        case AuthErrorKind::token_refresh_unavailable:
            return "Google Drive could not refresh authorization. Your saved connection was kept. "
                   "Please try again.";
        case AuthErrorKind::missing_refresh_token:
            return "Google did not return a refresh token\nPlease try connecting again";
        case AuthErrorKind::missing_library_access:
            return "Google Drive file access was not granted. Reconnect and allow access to sync.";
        case AuthErrorKind::credentials_save_failed:
            return "Could not save Google Drive authorization. Please try connecting again.";
        case AuthErrorKind::already_in_progress:
            return loopback_error_description(LoopbackErrorKind::already_in_progress);
        case AuthErrorKind::cancelled:
            return loopback_error_description(LoopbackErrorKind::cancelled);
        case AuthErrorKind::listener_unavailable:
            return loopback_error_description(LoopbackErrorKind::listener_unavailable);
        case AuthErrorKind::browser_unavailable:
            return loopback_error_description(LoopbackErrorKind::browser_unavailable);
        case AuthErrorKind::timed_out:
            return loopback_error_description(LoopbackErrorKind::timed_out);
        case AuthErrorKind::authorization_denied:
            return loopback_error_description(LoopbackErrorKind::authorization_denied);
    }
    return {};
}

struct GoogleDriveAuth::State {
    GoogleDriveAuth* owner;
    std::filesystem::path settings_file;
    CredentialsStore store;
    QPointer<LoopbackAuthorization> loopback;
    Config config;
    QNetworkAccessManager manager;
    mutable std::optional<GoogleDriveCredentials> cached;
    AuthCallback pending_auth;
    QPointer<QNetworkReply> exchange_reply;
    std::size_t attempt = 0;
    std::size_t credentials_generation = 0;

    State(GoogleDriveAuth* owner, std::filesystem::path settings_file,
          std::filesystem::path credentials_file, LoopbackAuthorization& loopback, Config config)
        : owner(owner), settings_file(std::move(settings_file)), store(std::move(credentials_file)),
          loopback(&loopback), config(std::move(config)), manager(owner) {}

    const std::optional<GoogleDriveCredentials>& credentials() const {
        if (!cached) {
            cached = store.load();
        }
        return cached;
    }

    bool save(GoogleDriveCredentials credentials) {
        if (!store.save(credentials)) {
            return false;
        }
        cached = std::move(credentials);
        return true;
    }

    void clear() {
        ++credentials_generation;
        store.clear();
        cached.reset();
    }

    QNetworkReply* post(QByteArray body, std::function<void(HttpResult)> completion) {
        QNetworkRequest request(config.token_endpoint);
        request.setTransferTimeout(config.timeout);
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          QStringLiteral("application/x-www-form-urlencoded"));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::SameOriginRedirectPolicy);
        auto* reply = manager.post(request, body);
        QObject::connect(
            reply, &QNetworkReply::finished, owner, [reply, completion = std::move(completion)] {
                const auto value = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
                auto body = reply->isReadable() ? reply->readAll() : QByteArray{};
                const auto error = reply->error() != QNetworkReply::NoError;
                reply->deleteLater();
                completion({value.isValid() ? std::optional{value.toInt()} : std::nullopt,
                            std::move(body), error});
            });
        return reply;
    }

    void finish_auth(std::optional<AuthError> error) {
        exchange_reply.clear();
        if (auto completion = std::exchange(pending_auth, {})) {
            completion(error);
        }
    }

    void start_auth(AuthCallback callback) {
        const auto settings = load_sync_oauth_config(settings_file);
        auto client_id = resolved_client_id(settings);
        if (!valid_client_id(client_id)) {
            callback(AuthError{AuthErrorKind::invalid_client_id});
            return;
        }
        if (pending_auth) {
            callback(AuthError{AuthErrorKind::already_in_progress});
            return;
        }
        if (!loopback) {
            callback(AuthError{AuthErrorKind::listener_unavailable});
            return;
        }
        const auto id = ++attempt;
        pending_auth = std::move(callback);
        const QPointer guard(owner);
        auto callback_client = client_id;
        loopback->authorize(
            std::move(client_id), std::string(requested_scope), config.loopback_timeout,
            config.browser_opener,
            [this, guard, id, client_id = std::move(callback_client),
             secret = resolved_client_secret(settings)](std::optional<LoopbackCode> code,
                                                        std::optional<LoopbackErrorKind> error) {
                if (!guard || id != attempt || !pending_auth) {
                    return;
                }
                if (error) {
                    finish_auth(AuthError{auth_error(*error)});
                } else if (!code) {
                    finish_auth(AuthError{AuthErrorKind::invalid_response});
                } else {
                    exchange(id, *code, client_id, secret);
                }
            });
    }

    void exchange(std::size_t id, const LoopbackCode& code, std::string client_id,
                  std::optional<std::string> secret) {
        auto form = encode_form({{"code", code.code},
                                 {"client_id", client_id},
                                 {"redirect_uri", code.redirect_uri},
                                 {"grant_type", "authorization_code"},
                                 {"code_verifier", code.code_verifier}},
                                secret);
        exchange_reply = post(std::move(form),
                              [this, id, client_id = std::move(client_id),
                               secret = std::move(secret)](HttpResult response) {
                                  if (id != attempt || !pending_auth) {
                                      return;
                                  }
                                  finish_auth(apply_exchange(response, client_id, secret));
                              });
    }

    std::optional<AuthError> apply_exchange(const HttpResult& response,
                                            const std::string& client_id,
                                            const std::optional<std::string>& secret) {
        const auto status = response.status.value_or(-1);
        if (status < 200 || status > 299 || response.network_error) {
            return AuthError{AuthErrorKind::token_exchange_failed, status};
        }
        const auto tokens = decode_tokens(response.body);
        if (!tokens) {
            return AuthError{AuthErrorKind::invalid_response};
        }
        if (!granted_scope(tokens->scope, requested_scope)) {
            return AuthError{AuthErrorKind::missing_library_access};
        }
        if (!tokens->refresh_token) {
            return AuthError{AuthErrorKind::missing_refresh_token};
        }
        if (!save({tokens->access_token, *tokens->refresh_token, client_id, secret})) {
            return AuthError{AuthErrorKind::credentials_save_failed};
        }
        ++credentials_generation;
        return std::nullopt;
    }

    void refresh(RefreshCallback callback) {
        auto previous = credentials();
        if (!previous) {
            callback(std::nullopt, AuthError{AuthErrorKind::not_authenticated});
            return;
        }
        auto secret = previous->client_secret;
        if (!secret) {
            const auto settings = load_sync_oauth_config(settings_file);
            if (previous->client_id == resolved_client_id(settings)) {
                secret = resolved_client_secret(settings);
            }
        }
        auto form = encode_form({{"client_id", previous->client_id},
                                 {"grant_type", "refresh_token"},
                                 {"refresh_token", previous->refresh_token}},
                                secret);
        const auto generation = credentials_generation;
        post(std::move(form),
             [this, generation, previous = std::move(*previous),
              callback = std::move(callback)](HttpResult response) mutable {
                 if (generation != credentials_generation) {
                     callback(std::nullopt, AuthError{AuthErrorKind::cancelled});
                     return;
                 }
                 finish_refresh(response, std::move(previous), std::move(callback));
             });
    }

    void finish_refresh(const HttpResult& response, GoogleDriveCredentials previous,
                        RefreshCallback callback) {
        if (response.status != 200 || response.network_error) {
            if (invalidates_stored_credentials(response.status, response.body)) {
                clear();
                callback(std::nullopt, AuthError{AuthErrorKind::token_refresh_failed});
            } else {
                callback(std::nullopt, AuthError{AuthErrorKind::token_refresh_unavailable});
            }
            return;
        }
        const auto tokens = decode_tokens(response.body);
        if (!tokens) {
            callback(std::nullopt, AuthError{AuthErrorKind::invalid_response});
        } else if (tokens->scope && !scope_includes(tokens->scope, file_scope)) {
            callback(std::nullopt, AuthError{AuthErrorKind::missing_library_access});
        } else {
            previous.access_token = tokens->access_token;
            if (save(std::move(previous))) {
                callback(tokens->access_token, std::nullopt);
            } else {
                callback(std::nullopt, AuthError{AuthErrorKind::credentials_save_failed});
            }
        }
    }

    AuthCallback cancel() {
        if (!pending_auth) {
            return {};
        }
        ++attempt;
        auto callback = std::exchange(pending_auth, {});
        if (exchange_reply) {
            auto reply = std::exchange(exchange_reply, {});
            reply->abort();
        } else if (loopback) {
            loopback->cancel();
        }
        return callback;
    }
};

GoogleDriveAuth::GoogleDriveAuth(std::filesystem::path settings_file,
                                 std::filesystem::path credentials_file,
                                 LoopbackAuthorization& loopback, Config config, QObject* parent)
    : QObject(parent),
      state_(std::make_unique<State>(this, std::move(settings_file), std::move(credentials_file),
                                     loopback, std::move(config))) {}

GoogleDriveAuth::~GoogleDriveAuth() {
    state_->cancel();
}

std::optional<std::string> GoogleDriveAuth::access_token(std::string* error) const {
    const auto& credentials = state_->credentials();
    if (error) {
        *error =
            credentials ? std::string{} : AuthError{AuthErrorKind::not_authenticated}.description();
    }
    return credentials ? std::optional{credentials->access_token} : std::nullopt;
}

bool GoogleDriveAuth::is_authenticated() const {
    const auto client_id = resolved_client_id(load_sync_oauth_config(state_->settings_file));
    const auto& credentials = state_->credentials();
    return credentials && valid_client_id(client_id) && credentials->client_id == client_id;
}

void GoogleDriveAuth::authenticate(AuthCallback callback) {
    state_->start_auth(std::move(callback));
}

void GoogleDriveAuth::refresh(RefreshCallback callback) {
    state_->refresh(std::move(callback));
}

void GoogleDriveAuth::cancel_authentication() {
    if (auto callback = state_->cancel()) {
        callback(AuthError{AuthErrorKind::cancelled});
    }
}

void GoogleDriveAuth::sign_out() {
    auto callback = state_->cancel();
    state_->clear();
    if (callback) {
        callback(AuthError{AuthErrorKind::cancelled});
    }
}

}  // namespace niratan::sync
