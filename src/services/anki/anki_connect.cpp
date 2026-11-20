// SPDX-License-Identifier: GPL-3.0-or-later

#include "anki_connect.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariant>

#include <utility>

namespace niratan::anki {

namespace {

AnkiConnectResult parse_reply(QNetworkReply* reply) {
    AnkiConnectResult result;
    const QByteArray data = reply->isOpen() ? reply->readAll() : QByteArray();
    const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    // URLSession has no error for an HTTP status; only a reply without any
    // HTTP response is a transport failure.
    if (!status.isValid() && reply->error() != QNetworkReply::NoError) {
        result.error = reply->errorString().toStdString();
        return result;
    }

    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        result.error = parse_error.errorString().toStdString();
        return result;
    }
    if (!document.isObject()) {
        // JSONSerialization's [String: Any] cast would have produced an empty
        // dictionary, whose result is nil.
        result.ok = true;
        return result;
    }

    const QJsonObject object = document.object();
    const QJsonValue error = object.value("error");
    if (error.isString()) {
        result.error = error.toString().toStdString();
        return result;
    }
    result.ok = true;
    result.result = object.value("result");
    return result;
}

}  // namespace

AnkiConnectClient::AnkiConnectClient(AnkiConnectConfig config, QObject* parent)
    : QObject(parent), config_(std::move(config)), network_(this) {}

AnkiConnectClient::~AnkiConnectClient() = default;

void AnkiConnectClient::request(std::string action, std::optional<QJsonObject> params,
                                Callback callback) {
    const QUrl url(QString::fromStdString(config_.url));
    if (!url.isValid()) {
        AnkiConnectResult result;
        result.error =
            QCoreApplication::translate("AnkiManager", "Invalid URL specified").toStdString();
        if (callback) {
            callback(std::move(result));
        }
        return;
    }

    QJsonObject body;
    body.insert("action", QString::fromStdString(action));
    body.insert("version", 6);
    if (params) {
        body.insert("params", *params);
    }
    if (config_.api_key && !config_.api_key->empty()) {
        body.insert("key", QString::fromStdString(*config_.api_key));
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(config_.timeout_ms);

    QNetworkReply* reply =
        network_.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [reply, callback = std::move(callback)] {
                const AnkiConnectResult result = parse_reply(reply);
                reply->deleteLater();
                if (callback) {
                    callback(result);
                }
            });
}

}  // namespace niratan::anki
