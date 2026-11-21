// SPDX-License-Identifier: GPL-3.0-or-later
//
// The contract is in mining_history.h. Decoding follows the Codable
// conformance in Features/Video/VideoMiningHistoryStore.swift
// (VideoMiningHistoryItem.init(from:)) and RemoteVideoSource.swift
// (RemoteVideoIdentity.init(from:)); the synthesized decoder rejects a member
// whose type is wrong, and a subtitleFormat outside SubtitleFormat's raw values
// rejects the document, while an absent or null optional member is absent.

#include "mining_history.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QLocale>
#include <QString>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "services/storage/json_file.h"

namespace niratan::player {
namespace {

QString text(const std::string& value) {
    return QString::fromStdString(value);
}

// Synthesized Codable decodes an optional property from an absent key and from
// null alike, and rejects a value of any other type. The helpers return false
// only for the last case.
bool optional_string(const QJsonObject& object, QLatin1StringView key,
                     std::optional<std::string>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    if (!value.isString()) {
        return false;
    }
    *out = value.toString().toStdString();
    return true;
}

bool required_string(const QJsonObject& object, QLatin1StringView key, std::string* out) {
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return false;
    }
    *out = value.toString().toStdString();
    return true;
}

bool required_number(const QJsonObject& object, QLatin1StringView key, double* out) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    *out = value.toDouble();
    return true;
}

bool optional_number(const QJsonObject& object, QLatin1StringView key,
                     std::optional<double>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    *out = value.toDouble();
    return true;
}

// An integer property decodes only from a number without a fractional part that
// fits the property's range; anything else fails instead of saturating.
std::optional<int> integer_value(const QJsonValue& value) {
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    constexpr double kUpperBound = 2147483648.0;  // 2^31, one past INT_MAX
    if (!std::isfinite(number) || std::trunc(number) != number || number < -kUpperBound ||
        number >= kUpperBound) {
        return std::nullopt;
    }
    return static_cast<int>(number);
}

bool optional_integer(const QJsonObject& object, QLatin1StringView key,
                      std::optional<int>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    const std::optional<int> number = integer_value(value);
    if (!number) {
        return false;
    }
    *out = *number;
    return true;
}

// SubtitleFormat's raw values; decoding rejects any other string, so a stored
// format this port does not know is not silently dropped.
bool is_subtitle_format(const QString& value) {
    return value == QLatin1StringView("srt") || value == QLatin1StringView("webVTT") ||
           value == QLatin1StringView("ass") || value == QLatin1StringView("ssa") ||
           value == QLatin1StringView("embedded");
}

bool optional_subtitle_format(const QJsonObject& object, QLatin1StringView key,
                              std::optional<std::string>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    if (!value.isString() || !is_subtitle_format(value.toString())) {
        return false;
    }
    *out = value.toString().toStdString();
    return true;
}

bool host_is(const QString& host, const QString& domain) {
    return host == domain || host.endsWith(QLatin1Char('.') + domain);
}

// RemoteVideoIdentity.isYouTube uses YouTubeURLParser, which also checks the
// path shape, youtube-nocookie.com and an eleven-character video id. A history
// record only needs the host, so that is what the rewrite below tests.
bool is_youtube_url(const std::string& url) {
    const QUrl parsed{QString::fromStdString(url)};
    const QString scheme = parsed.scheme().toLower();
    if (scheme != QLatin1StringView("http") && scheme != QLatin1StringView("https")) {
        return false;
    }
    const QString host = parsed.host().toLower();
    if (host.isEmpty()) {
        return false;
    }
    return host_is(host, QStringLiteral("youtube.com")) || host_is(host, QStringLiteral("youtu.be"));
}

// providerID wins over the legacy "provider" member; when both are absent or
// null the identity cannot decode, and a wrongly typed member rejects it.
bool provider_id_from_json(const QJsonObject& object, std::string* out) {
    const QJsonValue provider_id = object.value(QLatin1StringView("providerID"));
    if (provider_id.isUndefined() || provider_id.isNull()) {
        const QJsonValue legacy = object.value(QLatin1StringView("provider"));
        if (!legacy.isString()) {
            return false;
        }
        *out = legacy.toString().toStdString();
        return true;
    }
    if (!provider_id.isString()) {
        return false;
    }
    *out = provider_id.toString().toStdString();
    return true;
}

bool optional_identity(const QJsonObject& object, QLatin1StringView key,
                       std::optional<MiningHistoryItem::RemoteVideoIdentity>* out) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        out->reset();
        return true;
    }
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject identity = value.toObject();

    MiningHistoryItem::RemoteVideoIdentity decoded;
    if (!required_string(identity, QLatin1StringView("remoteID"), &decoded.remote_id) ||
        !required_string(identity, QLatin1StringView("originalURL"), &decoded.original_url) ||
        !required_string(identity, QLatin1StringView("title"), &decoded.title) ||
        !optional_string(identity, QLatin1StringView("canonicalURL"), &decoded.canonical_url) ||
        !optional_string(identity, QLatin1StringView("thumbnailURL"), &decoded.thumbnail_url) ||
        !optional_number(identity, QLatin1StringView("duration"), &decoded.duration) ||
        !provider_id_from_json(identity, &decoded.provider_id)) {
        return false;
    }

    // A ytdlp source is recorded as YouTube when the canonical URL, else the
    // original URL, is a YouTube URL, so later playback uses the native path.
    if (decoded.provider_id == "ytdlp") {
        const std::string& url =
            decoded.canonical_url.has_value() ? *decoded.canonical_url : decoded.original_url;
        if (is_youtube_url(url)) {
            decoded.provider_id = "youtube";
        }
    }

    *out = std::move(decoded);
    return true;
}

std::optional<MiningHistoryItem> item_from_json(const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();

    MiningHistoryItem item;
    if (!required_string(object, QLatin1StringView("id"), &item.id) ||
        !required_number(object, QLatin1StringView("createdAt"), &item.created_at) ||
        !required_string(object, QLatin1StringView("subtitleText"), &item.subtitle_text) ||
        !required_string(object, QLatin1StringView("videoFileName"), &item.video_file_name) ||
        !required_number(object, QLatin1StringView("cueStart"), &item.cue_start) ||
        !required_number(object, QLatin1StringView("cueEnd"), &item.cue_end) ||
        !optional_string(object, QLatin1StringView("videoPath"), &item.video_path) ||
        !optional_identity(object, QLatin1StringView("remoteVideoIdentity"),
                           &item.remote_video_identity) ||
        !optional_string(object, QLatin1StringView("subtitleSourcePath"),
                         &item.subtitle_source_path) ||
        !optional_subtitle_format(object, QLatin1StringView("subtitleFormat"),
                                  &item.subtitle_format) ||
        !optional_integer(object, QLatin1StringView("embeddedSubtitleTrackID"),
                          &item.embedded_subtitle_track_id)) {
        return std::nullopt;
    }

    // A record written before these members existed falls back to the video file
    // name, as VideoMiningHistoryItem.init(from:) does.
    std::optional<std::string> video_title;
    std::optional<std::string> source_name;
    if (!optional_string(object, QLatin1StringView("videoTitle"), &video_title) ||
        !optional_string(object, QLatin1StringView("subtitleSourceName"), &source_name)) {
        return std::nullopt;
    }
    item.video_title = video_title.value_or(item.video_file_name);
    item.subtitle_source_name = source_name.value_or(item.video_file_name);

    return item;
}

std::optional<QJsonObject> identity_to_json(
    const MiningHistoryItem::RemoteVideoIdentity& identity) {
    if (identity.duration.has_value() && !std::isfinite(*identity.duration)) {
        return std::nullopt;
    }

    QJsonObject object;
    object.insert(QLatin1StringView("providerID"), text(identity.provider_id));
    object.insert(QLatin1StringView("remoteID"), text(identity.remote_id));
    object.insert(QLatin1StringView("originalURL"), text(identity.original_url));
    if (identity.canonical_url.has_value()) {
        object.insert(QLatin1StringView("canonicalURL"), text(*identity.canonical_url));
    }
    object.insert(QLatin1StringView("title"), text(identity.title));
    if (identity.thumbnail_url.has_value()) {
        object.insert(QLatin1StringView("thumbnailURL"), text(*identity.thumbnail_url));
    }
    if (identity.duration.has_value()) {
        object.insert(QLatin1StringView("duration"), *identity.duration);
    }
    return object;
}

// A non-finite number has no JSON form: the writer would emit null, which
// decoding rejects, so the whole save is refused instead.
std::optional<QJsonObject> item_to_json(const MiningHistoryItem& item) {
    if (!std::isfinite(item.created_at) || !std::isfinite(item.cue_start) ||
        !std::isfinite(item.cue_end)) {
        return std::nullopt;
    }

    QJsonObject object;
    object.insert(QLatin1StringView("id"), text(item.id));
    object.insert(QLatin1StringView("createdAt"), item.created_at);
    object.insert(QLatin1StringView("subtitleText"), text(item.subtitle_text));
    object.insert(QLatin1StringView("videoFileName"), text(item.video_file_name));
    object.insert(QLatin1StringView("videoTitle"), text(item.video_title));
    if (item.video_path.has_value()) {
        object.insert(QLatin1StringView("videoPath"), text(*item.video_path));
    }
    if (item.remote_video_identity.has_value()) {
        const std::optional<QJsonObject> identity = identity_to_json(*item.remote_video_identity);
        if (!identity) {
            return std::nullopt;
        }
        object.insert(QLatin1StringView("remoteVideoIdentity"), *identity);
    }
    object.insert(QLatin1StringView("subtitleSourceName"), text(item.subtitle_source_name));
    if (item.subtitle_source_path.has_value()) {
        object.insert(QLatin1StringView("subtitleSourcePath"), text(*item.subtitle_source_path));
    }
    if (item.subtitle_format.has_value()) {
        object.insert(QLatin1StringView("subtitleFormat"), text(*item.subtitle_format));
    }
    if (item.embedded_subtitle_track_id.has_value()) {
        object.insert(QLatin1StringView("embeddedSubtitleTrackID"),
                      *item.embedded_subtitle_track_id);
    }
    object.insert(QLatin1StringView("cueStart"), item.cue_start);
    object.insert(QLatin1StringView("cueEnd"), item.cue_end);
    return object;
}

// createdAt ascending, then id ascending. A NaN cannot come from JSON, but a
// caller can pass one, so it is ordered explicitly to keep the comparator a
// strict weak ordering.
bool item_before(const MiningHistoryItem& lhs, const MiningHistoryItem& rhs) {
    const bool lhs_nan = std::isnan(lhs.created_at);
    const bool rhs_nan = std::isnan(rhs.created_at);
    if (lhs_nan != rhs_nan) {
        return lhs_nan;
    }
    if (!lhs_nan && lhs.created_at != rhs.created_at) {
        return lhs.created_at < rhs.created_at;
    }
    return lhs.id < rhs.id;
}

}  // namespace

std::filesystem::path mining_history_path(const std::filesystem::path& app_directory) {
    return app_directory / "video_mining_history.json";
}

QString mining_history_date_label(double created_at, const QDateTime& now) {
    constexpr double foundation_epoch = 978307200.0;
    const double elapsed = (now.toMSecsSinceEpoch() / 1000.0 - foundation_epoch) - created_at;
    if (elapsed < 60) {
        return QCoreApplication::translate("PlayerStudyViewModel", "Just now");
    }
    if (elapsed < 3600) {
        return QCoreApplication::translate("PlayerStudyViewModel", "%n min ago", nullptr, int(elapsed / 60));
    }
    if (elapsed < 86400) {
        const int hours = int(elapsed / 3600);
        return hours == 1 ? QCoreApplication::translate("PlayerStudyViewModel", "1 hour ago")
            : QCoreApplication::translate("PlayerStudyViewModel", "%n hours ago", nullptr, hours);
    }
    const double unix_ms = std::floor((created_at + foundation_epoch) * 1000);
    if (!std::isfinite(unix_ms) || unix_ms <= double(std::numeric_limits<qint64>::min()) ||
        unix_ms >= double(std::numeric_limits<qint64>::max())) {
        return {};
    }
    const QDate date = QDateTime::fromMSecsSinceEpoch(qint64(unix_ms), now.timeZone()).date();
    return date == now.date().addDays(-1)
        ? QCoreApplication::translate("PlayerStudyViewModel", "Yesterday")
        : QLocale().toString(date, QLocale::ShortFormat);
}

std::vector<MiningHistoryItem> load(const std::filesystem::path& app_directory) {
    const storage::JsonReadResult read = storage::read_json_file(mining_history_path(app_directory));
    if (read.status != storage::JsonReadStatus::success) {
        return {};
    }

    // A bare array is the stored shape; the "items" wrapper is unwrapped so a
    // file written by an earlier build of the port still reads. Any other
    // document reads as empty, as a failed decode does in loadItems.
    QJsonArray records;
    if (read.document.isArray()) {
        records = read.document.array();
    } else if (read.document.isObject()) {
        const QJsonValue items = read.document.object().value(QLatin1StringView("items"));
        if (!items.isArray()) {
            return {};
        }
        records = items.toArray();
    } else {
        return {};
    }

    std::vector<MiningHistoryItem> items;
    items.reserve(static_cast<std::size_t>(records.size()));
    for (const QJsonValue& entry : records) {
        std::optional<MiningHistoryItem> item = item_from_json(entry);
        if (!item) {
            return {};
        }
        items.push_back(std::move(*item));
    }
    return items;
}

bool save(const std::filesystem::path& app_directory,
          const std::vector<MiningHistoryItem>& items) {
    QJsonArray records;
    for (const MiningHistoryItem& item : items) {
        const std::optional<QJsonObject> record = item_to_json(item);
        if (!record) {
            return false;
        }
        records.append(*record);
    }
    return storage::write_json_file(mining_history_path(app_directory), QJsonDocument(records));
}

std::vector<MiningHistoryItem> pruned_items(std::vector<MiningHistoryItem> items, int limit) {
    if (limit <= 0) {
        return {};
    }

    std::sort(items.begin(), items.end(), item_before);
    if (items.size() > static_cast<std::size_t>(limit)) {
        items.erase(items.begin(), items.end() - static_cast<std::ptrdiff_t>(limit));
    }
    return items;
}

bool add(const std::filesystem::path& app_directory, const MiningHistoryItem& item, int limit) {
    std::vector<MiningHistoryItem> items = load(app_directory);
    items.push_back(item);
    return save(app_directory, pruned_items(std::move(items), limit));
}

bool remove(const std::filesystem::path& app_directory, std::string_view id) {
    std::vector<MiningHistoryItem> items = load(app_directory);
    const auto kept_end = std::remove_if(items.begin(), items.end(),
                                         [id](const MiningHistoryItem& item) {
                                             return std::string_view(item.id) == id;
                                         });
    if (kept_end == items.end()) {
        return false;
    }
    items.erase(kept_end, items.end());
    return save(app_directory, items);
}

bool clear(const std::filesystem::path& app_directory) {
    return save(app_directory, {});
}

}  // namespace niratan::player
