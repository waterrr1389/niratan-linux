// SPDX-License-Identifier: GPL-3.0-or-later
#include "fake_drive_server.h"

#include <QHostAddress>
#include <QTcpSocket>

FakeDriveServer::FakeDriveServer() {
    connect(this, &QTcpServer::newConnection, this, [this] {
        while (auto* socket = nextPendingConnection()) {
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(
                socket, &QTcpSocket::readyRead, this,
                [this, socket, buffer = QByteArray{}]() mutable {
                    buffer += socket->readAll();
                    const auto split = buffer.indexOf("\r\n\r\n");
                    if (split < 0) {
                        return;
                    }
                    const auto lines = buffer.first(split).split('\n');
                    const auto start_line = lines.front().trimmed().split(' ');
                    if (start_line.size() != 3) {
                        socket->disconnectFromHost();
                        return;
                    }
                    const auto target = QUrl::fromEncoded(start_line[1]);
                    Request request{start_line[0], target.path(), QUrlQuery(target), {}, {}};
                    for (qsizetype i = 1; i < lines.size(); ++i) {
                        const auto colon = lines[i].indexOf(':');
                        if (colon > 0) {
                            request.headers.insert(lines[i].first(colon).toLower(),
                                                   lines[i].sliced(colon + 1).trimmed());
                        }
                    }
                    const auto size = request.headers.value("content-length").toLongLong();
                    if (size < 0 || size > buffer.size() - split - 4) {
                        return;
                    }
                    request.body = buffer.sliced(split + 4, size);
                    requests_.append(std::move(request));
                    const auto reply =
                        replies_.isEmpty() ? Reply{500, "Unexpected request"} : replies_.dequeue();
                    socket->write("HTTP/1.1 " + QByteArray::number(reply.status) +
                                  " Fake Drive\r\nContent-Type: " + reply.content_type +
                                  "\r\nContent-Length: " + QByteArray::number(reply.body.size()) +
                                  "\r\nConnection: close\r\n\r\n" + reply.body);
                    socket->disconnectFromHost();
                });
        }
    });
}

bool FakeDriveServer::start() {
    return listen(QHostAddress::LocalHost, 0);
}

QUrl FakeDriveServer::base_url() const {
    return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(serverPort()));
}

void FakeDriveServer::enqueue(Reply reply) {
    replies_.enqueue(std::move(reply));
}

const QList<FakeDriveServer::Request>& FakeDriveServer::requests() const {
    return requests_;
}
