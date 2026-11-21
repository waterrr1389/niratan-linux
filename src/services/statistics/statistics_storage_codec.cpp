// SPDX-License-Identifier: GPL-3.0-or-later

#include "statistics_storage_codec.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "services/storage/json_file.h"

namespace niratan::statistics::detail {
namespace {

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
    // QJsonValue keeps a parsed integer literal exact, so toInteger() avoids
    // the double round trip. It returns 0 for an integral value that does not
    // fit int64; a genuine zero is the only zero double accepted here.
    const qint64 integer = value.toInteger();
    if (integer == 0 && number != 0.0) {
        return false;
    }
    result = integer;
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
    std::int64_t characters = 0;
    if (!decode_int64(field(object, "startedAt"), session.started_at) ||
        !decode_int64(field(object, "endedAt"), session.ended_at) ||
        !decode_int64(field(object, "charactersRead"), characters) ||
        !decode_double(field(object, "readingTime"), session.reading_time)) {
        return false;
    }
    session.characters_read = static_cast<qint64>(characters);
    return true;
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
    std::int64_t characters = 0;
    if (!decode_string(field(object, "title"), decoded.title) ||
        !decode_string(field(object, "dateKey"), decoded.date_key) ||
        !decode_int64(field(object, "charactersRead"), characters) ||
        !decode_double(field(object, "readingTime"), decoded.reading_time) ||
        !decode_int(field(object, "minReadingSpeed"), decoded.min_reading_speed) ||
        !decode_int(field(object, "altMinReadingSpeed"), decoded.alt_min_reading_speed) ||
        !decode_int(field(object, "lastReadingSpeed"), decoded.last_reading_speed) ||
        !decode_int(field(object, "maxReadingSpeed"), decoded.max_reading_speed) ||
        !decode_int64(field(object, "lastStatisticModified"), decoded.last_statistic_modified)) {
        return false;
    }
    decoded.characters_read = static_cast<qint64>(characters);
    row = std::move(decoded);
    return true;
}

QJsonObject encode_session(const ReadingSession& session) {
    QJsonObject object;
    object.insert(QLatin1StringView("startedAt"), static_cast<qint64>(session.started_at));
    object.insert(QLatin1StringView("endedAt"), static_cast<qint64>(session.ended_at));
    object.insert(QLatin1StringView("charactersRead"),
                  static_cast<qint64>(session.characters_read));
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
    object.insert(QLatin1StringView("charactersRead"), static_cast<qint64>(row.characters_read));
    object.insert(QLatin1StringView("readingTime"), row.reading_time);
    object.insert(QLatin1StringView("minReadingSpeed"), row.min_reading_speed);
    object.insert(QLatin1StringView("altMinReadingSpeed"), row.alt_min_reading_speed);
    object.insert(QLatin1StringView("lastReadingSpeed"), row.last_reading_speed);
    object.insert(QLatin1StringView("maxReadingSpeed"), row.max_reading_speed);
    object.insert(QLatin1StringView("lastStatisticModified"),
                  static_cast<qint64>(row.last_statistic_modified));
    return object;
}

// StatisticsStorage.encode: Data.write(options: .atomic) replaces the target
// wholesale, so even bytes that are not a JSON document are overwritten.
bool write_document(const std::filesystem::path& target, const QJsonDocument& document,
                    std::string* error) {
    QSaveFile file(QString::fromStdString(target.string()));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = file.errorString().toStdString();
        }
        return false;
    }
    const QByteArray bytes = document.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != static_cast<qint64>(bytes.size())) {
        const QString reason = file.errorString();
        file.cancelWriting();
        if (error != nullptr) {
            *error = reason.toStdString();
        }
        return false;
    }
    if (!file.commit()) {
        if (error != nullptr) {
            *error = file.errorString().toStdString();
        }
        return false;
    }
    return true;
}

}  // namespace

std::optional<std::vector<Statistics>> load_daily(const std::filesystem::path& root) {
    const storage::JsonReadResult read =
        storage::read_json_file(root / std::filesystem::path(daily_file_name));
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
        storage::read_json_file(root / std::filesystem::path(sessions_file_name));
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
    for (auto entry = sessions_object.begin(); entry != sessions_object.end(); ++entry) {
        TimestampedSession record;
        if (!decode_record(entry.value(), record)) {
            return std::nullopt;
        }
        file.records.emplace(entry.key().toStdString(), std::move(record));
    }
    const QJsonObject exported_object = exported_value.toObject();
    for (auto entry = exported_object.begin(); entry != exported_object.end(); ++entry) {
        std::int64_t stamp = 0;
        if (!decode_int64(entry.value(), stamp)) {
            return std::nullopt;
        }
        file.exported_daily.emplace(entry.key().toStdString(), stamp);
    }
    return file;
}

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
        storage::read_json_file(root / std::filesystem::path(metadata_file_name));
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

bool write_files(const std::filesystem::path& root, const ReadingSessionRecords& records,
                 std::string_view title, int reset_minutes, const QTimeZone& zone,
                 std::string* error) {
    const std::vector<Statistics> daily = daily_statistics(records, title, reset_minutes, zone);

    std::map<std::string, std::int64_t, std::less<>> exported_daily;
    for (const Statistics& row : daily) {
        const auto [position, inserted] =
            exported_daily.try_emplace(row.date_key, row.last_statistic_modified);
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
        exported.insert(QString::fromStdString(date_key), static_cast<qint64>(stamp));
    }
    QJsonObject file;
    file.insert(QLatin1StringView("sessions"), sessions);
    file.insert(QLatin1StringView("exportedDaily"), exported);

    // The sessions are the source of truth, so they reach the disk first.
    if (!write_document(root / std::filesystem::path(sessions_file_name), QJsonDocument(file),
                        error)) {
        return false;
    }

    QJsonArray rows;
    for (const Statistics& row : daily) {
        rows.append(encode_statistics(row));
    }
    return write_document(root / std::filesystem::path(daily_file_name), QJsonDocument(rows),
                          error);
}

}  // namespace niratan::statistics::detail
