// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveHandler.swift: session, performRequest, performDownloadRequest
// and the multipart upload methods. GoogleDriveAuth.swift: getAccessToken,
// refreshAccessToken.
// Each attempt is one network manager operation. The completion runs once on
// this transport's thread; the transport must outlive it.
// Requests carry Authorization: Bearer <token>. Token errors and offline
// ("No Internet connection.") fail as unavailable without opening a socket.
// The token source is called once; a 401 refreshes and replays once using
// the refresh result. HTTP errors (status >= 400) carry the HTTP status and
// the JSON error.message string when present, or "Request failed with status <code>".
// Download progress follows byte transfer; only success adds an explicit 1.
// Paths join the injected base URL and query values keep their decoded text.

#pragma once

#include "drive_error.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QObject>
#include <QUrl>
#include <QUrlQuery>

#include <chrono>
#include <functional>
#include <memory>
#include <string_view>

namespace niratan::sync {

struct DriveToken {
    std::string value;
    std::string error;
    bool ok() const { return error.empty(); }
};

enum class DriveMethod { get, post, patch };

struct DriveResponse {
    std::optional<QByteArray> body;
    std::optional<GoogleDriveError> error;
    bool ok() const { return body.has_value(); }
};

struct MultipartBody {
    QByteArray body;
    std::string content_type;
};

MultipartBody build_multipart_related(QByteArrayView metadata_json, std::string_view media_type,
                                      QByteArrayView data, std::string_view boundary = {});

class DriveTransport : public QObject {
    Q_OBJECT

   public:
    using TokenSource = std::function<DriveToken()>;
    using RefreshCallback = std::function<void(DriveToken)>;
    using TokenRefresh = std::function<void(RefreshCallback)>;
    using Connectivity = std::function<bool()>;
    using Callback = std::function<void(DriveResponse)>;
    using Progress = std::function<void(double)>;

    DriveTransport(QUrl base_url, TokenSource token, TokenRefresh refresh,
                   QObject* parent = nullptr);
    ~DriveTransport() override;

    void set_connectivity(Connectivity connectivity);  // Empty: always online.
    std::chrono::milliseconds request_timeout() const;

    void request(DriveMethod method, std::string_view path, const QUrlQuery& query, QByteArray body,
                 std::string content_type, Callback callback);
    void download(std::string_view file_id, Progress progress, Callback callback);

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::sync
