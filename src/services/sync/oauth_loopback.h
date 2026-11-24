// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveLoopbackAuthorization.swift: authorize, cancel, PKCE and Session.
// One active authorization uses an OS-assigned 127.0.0.1 port, S256 (never
// exposing the verifier to the browser), state, offline access and consent.
// The callback must be GET, with the exact loopback Host, path /, no fragment,
// one matching state and exactly one nonempty code or error, not both.
// Invalid requests get 400/403 and do not finish authorization. Headers above
// 100000 bytes get 431. At most eight connections are held; an incomplete
// header is dropped after five seconds. Pages never echo callback values.

#pragma once

#include <QObject>
#include <QUrl>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace niratan::sync {

enum class LoopbackErrorKind {
    already_in_progress,
    listener_unavailable,
    browser_unavailable,
    timed_out,
    authorization_denied,
    cancelled,
};

std::string loopback_error_description(LoopbackErrorKind kind);

struct LoopbackCode {
    std::string code;
    std::string redirect_uri;
    std::string code_verifier;
};

class LoopbackAuthorization : public QObject {
    Q_OBJECT

   public:
    using BrowserOpener = std::function<bool(const QUrl&)>;
    using Callback =
        std::function<void(std::optional<LoopbackCode>, std::optional<LoopbackErrorKind>)>;

    explicit LoopbackAuthorization(QObject* parent = nullptr);
    ~LoopbackAuthorization() override;

    void authorize(std::string client_id, std::string scope, std::chrono::milliseconds timeout,
                   BrowserOpener opener, Callback callback);
    void cancel();
    bool is_authorizing() const;
    static std::string code_challenge(std::string_view verifier);

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::sync
