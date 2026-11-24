// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QQueue>
#include <QTcpServer>
#include <QUrl>
#include <QUrlQuery>

class QTcpSocket;

class FakeDriveServer : public QTcpServer {
   public:
    struct Request {
        QByteArray method;
        QString path;
        QUrlQuery query;
        QHash<QByteArray, QByteArray> headers;
        QByteArray body;
        QUrlQuery form;
        QJsonObject metadata;
        QByteArray media_type;
        QByteArray media;
    };

    struct Reply {
        int status;
        QByteArray body;
        QByteArray content_type = "application/json";
        bool hold = false;
    };

    FakeDriveServer();
    bool start();
    QUrl base_url() const;
    void enqueue(Reply reply);
    void release();
    void enable_tree(int page_size = 0);
    void put_file(QString id, QString name, QString parent, QByteArray media = {},
                  QString mime_type = QStringLiteral("application/json"));
    const QList<Request>& requests() const;

   private:
    struct File {
        QString id;
        QString name;
        QString parent;
        QByteArray media;
        QString mime_type;
        bool trashed = false;
    };
    Reply tree_reply(const Request& request);
    void respond(QTcpSocket* socket, const Reply& reply);
    QList<File> tree_;
    bool tree_enabled_ = false;
    int page_size_ = 0;
    int next_file_ = 0;
    QPointer<QTcpSocket> held_socket_;
    Reply held_reply_{200, {}};
    QQueue<Reply> replies_;
    QList<Request> requests_;
};
