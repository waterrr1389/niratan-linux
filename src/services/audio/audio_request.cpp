// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_request.h"

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

namespace niratan::audio {

std::optional<AudioRequest> parse_audio_request(std::string_view request_url) {
    const QUrl request = QUrl::fromEncoded(
        QByteArray(request_url.data(), static_cast<qsizetype>(request_url.size())));
    if (!request.isValid()) {
        return std::nullopt;
    }

    // The target is the first "url" item, decoded once; later items and other
    // names are ignored, as URLComponents.queryItems?.first(where:) does.
    const QList<QPair<QString, QString>> items =
        QUrlQuery(request).queryItems(QUrl::FullyDecoded);
    QString target_text;
    bool found = false;
    for (const auto& item : items) {
        if (item.first == QStringLiteral("url")) {
            target_text = item.second;
            found = true;
            break;
        }
    }
    if (!found || target_text.isEmpty()) {
        return std::nullopt;
    }

    const QUrl target(target_text);
    if (!target.isValid() || target.isEmpty()) {
        return std::nullopt;
    }

    AudioRequest result;
    result.url = target_text.toStdString();
    result.timeout_ms = target.host() == QStringLiteral("localhost") ? 5000 : 4000;
    return result;
}

}  // namespace niratan::audio
