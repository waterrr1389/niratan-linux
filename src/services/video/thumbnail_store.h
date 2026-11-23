// SPDX-License-Identifier: GPL-3.0-or-later
//
// Generate and cache one poster frame per local video, following
// Features/Video/VideoThumbnailStore.swift: `VideoThumbnailRequest`,
// `VideoThumbnailStore.maximumDimension`, `cachedThumbnailURL`,
// `generateThumbnailURL`, `invalidateThumbnail`, `cacheURL`, both `cacheKey`
// overloads, `defaultCacheDirectory` and `fnv1a64`, and
// Features/Video/Playback/HSMpvClient.mm's
// `HSMpvRenderThumbnailPNGData` / `HSMpvPNGDataByLimitingMaximumDimension`.
//
// The cache directory is <cache root>/VideoThumbnails (Decision D1), not
// Application Support. The cache file is <directory>/<key>.png where the key
// is sixteen lowercase hex digits of FNV-1a 64 over the UTF-8 identity
// "<path>|<fileSize>|<modified>". `modified` is the absolute seconds since
// 1970 with one fractional digit at minimum: a whole second prints as "N.0",
// a millisecond value as "N.f" with trailing zeros dropped. An item without
// a modified date contributes "0.0". A date is only millisecond precise in Qt,
// so a sub-millisecond date hashes differently from Swift's Double (Decision
// D6); the cache is disposable.
//
// `cached_url` only reports an existing file and never creates anything.
// `generate_url` returns a cache hit first, refuses a missing source, and
// otherwise calls the generator with 384, creates the cache directory with
// parents and replaces the PNG atomically. Any failure returns neither a path
// nor a file. An empty path for the cache directory (no HOME and no
// XDG_CACHE_HOME) means generation publishes nothing (Decision D4).
//
// The generator runs ffmpeg without a shell (Decision D2):
//   ffmpeg -nostdin -y -ss 5 -i <source> -frames:v 1
//          -vf scale='min(384,iw)':'min(384,ih)':force_original_aspect_ratio=decrease
//          -f image2pipe -c:v png -
// The scale value is one argument with its single quotes. When the 5 s pass
// yields no bytes the same command runs with -ss 0; a second empty pass, an
// empty executable result or a cancelled run returns empty bytes (Decision
// D8). The store and scheduler treat empty bytes as failure; tests inject
// their own generator (Decision D7/B18).

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>

#include "services/video/library_catalog.h"

namespace niratan::video {

inline constexpr int kThumbnailMaximumDimension = 384;

enum class ThumbnailRequestMode { cache_only, generate_if_missing };

enum class ThumbnailSuspendReason { playback, lookup, mining };

using ThumbnailCancelled = std::function<bool()>;

struct ThumbnailRequest {
    QString path;
    qint64 file_size = 0;
    std::optional<QDateTime> modified_at;

    static ThumbnailRequest for_item(const LibraryItem& item);

    bool operator==(const ThumbnailRequest&) const = default;
};

// `generate` runs on a worker thread and polls `is_cancelled` until it
// returns. Empty bytes mean failure; the cancellation flag is authoritative
// once set.
class ThumbnailGenerator {
   public:
    virtual ~ThumbnailGenerator() = default;

    virtual QByteArray generate(const QString& path, int maximum_dimension,
                                const ThumbnailCancelled& is_cancelled) = 0;
};

// The argument list of one ffmpeg pass; `start_seconds` is 5 or 0.
QStringList thumbnail_generator_arguments(const QString& source_path, int maximum_dimension,
                                          int start_seconds);

class FfmpegThumbnailGenerator : public ThumbnailGenerator {
   public:
    // An empty executable means "ffmpeg" on PATH.
    explicit FfmpegThumbnailGenerator(QString executable = {});

    QByteArray generate(const QString& path, int maximum_dimension,
                        const ThumbnailCancelled& is_cancelled) override;

   private:
    QByteArray run(const QString& path, int maximum_dimension, int start_seconds,
                   const ThumbnailCancelled& is_cancelled) const;

    QString executable_;
};

// <cache root>/VideoThumbnails, empty when the cache root is empty.
std::filesystem::path default_thumbnail_cache_directory();

QString thumbnail_cache_key(const ThumbnailRequest& request);

class ThumbnailStore {
   public:
    // The generator is not owned by the store; the production default is the
    // ffmpeg generator when nullptr is passed.
    explicit ThumbnailStore(std::filesystem::path cache_directory,
                            std::shared_ptr<ThumbnailGenerator> generator = {});

    std::filesystem::path cache_url(const ThumbnailRequest& request) const;
    std::optional<std::filesystem::path> cached_url(const ThumbnailRequest& request) const;
    std::optional<std::filesystem::path> generate_url(const ThumbnailRequest& request,
                                                      const ThumbnailCancelled& is_cancelled = {});
    void invalidate(const ThumbnailRequest& request);

    int maximum_dimension() const { return kThumbnailMaximumDimension; }

   private:
    std::filesystem::path cache_directory_;
    std::shared_ptr<ThumbnailGenerator> generator_;
};

}  // namespace niratan::video
