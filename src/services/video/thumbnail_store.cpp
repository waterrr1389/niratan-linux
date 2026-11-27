// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video poster cache: key, path, atomic write and invalidation. Follows
// Features/Video/VideoThumbnailStore.swift (`VideoThumbnailStore`) as stated
// in thumbnail_store.h.

#include "services/video/thumbnail_store.h"

#include "services/storage/app_roots.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <utility>

namespace niratan::video {
namespace {

constexpr quint64 kFnvOffset = 0xcbf29ce484222325ULL;
constexpr quint64 kFnvPrime = 0x100000001b3ULL;

// Swift interpolates the Double seconds of the modified date: a whole second
// keeps one fractional digit and a millisecond fraction drops trailing zeros.
// A Qt date is millisecond precise, so this matches every representable date
// (Decision D6).
QString modified_identity(const std::optional<QDateTime>& modified_at) {
    if (!modified_at) {
        return QStringLiteral("0.0");
    }
    const qint64 milliseconds = modified_at->toMSecsSinceEpoch();
    const bool negative = milliseconds < 0;
    const quint64 magnitude = negative ? static_cast<quint64>(-(milliseconds + 1)) + 1
                                       : static_cast<quint64>(milliseconds);
    QString text = QString::number(magnitude / 1000);
    text += u'.';
    const quint64 fraction = magnitude % 1000;
    if (fraction == 0) {
        text += u'0';
    } else {
        QString digits = QStringLiteral("%1").arg(fraction, 3, 10, QLatin1Char('0'));
        while (digits.endsWith(u'0')) {
            digits.chop(1);
        }
        text += digits;
    }
    if (negative) {
        text.prepend(u'-');
    }
    return text;
}

QString fnv1a64(const QString& value) {
    quint64 hash = kFnvOffset;
    for (const char byte : value.toUtf8()) {
        hash ^= static_cast<quint64>(static_cast<unsigned char>(byte));
        hash *= kFnvPrime;
    }
    return QStringLiteral("%1").arg(hash, 16, 16, QLatin1Char('0'));
}

}  // namespace

ThumbnailRequest ThumbnailRequest::for_item(const LibraryItem& item) {
    ThumbnailRequest request;
    request.path = item.path;
    request.file_size = item.file_size;
    request.modified_at = item.modified_at;
    return request;
}

std::filesystem::path default_thumbnail_cache_directory() {
    const auto root = storage::cache_root();
    if (root.empty()) {
        return {};
    }
    return root / "VideoThumbnails";
}

QString thumbnail_cache_key(const ThumbnailRequest& request) {
    const QString identity = request.path + u'|' + QString::number(request.file_size) + u'|' +
        modified_identity(request.modified_at);
    return fnv1a64(identity);
}

ThumbnailStore::ThumbnailStore(std::filesystem::path cache_directory,
                               std::shared_ptr<ThumbnailGenerator> generator)
    : cache_directory_(std::move(cache_directory)), generator_(std::move(generator)) {
    if (!generator_) {
        generator_ = std::make_shared<FfmpegThumbnailGenerator>();
    }
}

std::filesystem::path ThumbnailStore::cache_url(const ThumbnailRequest& request) const {
    if (cache_directory_.empty()) {
        return {};
    }
    return cache_directory_ / (thumbnail_cache_key(request).toStdString() + ".png");
}

std::optional<std::filesystem::path>
ThumbnailStore::cached_url(const ThumbnailRequest& request) const {
    const auto url = cache_url(request);
    if (url.empty() || !QFileInfo::exists(QString::fromStdString(url.string()))) {
        return std::nullopt;
    }
    return url;
}

std::optional<std::filesystem::path>
ThumbnailStore::generate_url(const ThumbnailRequest& request,
                             const ThumbnailCancelled& is_cancelled) {
    if (const auto cached = cached_url(request)) {
        return cached;
    }
    if (cache_directory_.empty()) {
        return std::nullopt;
    }
    if (!QFileInfo::exists(request.path)) {
        return std::nullopt;
    }
    if (is_cancelled && is_cancelled()) {
        return std::nullopt;
    }

    const QByteArray bytes = generator_->generate(request.path, maximum_dimension(), is_cancelled);
    if (bytes.isEmpty() || (is_cancelled && is_cancelled())) {
        return std::nullopt;
    }

    std::error_code error;
    std::filesystem::create_directories(cache_directory_, error);
    if (error) {
        return std::nullopt;
    }
    const auto url = cache_url(request);
    QSaveFile file(QString::fromStdString(url.string()));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        return std::nullopt;
    }
    return url;
}

void ThumbnailStore::invalidate(const ThumbnailRequest& request) {
    const auto url = cache_url(request);
    if (!url.empty()) {
        QFile::remove(QString::fromStdString(url.string()));
    }
}

}  // namespace niratan::video
