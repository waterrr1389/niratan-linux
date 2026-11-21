// SPDX-License-Identifier: GPL-3.0-or-later

#include "web_audio_download.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVariant>

#include <utility>

namespace niratan::audio {

void WebAudioDownload::get(const QUrl& url, Callback callback) {
    const auto scheme = url.scheme();
    if (!url.isValid() || (scheme != "http" && scheme != "https" && scheme != "file")) {
        callback(std::nullopt);
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = network_.get(request);
    QObject::connect(
        reply, &QNetworkReply::finished, &network_, [reply, callback = std::move(callback)] {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const int error = reply->error();
            const bool http_error_response =
                status >= 400 && ((error >= 201 && error <= 299) || error >= 401);
            std::optional<QByteArray> data;
            if (error == QNetworkReply::NoError || http_error_response) {
                data = reply->readAll();
            }
            reply->deleteLater();
            callback(std::move(data));
        });
}

}  // namespace niratan::audio
