// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fakes/fake_drive_server.h"
#include "services/sync/oauth_client.h"

#include <QEventLoop>
#include <QFile>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>

namespace oauth_fixture {

inline constexpr auto client_id = "123-fixture.apps.googleusercontent.com";
inline constexpr auto credentials_json =
    R"({"accessToken":"SYNTHETIC-OLD-ACCESS","refreshToken":"SYNTHETIC-OLD-REFRESH","clientId":"123-fixture.apps.googleusercontent.com","clientSecret":"saved-secret"})";
inline constexpr auto exchange_json =
    R"({"access_token":"SYNTHETIC-NEW-ACCESS","refresh_token":"SYNTHETIC-NEW-REFRESH","scope":"https://www.googleapis.com/auth/drive.file"})";

inline QByteArray read(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

inline bool write(const std::filesystem::path& path, QByteArrayView data) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::WriteOnly) && file.write(data.data(), data.size()) == data.size();
}

class Environment {
   public:
    Environment()
        : client_id_(qgetenv("NIRATAN_GDRIVE_CLIENT_ID")),
          client_secret_(qgetenv("NIRATAN_GDRIVE_CLIENT_SECRET")) {
        qunsetenv("NIRATAN_GDRIVE_CLIENT_ID");
        qunsetenv("NIRATAN_GDRIVE_CLIENT_SECRET");
    }

    ~Environment() {
        restore("NIRATAN_GDRIVE_CLIENT_ID", client_id_);
        restore("NIRATAN_GDRIVE_CLIENT_SECRET", client_secret_);
    }

   private:
    static void restore(const char* name, const QByteArray& value) {
        if (value.isNull()) {
            qunsetenv(name);
        } else {
            qputenv(name, value);
        }
    }

    QByteArray client_id_;
    QByteArray client_secret_;
};

struct AuthFixture {
    Environment environment;
    QTemporaryDir directory;
    FakeDriveServer endpoint;
    niratan::sync::LoopbackAuthorization loopback;
    QUrl authorization;
    int browser_calls = 0;
    bool browser_available = true;

    std::filesystem::path settings() const {
        return directory.filePath(QStringLiteral("settings.json")).toStdString();
    }

    std::filesystem::path credentials() const {
        return directory.filePath(QStringLiteral("sync/credentials.json")).toStdString();
    }

    bool start() {
        return directory.isValid() && endpoint.start() &&
            write(settings(),
                  R"({"googleClientId":" 123-fixture.apps.googleusercontent.com ","googleClientSecret":" configured-secret "})");
    }

    niratan::sync::GoogleDriveAuth::Config config() {
        auto config = niratan::sync::GoogleDriveAuth::Config{};
        config.token_endpoint = endpoint.base_url();
        config.token_endpoint.setPath(QStringLiteral("/token"));
        config.browser_opener = [this](const QUrl& url) {
            ++browser_calls;
            authorization = url;
            return browser_available;
        };
        return config;
    }
};

inline QString query_value(const QUrl& url, const QString& key) {
    return QUrlQuery(url).queryItemValue(key, QUrl::FullyDecoded);
}

inline QUrl redirect(const QUrl& authorization) {
    return QUrl(query_value(authorization, QStringLiteral("redirect_uri")));
}

inline QByteArray state(const QUrl& authorization) {
    return query_value(authorization, QStringLiteral("state")).toUtf8();
}

inline QByteArray callback(const QUrl& redirect_uri, const QByteArray& target,
                           const QByteArray& method = "GET", const QByteArray& extra = {}) {
    QTcpSocket socket;
    QEventLoop loop;
    QByteArray reply;
    QObject::connect(&socket, &QTcpSocket::readyRead, &loop, [&] { reply += socket.readAll(); });
    QObject::connect(&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QTcpSocket::errorOccurred, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QTcpSocket::connected, &loop, [&] {
        socket.write(method + " " + target + " HTTP/1.1\r\nHost: " +
                     redirect_uri.authority().toLatin1() + "\r\n" + extra + "\r\n");
    });
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    if (redirect_uri.port() <= 0) {
        return {};
    }
    socket.connectToHost(redirect_uri.host(), static_cast<quint16>(redirect_uri.port()));
    loop.exec();
    return reply + socket.readAll();
}

inline bool port_closed(const QUrl& redirect_uri) {
    if (redirect_uri.port() <= 0) {
        return false;
    }
    QTcpSocket socket;
    socket.connectToHost(redirect_uri.host(), static_cast<quint16>(redirect_uri.port()));
    return !socket.waitForConnected(500) &&
        socket.error() == QAbstractSocket::ConnectionRefusedError;
}

}  // namespace oauth_fixture
