// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QQueue>
#include <QTcpServer>
#include <QUrl>
#include <QUrlQuery>

class FakeDriveServer : public QTcpServer {
   public:
    struct Request {
        QByteArray method;
        QString path;
        QUrlQuery query;
        QHash<QByteArray, QByteArray> headers;
        QByteArray body;
    };

    struct Reply {
        int status;
        QByteArray body;
        QByteArray content_type = "application/json";
    };

    FakeDriveServer();
    bool start();
    QUrl base_url() const;
    void enqueue(Reply reply);
    const QList<Request>& requests() const;

   private:
    QQueue<Reply> replies_;
    QList<Request> requests_;
};
