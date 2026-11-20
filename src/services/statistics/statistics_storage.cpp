// SPDX-License-Identifier: GPL-3.0-or-later

#include "statistics_storage.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>
#include <QUuid>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "services/storage/json_file.h"

namespace niratan::statistics {
namespace {

constexpr const char* kSessionsFileName = "statistics_sessions.json";
constexpr const char* kDailyFileName = "statistics.json";
constexpr const char* kMetadataFileName = "metadata.json";

// StatisticsStorage.SessionsFile: the source of truth plus the stamps of the
// daily rows this module last wrote.
struct SessionsFile {
    ReadingSessionRecords records;
    std::map<std::string, std::int64_t, std::less<>> exported_daily;
};

QJsonValue field(const QJsonObject& object, const char* key) {
    return object.value(QLatin1StringView(key));
}

bool decode_int(const QJsonValue& value, int& result) {
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number) {
        return false;
    }
    if (number < static_cast<double>(std::numeric_limits<int>::min()) ||
        number > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    result = static_cast<int>(number);
    return true;
}

bool decode_int64(const QJsonValue& value, std::int64_t& result) {
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number) {
        return false;
    }
    // 2^63 is the first double above the largest int64, so the bound is
    // exclusive on the upper side.
    constexpr double kInt64Limit = 9223372036854775808.0;
    if (number < -kInt64Limit || number >= kInt64Limit) {
        return false;
    }
    result = static_cast<std::int64_t>(number);
    return true;
}

bool decode_double(const QJsonValue& value, double& result) {
    if (!value.isDouble()) {
        return false;
    }
    result = value.toDouble();
    return true;
}

bool decode_string(const QJsonValue& value, std::string& result) {
    if (!value.isString()) {
        return false;
    }
    result = value.toString().toStdString();
    return true;
}

bool decode_session(const QJsonValue& value, ReadingSession& session) {
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    return decode_int64(field(object, "startedAt"), session.started_at) &&
           decode_int64(field(object, "endedAt"), session.ended_at) &&
           decode_int(field(object, "charactersRead"), session.characters_read) &&
           decode_double(field(object, "readingTime"), session.reading_time);
}

// Timestamped<ReadingSession?>: the synthesized decoding asks for "value" even
// when it is optional, so the key must be present; null is the tombstone.
bool decode_record(const QJsonValue& value, TimestampedSession& record) {
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!decode_int64(field(object, "modified"), record.modified)) {
        return false;
    }
    if (!object.contains(QLatin1StringView("value"))) {
        return false;
    }
    const QJsonValue stored = field(object, "value");
    if (stored.isNull()) {
        record.value = std::nullopt;
        return true;
    }
    ReadingSession session;
    if (!decode_session(stored, session)) {
        return false;
    }
    record.value = std::move(session);
    return true;
}

bool decode_statistics(const QJsonValue& value, Statistics& row) {
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    Statistics decoded;
    if (!decode_string(field(object, "title"), decoded.title) ||
        !decode_string(field(object, "dateKey"), decoded.date_key) ||
        !decode_int(field(object, "charactersRead"), decoded.characters_read) ||
        !decode_double(field(object, "readingTime"), decoded.reading_time) ||
        !decode_int(field(object, "minReadingSpeed"), decoded.min_reading_speed) ||
        !decode_int(field(object, "altMinReadingSpeed"), decoded.alt_min_reading_speed) ||
        !decode_int(field(object, "lastReadingSpeed"), decoded.last_reading_speed) ||
        !decode_int(field(object, "maxReadingSpeed"), decoded.max_reading_speed) ||
        !decode_int64(field(object, "lastStatisticModified"),
                      decoded.last_statistic_modified)) {
        return false;
    }
    row = std::move(decoded);
    return true;
}

QJsonObject encode_session(const ReadingSession& session) {
    QJsonObject object;
    object.insert(QLatin1StringView("startedAt"),
                  static_cast<qint64>(session.started_at));
    object.insert(QLatin1StringView("endedAt"), static_cast<qint64>(session.ended_at));
    object.insert(QLatin1StringView("charactersRead"), session.characters_read);
    object.insert(QLatin1StringView("readingTime"), session.reading_time);
    return object;
}

QJsonObject encode_record(const TimestampedSession& record) {
    QJsonObject object;
    object.insert(QLatin1StringView("modified"), static_cast<qint64>(record.modified));
    QJsonValue value;
    if (record.value.has_value()) {
        value = encode_session(*record.value);
    }
    object.insert(QLatin1StringView("value"), value);
    return object;
}

QJsonObject encode_statistics(const Statistics& row) {
    QJsonObject object;
    object.insert(QLatin1StringView("title"), QString::fromStdString(row.title));
    object.insert(QLatin1StringView("dateKey"), QString::fromStdString(row.date_key));
    object.insert(QLatin1StringView("charactersRead"), row.characters_read);
    object.insert(QLatin1StringView("readingTime"), row.reading_time);
    object.insert(QLatin1StringView("minReadingSpeed"), row.min_reading_speed);
    object.insert(QLatin1StringView("altMinReadingSpeed"), row.alt_min_reading_speed);
    object.insert(QLatin1StringView("lastReadingSpeed"), row.last_reading_speed);
    object.insert(QLatin1StringView("maxReadingSpeed"), row.max_reading_speed);
    object.insert(QLatin1StringView("lastStatisticModified"),
                  static_cast<qint64>(row.last_statistic_modified));
    return object;
}

// A document of the wrong shape is returned as none; a decode failure rejects
// the whole file, as synthesized Codable does.
std::optional<std::vector<Statistics>> load_daily(const std::filesystem::path& root) {
    const storage::JsonReadResult read =
        storage::read_json_file(root / kDailyFileName);
    if (read.status != storage::JsonReadStatus::success || !read.document.isArray()) {
        return std::nullopt;
    }
    std::vector<Statistics> rows;
    const QJsonArray array = read.document.array();
    rows.reserve(static_cast<std::size_t>(array.size()));
    for (const QJsonValue& value : array) {
        Statistics row;
        if (!decode_statistics(value, row)) {
            return std::nullopt;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

std::optional<SessionsFile> load_sessions_file(const std::filesystem::path& root) {
    const storage::JsonReadResult read =
        storage::read_json_file(root / kSessionsFileName);
    if (read.status != storage::JsonReadStatus::success || !read.document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = read.document.object();
    const QJsonValue sessions_value = field(object, "sessions");
    const QJsonValue exported_value = field(object, "exportedDaily");
    if (!sessions_value.isObject() || !exported_value.isObject()) {
        return std::nullopt;
    }

    SessionsFile file;
    const QJsonObject sessions_object = sessions_value.toObject();
    for (auto entry = sessions_object.begin(); entry != sessions_object.end();
         ++entry) {
        TimestampedSession record;
        if (!decode_record(entry.value(), record)) {
            return std::nullopt;
        }
        file.records.emplace(entry.key().toStdString(), std::move(record));
    }
    const QJsonObject exported_object = exported_value.toObject();
    for (auto entry = exported_object.begin(); entry != exported_object.end();
         ++entry) {
        std::int64_t stamp = 0;
        if (!decode_int64(entry.value(), stamp)) {
            return std::nullopt;
        }
        file.exported_daily.emplace(entry.key().toStdString(), stamp);
    }
    return file;
}

// StatisticsStorage.title(root:daily:): the first non-empty stored row title,
// then metadata.json's renamedTitle, then its title.
std::string resolve_title(const std::filesystem::path& root,
                          const std::optional<std::vector<Statistics>>& daily) {
    if (daily.has_value()) {
        for (const Statistics& row : *daily) {
            if (!row.title.empty()) {
                return row.title;
            }
        }
    }

    const storage::JsonReadResult read =
        storage::read_json_file(root / kMetadataFileName);
    if (read.status != storage::JsonReadStatus::success || !read.document.isObject()) {
        return {};
    }
    const QJsonObject object = read.document.object();
    const QJsonValue renamed = field(object, "renamedTitle");
    if (renamed.isString()) {
        return renamed.toString().toStdString();
    }
    const QJsonValue title = field(object, "title");
    if (title.isString()) {
        return title.toString().toStdString();
    }
    return {};
}

// Swift's root.lastPathComponent: the final component with trailing separators
// ignored.
std::string book_key(const std::filesystem::path& root) {
    std::filesystem::path path = root;
    while (!path.empty()) {
        const std::filesystem::path name = path.filename();
        if (!name.empty()) {
            return name.string();
        }
        const std::filesystem::path parent = path.parent_path();
        if (parent == path) {
            break;
        }
        path = parent;
    }
    return {};
}

std::string new_session_id() {
    // Swift generates a random UUID here; the case matches UUID.uuidString.
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

// ReadingSessionLog.importingDaily. Rows are deduplicated and visited in
// ascending date-key order; a visited row takes its day out of the untouched
// days that Replace tombstones at the end.
ReadingSessionRecords importing_daily(const std::vector<Statistics>& statistics,
                                      const ReadingSessionRecords& records,
                                      std::string_view key, StatisticsSyncMode mode,
                                      int reset_minutes, const QTimeZone& zone,
                                      std::int64_t now_ms) {
    ReadingSessionRecords updated = records;

    std::map<std::string, ReadingSessionDay, std::less<>> days;
    for (ReadingSessionDay& day : session_days(records, reset_minutes, zone)) {
        days.emplace(day.date_key, std::move(day));
    }

    std::vector<Statistics> imported_rows = deduplicated_statistics(statistics);
    std::ranges::sort(imported_rows, {}, &Statistics::date_key);

    for (const Statistics& imported : imported_rows) {
        std::optional<ReadingSessionDay> previous;
        std::vector<std::string> previous_ids;
        const auto day_position = days.find(imported.date_key);
        if (day_position != days.end()) {
            previous = std::move(day_position->second);
            days.erase(day_position);
            previous_ids.reserve(previous->sessions.size());
            for (const ReadingSessionEntry& entry : previous->sessions) {
                previous_ids.push_back(entry.id);
            }
        }

        const std::int64_t modified = imported.last_statistic_modified;
        if (mode == StatisticsSyncMode::merge && previous.has_value() &&
            previous->last_modified() >= modified) {
            continue;
        }

        const bool has_activity =
            imported.characters_read > 0 || imported.reading_time > 0.0;
        if (!has_activity && previous_ids.empty()) {
            continue;
        }

        const std::optional<ReadingSession> value =
            has_activity ? legacy_session(imported, reset_minutes, zone)
                         : std::nullopt;

        // An unchanged Replace day keeps its existing stamps.
        if (mode == StatisticsSyncMode::replace && previous_ids.size() == 1 &&
            value.has_value() &&
            previous->sessions.front().session == *value) {
            continue;
        }

        std::string id;
        if (!previous_ids.empty()) {
            id = *std::ranges::min_element(previous_ids);
        } else {
            id = legacy_session_id(key, imported.date_key);
        }
        const bool id_existed =
            std::ranges::find(previous_ids, id) != previous_ids.end();
        if (!id_existed && updated.contains(id)) {
            id = new_session_id();
        }

        const std::int64_t timestamp =
            mode == StatisticsSyncMode::replace ? now_ms : modified;
        for (const std::string& previous_id : previous_ids) {
            if (previous_id != id || !value.has_value()) {
                updated[previous_id] =
                    TimestampedSession{timestamp, std::nullopt};
            }
        }
        if (value.has_value()) {
            updated[id] = TimestampedSession{timestamp, value};
        }
    }

    if (mode == StatisticsSyncMode::replace) {
        for (const auto& day : days) {
            for (const ReadingSessionEntry& entry : day.second.sessions) {
                updated[entry.id] = TimestampedSession{now_ms, std::nullopt};
            }
        }
    }

    return updated;
}

bool write_statistics(const std::filesystem::path& root,
                      const ReadingSessionRecords& records, std::string_view title,
                      int reset_minutes, const QTimeZone& zone, std::string* error) {
    const std::vector<Statistics> daily =
        daily_statistics(records, title, reset_minutes, zone);

    std::map<std::string, std::int64_t, std::less<>> exported_daily;
    for (const Statistics& row : daily) {
        const auto [position, inserted] = exported_daily.try_emplace(
            row.date_key, row.last_statistic_modified);
        if (!inserted) {
            position->second = std::max(position->second, row.last_statistic_modified);
        }
    }

    QJsonObject sessions;
    for (const auto& [id, record] : records) {
        sessions.insert(QString::fromStdString(id), encode_record(record));
    }
    QJsonObject exported;
    for (const auto& [date_key, stamp] : exported_daily) {
        exported.insert(QString::fromStdString(date_key),
                        static_cast<qint64>(stamp));
    }
    QJsonObject file;
    file.insert(QLatin1StringView("sessions"), sessions);
    file.insert(QLatin1StringView("exportedDaily"), exported);

    // The sessions are the source of truth, so they reach the disk first.
    if (!storage::write_json_file(root / kSessionsFileName,
                                  QJsonDocument(file), error)) {
        return false;
    }

    QJsonArray rows;
    for (const Statistics& row : daily) {
        rows.append(encode_statistics(row));
    }
    return storage::write_json_file(root / kDailyFileName,
                                    QJsonDocument(rows), error);
}

}  // namespace

std::string_view statistics_sessions_file_name() { return kSessionsFileName; }

std::string_view statistics_daily_file_name() { return kDailyFileName; }

std::int64_t current_time_milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

StatisticsReadResult load_statistics(const std::filesystem::path& book_root, int reset_minutes,
                                     const QTimeZone& zone) {
    const std::optional<std::vector<Statistics>> daily_rows = load_daily(book_root);
    const std::optional<SessionsFile> sessions_file = load_sessions_file(book_root);
    const std::string title = resolve_title(book_root, daily_rows);

    StatisticsReadResult result;
    if (sessions_file.has_value()) {
        std::vector<Statistics> external;
        for (Statistics& row :
             deduplicated_statistics(daily_rows.value_or(std::vector<Statistics>{}))) {
            const auto stamp = sessions_file->exported_daily.find(row.date_key);
            if (stamp == sessions_file->exported_daily.end() ||
                stamp->second != row.last_statistic_modified) {
                external.push_back(std::move(row));
            }
        }

        if (external.empty()) {
            result.records = sessions_file->records;
        } else {
            result.records =
                importing_daily(external, sessions_file->records, book_key(book_root),
                                StatisticsSyncMode::merge, reset_minutes, zone, 0);
            result.needs_save = true;
        }
    } else if (daily_rows.has_value() && !daily_rows->empty()) {
        result.records =
            legacy_sessions(*daily_rows, book_key(book_root), reset_minutes, zone);
        result.needs_save = true;
    }

    result.daily = daily_statistics(result.records, title, reset_minutes, zone);
    return result;
}

std::vector<Statistics> read_daily_statistics(const std::filesystem::path& book_root,
                                              int reset_minutes, const QTimeZone& zone) {
    const std::optional<std::vector<Statistics>> daily_rows = load_daily(book_root);
    const std::optional<SessionsFile> sessions_file = load_sessions_file(book_root);
    if (sessions_file.has_value()) {
        return daily_statistics(sessions_file->records,
                                resolve_title(book_root, daily_rows), reset_minutes, zone);
    }
    if (daily_rows.has_value()) {
        return visible_statistics(*daily_rows);
    }
    return {};
}

bool save_statistics(const std::filesystem::path& book_root,
                     const ReadingSessionRecords& records, std::string_view title,
                     int reset_minutes, const QTimeZone& zone, std::string* error) {
    return write_statistics(book_root, records, title, reset_minutes, zone, error);
}

bool import_daily_statistics(const std::filesystem::path& book_root,
                             const std::vector<Statistics>& statistics, StatisticsSyncMode mode,
                             int reset_minutes, const QTimeZone& zone,
                             std::int64_t now_ms, std::string* error) {
    const StatisticsReadResult current = load_statistics(book_root, reset_minutes, zone);
    const ReadingSessionRecords updated =
        importing_daily(statistics, current.records, book_key(book_root), mode,
                        reset_minutes, zone, now_ms);
    if (updated == current.records) {
        return true;
    }

    const std::string title =
        statistics.empty() ? resolve_title(book_root, load_daily(book_root))
                           : statistics.front().title;
    return write_statistics(book_root, updated, title, reset_minutes, zone, error);
}

bool has_statistics(const std::filesystem::path& book_root) {
    std::error_code error;
    if (std::filesystem::exists(book_root / kSessionsFileName, error)) {
        return true;
    }
    error.clear();
    return std::filesystem::exists(book_root / kDailyFileName, error);
}

}  // namespace niratan::statistics
