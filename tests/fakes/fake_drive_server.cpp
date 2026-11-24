// SPDX-License-Identifier: GPL-3.0-or-later
#include "fake_drive_server.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTcpSocket>

namespace {

constexpr auto folder_mime = "application/vnd.google-apps.folder";

QByteArray json(const QJsonObject& value) {
    return QJsonDocument(value).toJson(QJsonDocument::Compact);
}

void parse_multipart(FakeDriveServer::Request& request) {
    const auto content_type = request.headers.value("content-type");
    constexpr auto prefix = QByteArrayView("multipart/related; boundary=");
    if (!content_type.startsWith(prefix)) {
        return;
    }
    const QByteArray marker = "--" + content_type.sliced(prefix.size());
    const auto meta_start = request.body.indexOf("\r\n\r\n");
    const auto next = request.body.indexOf("\r\n" + marker, meta_start + 4);
    const auto media_header = request.body.indexOf("Content-Type: ", next + 2 + marker.size());
    const auto media_start = request.body.indexOf("\r\n\r\n", media_header);
    const auto end = request.body.lastIndexOf("\r\n" + marker + "--\r\n");
    if (!request.body.startsWith(marker) || meta_start < 0 || next < 0 || media_header < 0 ||
        media_start < 0 || end < media_start + 4) {
        return;
    }
    request.metadata =
        QJsonDocument::fromJson(request.body.sliced(meta_start + 4, next - meta_start - 4))
            .object();
    const auto media_type_start = media_header + qsizetype(sizeof("Content-Type: ") - 1);
    request.media_type = request.body.sliced(media_type_start, media_start - media_type_start);
    request.media = request.body.sliced(media_start + 4, end - media_start - 4);
}

}  // namespace

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
                    Request request{
                        start_line[0], target.path(), QUrlQuery(target), {}, {}, {}, {}, {}, {}};
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
                    if (request.method == "POST" && request.path == QStringLiteral("/token")) {
                        auto form = request.body;
                        form.replace("+", "%20");
                        request.form.setQuery(QString::fromUtf8(form));
                    }
                    parse_multipart(request);
                    requests_.append(request);
                    const auto reply = !replies_.isEmpty() ? replies_.dequeue()
                        : tree_enabled_                    ? tree_reply(request)
                                                           : Reply{500, "Unexpected request"};
                    if (reply.hold) {
                        held_socket_ = socket;
                        held_reply_ = reply;
                    } else {
                        respond(socket, reply);
                    }
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

void FakeDriveServer::release() {
    if (held_socket_) {
        respond(held_socket_, held_reply_);
        held_socket_.clear();
    }
}

void FakeDriveServer::enable_tree(int page_size) {
    tree_enabled_ = true;
    page_size_ = page_size;
}

void FakeDriveServer::put_file(QString id, QString name, QString parent, QByteArray media,
                               QString mime_type) {
    for (auto& file : tree_) {
        if (file.id == id) {
            file = {std::move(id), std::move(name), std::move(parent), std::move(media),
                    std::move(mime_type)};
            return;
        }
    }
    tree_.append({std::move(id), std::move(name), std::move(parent), std::move(media),
                  std::move(mime_type)});
}

FakeDriveServer::Reply FakeDriveServer::tree_reply(const Request& request) {
    const auto query = [&](const QString& field) {
        return request.query.queryItemValue(field, QUrl::FullyDecoded);
    };
    const auto api = QStringLiteral("/drive/v3/files");
    const auto upload = QStringLiteral("/upload/drive/v3/files");
    const bool multipart = request.path == upload || request.path.startsWith(upload + '/');
    const auto route = multipart ? upload : api;
    if (request.path != route && !request.path.startsWith(route + '/')) {
        return {404, {}};
    }
    if (request.method == "GET" && request.path == api) {
        const auto filter = query("q");
        static const QRegularExpression parent_pattern(QStringLiteral("'([^']*)' in parents"));
        static const QRegularExpression name_pattern(
            QStringLiteral("name\\s*=\\s*(['\"])(.*?)\\1"));
        QStringList parents;
        for (const auto& match : parent_pattern.globalMatch(filter)) {
            parents.append(match.captured(1));
        }
        const auto name = name_pattern.match(filter);
        const bool folders_only = filter.contains("mimeType=");
        const bool files_only = filter.contains("mimeType !=");
        QJsonArray files;
        for (const auto& file : tree_) {
            const bool folder = file.mime_type == QString::fromLatin1(folder_mime);
            if ((file.trashed && filter.contains("trashed=false")) ||
                (!parents.empty() && !parents.contains(file.parent)) ||
                (name.hasMatch() && name.captured(2) != file.name) || (folders_only && !folder) ||
                (files_only && folder)) {
                continue;
            }
            files.append(QJsonObject{
                {"id", file.id}, {"name", file.name}, {"parents", QJsonArray{file.parent}}});
        }
        const auto start = qBound<qsizetype>(0, query("pageToken").toLongLong(), files.size());
        const auto end = page_size_ > 0 ? qMin(start + page_size_, files.size()) : files.size();
        QJsonArray page;
        for (auto i = start; i < end; ++i) {
            page.append(files[i]);
        }
        QJsonObject response{{"files", page}};
        if (end < files.size()) {
            response.insert("nextPageToken", QString::number(end));
        }
        return {200, json(response)};
    }

    const auto metadata =
        multipart ? request.metadata : QJsonDocument::fromJson(request.body).object();
    if (request.method == "POST" && request.path == route) {
        const auto name = metadata.value("name").toString();
        const auto parents = metadata.value("parents").toArray();
        if (name.isEmpty() || parents.empty()) {
            return {400, {}};
        }
        const auto id = QStringLiteral("new-%1").arg(++next_file_);
        put_file(id, name, parents.first().toString(), request.media,
                 multipart ? QString::fromLatin1(request.media_type)
                           : metadata.value("mimeType").toString());
        return {200, json({{"id", id}})};
    }
    const auto id = request.path.mid(route.size() + 1);
    for (auto& file : tree_) {
        if (file.id != id || file.trashed) {
            continue;
        }
        if (request.method == "GET" && query("alt") == QStringLiteral("media")) {
            return {200, file.media, file.mime_type.toLatin1()};
        }
        if (request.method == "PATCH") {
            if (multipart) {
                file.name = metadata.value("name").toString();
                file.media = request.media;
                file.mime_type = QString::fromLatin1(request.media_type);
            } else if (metadata.value("trashed").toBool()) {
                file.trashed = true;
            }
            return {200, json({{"id", id}})};
        }
    }
    return {404, {}};
}

void FakeDriveServer::respond(QTcpSocket* socket, const Reply& reply) {
    socket->write("HTTP/1.1 " + QByteArray::number(reply.status) +
                  " Fake Drive\r\nContent-Type: " + reply.content_type +
                  "\r\nContent-Length: " + QByteArray::number(reply.body.size()) +
                  "\r\nConnection: close\r\n\r\n" + reply.body);
    socket->disconnectFromHost();
}

const QList<FakeDriveServer::Request>& FakeDriveServer::requests() const {
    return requests_;
}
