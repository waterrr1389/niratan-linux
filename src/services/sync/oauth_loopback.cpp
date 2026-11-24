// SPDX-License-Identifier: GPL-3.0-or-later

#include "oauth_loopback.h"

#include <QCryptographicHash>
#include <QHash>
#include <QHostAddress>
#include <QPointer>
#include <QRandomGenerator>
#include <QStringDecoder>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>

#include <array>
#include <utility>

namespace niratan::sync {
namespace {

constexpr qsizetype maximum_header = 100000;
constexpr qsizetype maximum_connections = 8;
constexpr auto socket_timeout = std::chrono::seconds(5);

std::string base64_url(QByteArrayView data) {
    return data.toByteArray()
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)
        .toStdString();
}

std::string random_value() {
    std::array<quint32, 8> words;
    QRandomGenerator::system()->fillRange(words.data(), words.size());
    return base64_url(QByteArrayView(reinterpret_cast<const char*>(words.data()), 32));
}

void respond(QTcpSocket& socket, QByteArrayView status, bool accepted) {
    const QByteArray message = accepted
        ? "Google Drive authorization received. Return to Niratan to finish connecting."
        : "This Google Drive callback could not be accepted. Return to Niratan to continue "
          "sign-in.";
    const QByteArray body =
        "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"referrer\" "
        "content=\"no-referrer\"><title>Google Drive sign-in</title></head><body>"
        "<h1>Google Drive sign-in</h1><p>" +
        message + "</p></body></html>";
    socket.write("HTTP/1.1 " + status.toByteArray() +
                 "\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " +
                 QByteArray::number(body.size()) +
                 "\r\nCache-Control: no-store"
                 "\r\nContent-Security-Policy: default-src 'none'; frame-ancestors 'none'"
                 "\r\nConnection: close\r\n\r\n" +
                 body);
    socket.disconnectFromHost();
}

}  // namespace

std::string loopback_error_description(LoopbackErrorKind kind) {
    switch (kind) {
        case LoopbackErrorKind::already_in_progress:
            return "Google Drive sign-in is already in progress.";
        case LoopbackErrorKind::listener_unavailable:
            return "Could not start Google Drive sign-in. Please try again.";
        case LoopbackErrorKind::browser_unavailable:
            return "Could not open the browser for Google Drive sign-in.";
        case LoopbackErrorKind::timed_out:
            return "Google Drive sign-in timed out. Please try again.";
        case LoopbackErrorKind::authorization_denied:
            return "Google Drive sign-in was not completed. Please try again.";
        case LoopbackErrorKind::cancelled:
            return "Google Drive sign-in was cancelled.";
    }
    return {};
}

struct LoopbackAuthorization::State {
    LoopbackAuthorization* owner;
    QTcpServer server;
    QTimer timer;
    Callback pending;
    QHash<QTcpSocket*, QByteArray> connections;
    QByteArray host;
    std::string redirect;
    std::string state;
    std::string verifier;
    std::size_t generation = 0;

    explicit State(LoopbackAuthorization* owner) : owner(owner), server(owner), timer(owner) {
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, owner,
                         [this] { finish(std::nullopt, LoopbackErrorKind::timed_out); });
        QObject::connect(&server, &QTcpServer::newConnection, owner, [this] {
            while (auto* socket = server.nextPendingConnection()) {
                accept(socket);
            }
        });
    }

    ~State() {
        shutdown();
        for (auto* socket : server.findChildren<QTcpSocket*>()) {
            QObject::disconnect(socket, nullptr, owner, nullptr);
        }
    }

    void shutdown(QTcpSocket* keep = nullptr) {
        timer.stop();
        server.close();
        const auto sockets = connections.keys();
        connections.clear();
        for (auto* socket : sockets) {
            if (socket != keep) {
                socket->abort();
                socket->deleteLater();
            }
        }
        while (auto* socket = server.nextPendingConnection()) {
            delete socket;
        }
        host.clear();
        redirect.clear();
        state.clear();
        verifier.clear();
    }

    void finish(std::optional<LoopbackCode> code, std::optional<LoopbackErrorKind> error,
                QTcpSocket* keep = nullptr) {
        auto callback = std::exchange(pending, {});
        shutdown(keep);
        if (callback) {
            callback(std::move(code), error);
        }
    }

    void start(std::string client_id, std::string scope, std::chrono::milliseconds timeout,
               BrowserOpener opener, Callback callback) {
        if (pending) {
            callback(std::nullopt, LoopbackErrorKind::already_in_progress);
            return;
        }
        pending = std::move(callback);
        const auto attempt = ++generation;
        state = random_value();
        verifier = random_value();
        if (!server.listen(QHostAddress::LocalHost, 0)) {
            finish(std::nullopt, LoopbackErrorKind::listener_unavailable);
            return;
        }
        host = "127.0.0.1:" + QByteArray::number(server.serverPort());
        redirect = "http://" + host.toStdString() + "/";
        QUrl url(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
        QUrlQuery query;
        query.addQueryItem("client_id", QString::fromStdString(client_id));
        query.addQueryItem("redirect_uri", QString::fromStdString(redirect));
        query.addQueryItem("response_type", "code");
        query.addQueryItem("scope", QString::fromStdString(scope));
        query.addQueryItem("access_type", "offline");
        query.addQueryItem("prompt", "consent");
        query.addQueryItem("state", QString::fromStdString(state));
        query.addQueryItem("code_challenge", QString::fromStdString(code_challenge(verifier)));
        query.addQueryItem("code_challenge_method", "S256");
        url.setQuery(query);
        timer.start(timeout);
        const QPointer guard(owner);
        const bool opened = opener && opener(url);
        if (guard && !opened && generation == attempt && pending) {
            finish(std::nullopt, LoopbackErrorKind::browser_unavailable);
        }
    }

    void accept(QTcpSocket* socket) {
        if (!pending || connections.size() >= maximum_connections) {
            socket->abort();
            socket->deleteLater();
            return;
        }
        connections.insert(socket, {});
        socket->setReadBufferSize(maximum_header + 1);
        QObject::connect(socket, &QTcpSocket::disconnected, owner, [this, socket] {
            connections.remove(socket);
            socket->deleteLater();
        });
        QObject::connect(socket, &QTcpSocket::readyRead, owner,
                         [this, socket] { receive(socket); });
        QTimer::singleShot(socket_timeout, socket, [socket] {
            socket->abort();
            socket->deleteLater();
        });
        receive(socket);
    }

    void receive(QTcpSocket* socket) {
        const auto it = connections.find(socket);
        if (it == connections.end() || !pending ||
            socket->state() != QAbstractSocket::ConnectedState) {
            return;
        }
        auto& buffer = it.value();
        buffer.append(socket->read(maximum_header + 1 - buffer.size()));
        if (buffer.size() > maximum_header) {
            respond(*socket, "431 Request Header Fields Too Large", false);
        } else if (const auto end = buffer.indexOf("\r\n\r\n"); end >= 0) {
            handle(socket, buffer.first(end));
        }
    }

    void handle(QTcpSocket* socket, QByteArrayView header) {
        auto utf8 = QStringDecoder(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
        const auto lines = QString(utf8(header)).split(QStringLiteral("\r\n"));
        const auto first = lines.front().split(u' ');
        int host_count = 0;
        bool correct_host = false;
        for (auto line = lines.cbegin() + 1; line != lines.cend(); ++line) {
            if (line->startsWith(QStringLiteral("host:"), Qt::CaseInsensitive)) {
                ++host_count;
                correct_host = line->sliced(5).trimmed() == QString::fromLatin1(host);
            }
        }
        if (utf8.hasError() || first.size() != 3 || first[0] != QStringLiteral("GET") ||
            (first[2] != QStringLiteral("HTTP/1.0") && first[2] != QStringLiteral("HTTP/1.1")) ||
            host_count != 1 || !correct_host || !first[1].startsWith(u'/')) {
            respond(*socket, "400 Bad Request", false);
            return;
        }
        const QUrl callback(QStringLiteral("http://") + QString::fromLatin1(host) + first[1],
                            QUrl::StrictMode);
        if (!callback.isValid() || callback.path() != QStringLiteral("/") ||
            callback.hasFragment()) {
            respond(*socket, "400 Bad Request", false);
            return;
        }
        const QUrlQuery query(callback);
        const auto states = query.allQueryItemValues("state", QUrl::FullyDecoded);
        if (states.size() != 1 || states.front() != QString::fromStdString(state)) {
            respond(*socket, "403 Forbidden", false);
            return;
        }
        const auto codes = query.allQueryItemValues("code", QUrl::FullyDecoded);
        const auto errors = query.allQueryItemValues("error", QUrl::FullyDecoded);
        if (codes.size() == 1 && !codes.front().isEmpty() && errors.empty()) {
            respond(*socket, "200 OK", true);
            finish(LoopbackCode{codes.front().toStdString(), redirect, verifier}, std::nullopt,
                   socket);
        } else if (errors.size() == 1 && !errors.front().isEmpty() && codes.empty()) {
            respond(*socket, "200 OK", false);
            finish(std::nullopt, LoopbackErrorKind::authorization_denied, socket);
        } else {
            respond(*socket, "400 Bad Request", false);
        }
    }
};

LoopbackAuthorization::LoopbackAuthorization(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>(this)) {}

LoopbackAuthorization::~LoopbackAuthorization() = default;

void LoopbackAuthorization::authorize(std::string client_id, std::string scope,
                                      std::chrono::milliseconds timeout, BrowserOpener opener,
                                      Callback callback) {
    state_->start(std::move(client_id), std::move(scope), timeout, std::move(opener),
                  std::move(callback));
}

void LoopbackAuthorization::cancel() {
    if (state_->pending) {
        state_->finish(std::nullopt, LoopbackErrorKind::cancelled);
    }
}

bool LoopbackAuthorization::is_authorizing() const {
    return bool(state_->pending);
}

std::string LoopbackAuthorization::code_challenge(std::string_view verifier) {
    const QByteArrayView bytes(verifier.data(), static_cast<qsizetype>(verifier.size()));
    return base64_url(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256));
}

}  // namespace niratan::sync
