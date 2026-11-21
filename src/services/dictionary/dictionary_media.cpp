// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_media.h"

#include <QMimeDatabase>
#include <QUrlQuery>

#include "dictionary_query.h"
#include "media_type.h"

namespace niratan::dictionary {
namespace {

QByteArray media_mime_type(const QString& path) {
    const auto type = dictionary_media_mime_type(path.toStdString());
    if (type != "application/octet-stream") {
        return QByteArray(type.data(), static_cast<qsizetype>(type.size()));
    }
    const auto name = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();
    if (name == QStringLiteral("audio/vnd.wave")) {
        return QByteArrayLiteral("audio/wav");
    }
    return name.startsWith(QStringLiteral("audio/")) ? name.toLatin1()
                                                     : QByteArrayLiteral("application/octet-stream");
}

}  // namespace

DictionaryMediaService::DictionaryMediaService(std::filesystem::path root)
    : store_(std::move(root)) {}

DictionaryMediaService::~DictionaryMediaService() = default;

MediaResponse DictionaryMediaService::get(const QUrl& url) {
    if (!url.isValid() || url.scheme() != QStringLiteral("dict")) {
        return {};
    }
    const QUrlQuery query(url);
    const QString dictionary = query.queryItemValue(QStringLiteral("dictionary"), QUrl::FullyDecoded);
    const QString path = query.queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded);
    if (dictionary.isEmpty() || path.isEmpty()) {
        return {};
    }

    auto directories = store_.installed();
    if (!query_ || directories_ != directories) {
        query_ = std::make_unique<DictionaryQueryService>("ja", directories);
        directories_ = std::move(directories);
    }
    const std::string bytes = query_->media_file(dictionary.toStdString(), path.toStdString());
    if (bytes.empty()) {
        return {MediaStatus::Missing, {}, {}};
    }
    return {MediaStatus::Found, media_mime_type(path),
            QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size()))};
}

}  // namespace niratan::dictionary
