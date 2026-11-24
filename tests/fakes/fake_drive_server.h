// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
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
    const QList<Request>& requests() const;

   private:
    void respond(QTcpSocket* socket, const Reply& reply);
    QPointer<QTcpSocket> held_socket_;
    Reply held_reply_{200, {}};
    QQueue<Reply> replies_;
    QList<Request> requests_;
};
