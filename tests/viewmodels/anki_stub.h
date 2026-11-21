// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>

#include <string>

namespace niratan::test {

inline constexpr qint64 added_id = 9007199254740993LL;

struct AnkiStub {
    QTcpServer server;
    QList<QJsonObject> requests;
    QHash<QString, QJsonObject> responses{
        {"canAddNotesWithErrorDetail", {{"result", QJsonArray{QJsonObject{{"canAdd", true}}}}}},
        {"findNotes", {{"result", QJsonArray{added_id, 37}}}},
        {"getMediaDirPath", {{"result", QJsonValue::Null}}},
        {"addNote", {{"result", added_id}}},
        {"guiBrowse", {{"result", QJsonArray{}}}},
    };
    QHash<QString, QJsonObject> model_field_responses;
    QString deferred_action;
    QPointer<QTcpSocket> deferred_socket;

    AnkiStub() {
        QObject::connect(&server, &QTcpServer::newConnection, &server, [this] {
            while (server.hasPendingConnections()) {
                auto* socket = server.nextPendingConnection();
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                 [this, socket, raw = QByteArray{}]() mutable {
                                     raw += socket->readAll();
                                     receive(socket, raw);
                                 });
            }
        });
        server.listen(QHostAddress::LocalHost, 0);
    }

    std::string url() const { return "http://127.0.0.1:" + std::to_string(server.serverPort()); }

    QStringList actions() const {
        QStringList result;
        for (const auto& request : requests) {
            result.append(request.value("action").toString());
        }
        return result;
    }

    QJsonObject params(const QString& action) const {
        for (const auto& request : requests) {
            if (request.value("action").toString() == action) {
                return request.value("params").toObject();
            }
        }
        return {};
    }

    void release_response() {
        if (deferred_socket) {
            respond(deferred_socket, deferred_request_);
            deferred_socket.clear();
        }
    }

   private:
    void respond(QTcpSocket* socket, const QJsonObject& request) const {
        const QString action = request.value("action").toString();
        auto response = responses.value(action, {{"error", "Unexpected action"}});
        if (action == "modelFieldNames") {
            const auto model = request.value("params").toObject().value("modelName").toString();
            response = model_field_responses.value(model, response);
        }
        const auto body = QJsonDocument(response).toJson(QJsonDocument::Compact);
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                      QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }

    void receive(QTcpSocket* socket, const QByteArray& raw) {
        const auto header_end = raw.indexOf("\r\n\r\n");
        if (header_end < 0) {
            return;
        }
        for (const auto& line : raw.left(header_end).split('\n')) {
            if (!line.toLower().startsWith("content-length:")) {
                continue;
            }
            bool ok = false;
            const auto length = line.mid(15).trimmed().toLongLong(&ok);
            const auto body_start = header_end + 4;
            if (!ok || length < 0 || raw.size() - body_start < length) {
                return;
            }
            const auto request = QJsonDocument::fromJson(raw.mid(body_start, length)).object();
            const auto action = request.value("action").toString();
            requests.append(request);
            if (!deferred_action.isEmpty() && action == deferred_action) {
                deferred_socket = socket;
                deferred_request_ = request;
            } else {
                respond(socket, request);
            }
            return;
        }
    }

    QJsonObject deferred_request_;
};

}  // namespace niratan::test
