// SPDX-License-Identifier: GPL-3.0-or-later

#include "drive_client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

#include <utility>

namespace niratan::sync {
namespace {

constexpr auto transfer_timeout = std::chrono::seconds(10);

QByteArray bytes(std::string_view text) {
    return QByteArray(text.data(), static_cast<qsizetype>(text.size()));
}

DriveResponse unavailable(std::string message) {
    return {std::nullopt,
            GoogleDriveError{GoogleDriveErrorKind::unavailable, std::move(message), std::nullopt}};
}

DriveResponse api_error(int status, const QByteArray& body) {
    const auto object = QJsonDocument::fromJson(body).object();
    const auto message = object.value("error").toObject().value("message");
    return {std::nullopt,
            GoogleDriveError{GoogleDriveErrorKind::api_error,
                             message.isString()
                                 ? message.toString().toStdString()
                                 : "Request failed with status " + std::to_string(status),
                             status}};
}

struct Operation {
    DriveMethod method;
    QNetworkRequest request;
    QByteArray body;
    DriveTransport::Progress progress;
    DriveTransport::Callback callback;

    void complete(DriveResponse response) {
        if (auto completion = std::exchange(callback, {})) {
            completion(std::move(response));
        }
    }
};

}  // namespace

struct DriveTransport::State {
    DriveTransport* owner;
    QUrl base_url;
    TokenSource token;
    TokenRefresh refresh;
    Connectivity connectivity;
    QNetworkAccessManager manager;

    State(DriveTransport* owner, QUrl base_url, TokenSource token, TokenRefresh refresh)
        : owner(owner), base_url(std::move(base_url)), token(std::move(token)),
          refresh(std::move(refresh)), manager(owner) {}

    void start(DriveMethod method, std::string_view path, const QUrlQuery& query, QByteArray body,
               std::string content_type, Progress progress, Callback callback) {
        auto url = base_url;
        auto full_path = url.path();
        const auto suffix = QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size()));
        if (full_path.endsWith(u'/') && suffix.startsWith(u'/')) {
            full_path.chop(1);
        } else if (!full_path.endsWith(u'/') && !suffix.startsWith(u'/')) {
            full_path += u'/';
        }
        url.setPath(full_path + suffix);
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setTransferTimeout(transfer_timeout);
        if (!content_type.empty()) {
            request.setRawHeader("Content-Type", bytes(content_type));
        }
        auto op = std::make_shared<Operation>(Operation{method, std::move(request), std::move(body),
                                                        std::move(progress), std::move(callback)});
        send(op, token ? token() : DriveToken{{}, "Please sign in"}, true);
    }

    void send(const std::shared_ptr<Operation>& op, DriveToken access_token, bool can_refresh) {
        if (!access_token.ok()) {
            op->complete(unavailable(std::move(access_token.error)));
            return;
        }
        if (connectivity && !connectivity()) {
            op->complete(unavailable("No Internet connection."));
            return;
        }

        op->request.setRawHeader("Authorization", "Bearer " + bytes(access_token.value));
        QNetworkReply* reply = nullptr;
        switch (op->method) {
            case DriveMethod::get:
                reply = manager.get(op->request);
                break;
            case DriveMethod::post:
                reply = manager.post(op->request, op->body);
                break;
            case DriveMethod::patch:
                reply = manager.sendCustomRequest(op->request, "PATCH", op->body);
                break;
        }
        if (op->progress) {
            QObject::connect(reply, &QNetworkReply::downloadProgress, owner,
                             [op](qint64 received, qint64 expected) {
                                 if (received > 0 && expected > 0) {
                                     op->progress(static_cast<double>(received) /
                                                  static_cast<double>(expected));
                                 }
                             });
        }
        QObject::connect(reply, &QNetworkReply::finished, owner, [this, op, reply, can_refresh] {
            reply->deleteLater();
            finish(op, *reply, can_refresh);
        });
    }

    void finish(const std::shared_ptr<Operation>& op, QNetworkReply& reply, bool can_refresh) {
        const auto status_attribute = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const auto status = status_attribute.toInt();
        auto body = reply.readAll();
        if (status == 401 && can_refresh && refresh) {
            refresh_and_replay(op);
        } else if (status >= 400) {
            op->complete(api_error(status, body));
        } else if (reply.error() != QNetworkReply::NoError) {
            op->complete(unavailable(reply.errorString().toStdString()));
        } else if (!status_attribute.isValid()) {
            op->complete({std::nullopt, GoogleDriveError{}});
        } else {
            if (op->progress) {
                op->progress(1);
            }
            op->complete({std::move(body), std::nullopt});
        }
    }

    void refresh_and_replay(const std::shared_ptr<Operation>& op) {
        auto awaiting_refresh = std::make_shared<bool>(true);
        refresh([this, op, awaiting_refresh](DriveToken refreshed) {
            QMetaObject::invokeMethod(
                owner,
                [this, op, awaiting_refresh, refreshed = std::move(refreshed)]() mutable {
                    if (std::exchange(*awaiting_refresh, false)) {
                        send(op, std::move(refreshed), false);
                    }
                },
                Qt::QueuedConnection);
        });
    }
};

DriveTransport::DriveTransport(QUrl base_url, TokenSource token, TokenRefresh refresh,
                               QObject* parent)
    : QObject(parent), state_(std::make_unique<State>(this, std::move(base_url), std::move(token),
                                                      std::move(refresh))) {}

DriveTransport::~DriveTransport() = default;

void DriveTransport::set_connectivity(Connectivity connectivity) {
    state_->connectivity = std::move(connectivity);
}

std::chrono::milliseconds DriveTransport::request_timeout() const {
    return transfer_timeout;
}

void DriveTransport::request(DriveMethod method, std::string_view path, const QUrlQuery& query,
                             QByteArray body, std::string content_type, Callback callback) {
    state_->start(method, path, query, std::move(body), std::move(content_type), {},
                  std::move(callback));
}

void DriveTransport::download(std::string_view file_id, Progress progress, Callback callback) {
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("alt"), QStringLiteral("media"));
    const auto path = "/drive/v3/files/" + std::string(file_id);
    state_->start(DriveMethod::get, path, query, {}, {}, std::move(progress), std::move(callback));
}

MultipartBody build_multipart_related(QByteArrayView metadata_json, std::string_view media_type,
                                      QByteArrayView data, std::string_view boundary) {
    const auto delimiter = boundary.empty()
        ? QUuid::createUuid().toByteArray(QUuid::Id128).toUpper()
        : bytes(boundary);
    QByteArray body;
    body += "--" + delimiter + "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n";
    body.append(metadata_json);
    body += "\r\n--" + delimiter + "\r\nContent-Type: " + bytes(media_type) + "\r\n\r\n";
    body.append(data);
    body += "\r\n--" + delimiter + "--\r\n";
    return {std::move(body), "multipart/related; boundary=" + delimiter.toStdString()};
}

}  // namespace niratan::sync
