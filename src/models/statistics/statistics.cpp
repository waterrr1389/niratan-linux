// SPDX-License-Identifier: GPL-3.0-or-later

#include "statistics.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QLatin1Char>
#include <QString>
#include <QTime>
#include <QTimeZone>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace niratan::statistics {
namespace {

void sort_by_date_key(std::vector<Statistics>& rows) {
    std::ranges::sort(rows, {}, &Statistics::date_key);
}

// Swift's Int(String): an optional sign followed by ASCII digits, and nil when
// the digits overflow the machine word.
std::optional<std::int64_t> parse_integer(std::string_view part) {
    if (part.empty()) {
        return std::nullopt;
    }
    std::size_t index = 0;
    bool negative = false;
    if (part.front() == '+' || part.front() == '-') {
        negative = part.front() == '-';
        index = 1;
    }
    if (index == part.size()) {
        return std::nullopt;
    }

    std::int64_t value = 0;
    for (; index < part.size(); ++index) {
        const char character = part[index];
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        const int digit = character - '0';
        if (value > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        value = value * 10 + digit;
    }
    return negative ? -value : value;
}

// ReadingSessionLog.session(for:) splits the key on '-' with empty
// subsequences omitted, drops the components Int cannot parse, and needs
// exactly three values left.
std::optional<QDate> parse_date_key(std::string_view key) {
    std::array<std::int64_t, 3> values{};
    std::size_t value_count = 0;
    std::size_t begin = 0;
    while (true) {
        const std::size_t dash = key.find('-', begin);
        const std::size_t end = dash == std::string_view::npos ? key.size() : dash;
        const std::string_view part = key.substr(begin, end - begin);
        if (!part.empty()) {
            const std::optional<std::int64_t> value = parse_integer(part);
            if (value.has_value()) {
                if (value_count == values.size()) {
                    return std::nullopt;
                }
                values[value_count] = *value;
                ++value_count;
            }
        }
        if (dash == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    if (value_count != values.size()) {
        return std::nullopt;
    }

    for (const std::int64_t value : values) {
        if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
    }
    const QDate date(static_cast<int>(values[0]), static_cast<int>(values[1]),
                     static_cast<int>(values[2]));
    if (!date.isValid()) {
        return std::nullopt;
    }
    return date;
}

// Range clamp, round to the nearest step with the half step away from zero,
// then clamp again, as StatisticsTargetSettings.snap does.
int snap_to_step(int value, int lower, int upper, int step) {
    const int clamped = std::clamp(value, lower, upper);
    const int offset = clamped - lower;
    const int snapped = lower + ((offset + step / 2) / step) * step;
    return std::clamp(snapped, lower, upper);
}

}  // namespace

std::string_view autostart_mode_id(StatisticsAutostartMode mode) {
    switch (mode) {
    case StatisticsAutostartMode::off:
        return "Off";
    case StatisticsAutostartMode::page_turn:
        return "Page Turn";
    case StatisticsAutostartMode::on:
        return "On";
    }
    return {};
}

std::string_view sync_mode_id(StatisticsSyncMode mode) {
    switch (mode) {
    case StatisticsSyncMode::merge:
        return "Merge";
    case StatisticsSyncMode::replace:
        return "Replace";
    }
    return {};
}

std::string_view daily_target_type_id(DailyTargetType type) {
    switch (type) {
    case DailyTargetType::characters:
        return "characters";
    case DailyTargetType::duration:
        return "duration";
    }
    return {};
}

int normalized_reset_minutes(int reset_minutes) {
    return std::clamp(reset_minutes, 0, minutes_per_day - 1);
}

QDate reporting_day(const QDateTime& date, int reset_minutes) {
    const int reset = normalized_reset_minutes(reset_minutes);
    const QTime time = date.time();
    const int minute_of_day = time.hour() * 60 + time.minute();
    if (minute_of_day < reset) {
        return date.date().addDays(-1);
    }
    return date.date();
}

std::string date_key(const QDateTime& date, int reset_minutes) {
    return reporting_day(date, reset_minutes).toString(QStringLiteral("yyyy-MM-dd")).toStdString();
}

ResetPreferenceLoad load_reset_preference(const StoredResetPreference& stored) {
    ResetPreferenceLoad load;
    if (!stored.stored_value.has_value()) {
        return load;
    }

    const int stored_value = *stored.stored_value;
    if (stored.migrated_to_minutes) {
        load.minutes = normalized_reset_minutes(stored_value);
        if (load.minutes != stored_value) {
            load.value_to_store = load.minutes;
        }
        return load;
    }

    // An unmigrated value is a reset hour; the migration always rewrites the
    // key with minutes and sets the flag.
    load.minutes = std::clamp(stored_value, 0, 23) * 60;
    load.value_to_store = load.minutes;
    load.mark_migrated = true;
    return load;
}

int reading_speed(std::int64_t characters_read, double reading_time) {
    if (!(reading_time > 0.0)) {
        return 0;
    }
    return static_cast<int>((static_cast<double>(characters_read) / reading_time) * 3600.0);
}

std::vector<Statistics> deduplicated_statistics(std::vector<Statistics> statistics) {
    std::vector<Statistics> records;
    records.reserve(statistics.size());
    std::map<std::string, std::size_t, std::less<>> index_by_key;
    for (Statistics& row : statistics) {
        const auto [position, inserted] = index_by_key.try_emplace(row.date_key, records.size());
        if (inserted) {
            records.push_back(std::move(row));
        } else {
            Statistics& kept = records[position->second];
            if (row.last_statistic_modified > kept.last_statistic_modified) {
                kept = std::move(row);
            }
        }
    }
    return records;
}

std::vector<Statistics> visible_statistics(std::vector<Statistics> statistics) {
    std::vector<Statistics> rows = deduplicated_statistics(std::move(statistics));
    std::erase_if(rows, [](const Statistics& row) {
        return !(row.characters_read > 0 || row.reading_time > 0);
    });
    sort_by_date_key(rows);
    return rows;
}

std::vector<Statistics> updating_statistics(std::string_view date_key, std::string_view title,
                                            std::int64_t characters_read, double reading_time,
                                            std::int64_t modified_at,
                                            std::vector<Statistics> statistics) {
    std::vector<Statistics> records = deduplicated_statistics(std::move(statistics));

    // The editor clamps before deriving the speed, so a negative character
    // count cannot produce a negative speed.
    const std::int64_t characters = std::max<std::int64_t>(characters_read, 0);
    const double time = std::max(reading_time, 0.0);
    const int speed = reading_speed(characters, time);

    Statistics record;
    record.date_key = std::string(date_key);
    record.characters_read = characters;
    record.reading_time = time;
    record.min_reading_speed = speed;
    record.alt_min_reading_speed = speed;
    record.last_reading_speed = speed;
    record.max_reading_speed = speed;
    record.last_statistic_modified = modified_at;

    const auto existing = std::find_if(records.begin(), records.end(),
                                       [date_key](const Statistics& row) {
                                           return row.date_key == date_key;
                                       });
    if (existing != records.end()) {
        record.title = existing->title;
        *existing = std::move(record);
    } else {
        record.title = std::string(title);
        records.push_back(std::move(record));
    }

    sort_by_date_key(records);
    return records;
}

std::vector<Statistics> deleting_statistics(std::string_view date_key, std::string_view title,
                                            std::int64_t modified_at,
                                            std::vector<Statistics> statistics) {
    return updating_statistics(date_key, title, 0, 0.0, modified_at, std::move(statistics));
}

std::vector<Statistics> deleting_all_statistics(std::string_view title,
                                                std::int64_t modified_at,
                                                std::vector<Statistics> statistics) {
    std::vector<Statistics> rows = deduplicated_statistics(std::move(statistics));
    for (Statistics& row : rows) {
        if (row.title.empty()) {
            row.title = std::string(title);
        }
        row.characters_read = 0;
        row.reading_time = 0.0;
        row.min_reading_speed = 0;
        row.alt_min_reading_speed = 0;
        row.last_reading_speed = 0;
        row.max_reading_speed = 0;
        row.last_statistic_modified = modified_at;
    }
    sort_by_date_key(rows);
    return rows;
}

int ReadingTotal::reading_speed() const {
    return niratan::statistics::reading_speed(characters_read, reading_time);
}

bool ReadingTotal::has_activity() const {
    return characters_read > 0 || reading_time > 0.0;
}

double ReadingTotal::time_to_read(int characters) const {
    const int speed = reading_speed();
    if (speed <= 0) {
        return 0.0;
    }
    return static_cast<double>(std::max(characters, 0)) / (static_cast<double>(speed) / 3600.0);
}

void ReadingTotal::add(const ReadingSession& session) {
    characters_read += session.characters_read;
    reading_time += session.reading_time;
}

void ReadingTotal::add(const ReadingTotal& total) {
    characters_read += total.characters_read;
    reading_time += total.reading_time;
}

bool ReadingSession::has_activity() const {
    return characters_read > 0 || reading_time > 0.0;
}

int ReadingSession::reading_speed() const {
    return niratan::statistics::reading_speed(characters_read, reading_time);
}

QDateTime ReadingSession::start_date(const QTimeZone& zone) const {
    return QDateTime::fromMSecsSinceEpoch(started_at, zone);
}

ReadingTotal ReadingSession::total() const {
    return ReadingTotal{characters_read, reading_time};
}

void ReadingSession::track(std::int64_t characters, double time, const QDateTime& until) {
    characters_read = std::max<std::int64_t>(characters_read + characters, 0);
    reading_time += time;
    ended_at = until.toMSecsSinceEpoch();
}

ReadingSession ReadingSession::starting_at(const QDateTime& date) {
    const std::int64_t timestamp = date.toMSecsSinceEpoch();
    return ReadingSession{timestamp, timestamp, 0, 0.0};
}

ReadingTotal ReadingSessionDay::total() const {
    ReadingTotal sum;
    for (const ReadingSessionEntry& entry : sessions) {
        sum.add(entry.session);
    }
    return sum;
}

std::int64_t ReadingSessionDay::last_modified() const {
    if (sessions.empty()) {
        return 0;
    }
    std::int64_t newest = sessions.front().modified;
    for (const ReadingSessionEntry& entry : sessions) {
        newest = std::max(newest, entry.modified);
    }
    return newest;
}

std::vector<ReadingSessionEntry> session_entries(const ReadingSessionRecords& records) {
    std::vector<ReadingSessionEntry> entries;
    entries.reserve(records.size());
    for (const auto& [id, record] : records) {
        if (record.value.has_value()) {
            entries.push_back(ReadingSessionEntry{id, *record.value, record.modified});
        }
    }
    std::ranges::sort(entries,
                      [](const ReadingSessionEntry& left, const ReadingSessionEntry& right) {
                          if (left.session.started_at != right.session.started_at) {
                              return left.session.started_at < right.session.started_at;
                          }
                          return left.id < right.id;
                      });
    return entries;
}

std::string session_date_key(const ReadingSession& session, int reset_minutes,
                             const QTimeZone& zone) {
    return date_key(session.start_date(zone), reset_minutes);
}

std::vector<ReadingSessionDay> session_days(const ReadingSessionRecords& records,
                                            int reset_minutes, const QTimeZone& zone) {
    // Entries are already ordered by start instant, so appending in entry order
    // keeps each day's sessions sorted; the map orders the days by date key.
    std::map<std::string, ReadingSessionDay, std::less<>> days;
    for (const ReadingSessionEntry& entry : session_entries(records)) {
        const std::string key = session_date_key(entry.session, reset_minutes, zone);
        days.try_emplace(key, ReadingSessionDay{key, {}}).first->second.sessions.push_back(entry);
    }

    std::vector<ReadingSessionDay> result;
    result.reserve(days.size());
    for (auto& entry : days) {
        result.push_back(std::move(entry.second));
    }
    return result;
}

std::vector<ReadingSessionDay> detail_days(const ReadingSessionRecords& records,
                                          int reset_minutes, const QTimeZone& zone) {
    auto days = session_days(records, reset_minutes, zone);
    std::erase_if(days, [](const auto& day) { return !day.total().has_activity(); });
    return days;
}

bool edit_detail_session(ReadingSessionRecords& records, std::string_view id, int characters,
                         int hours, int minutes, std::int64_t modified) {
    const auto entry = records.find(id);
    if (entry == records.end() || !entry->second.value) {
        return false;
    }
    auto session = *entry->second.value;
    const auto duration_minutes = static_cast<std::int64_t>(std::max(0, hours)) * 60 +
                                  std::clamp(minutes, 0, 59);
    if (static_cast<double>(duration_minutes) != std::round(session.reading_time / 60.0)) {
        session.reading_time = static_cast<double>(duration_minutes) * 60.0;
    }
    session.characters_read = std::max(0, characters);
    if (session == *entry->second.value) {
        return false;
    }
    entry->second = {modified, session};
    return true;
}

bool delete_detail_sessions(ReadingSessionRecords& records, const std::vector<std::string>& ids,
                            std::int64_t modified) {
    bool changed = false;
    for (const auto& id : ids) {
        const auto entry = records.find(id);
        if (entry != records.end() && entry->second.value) {
            entry->second = {modified, std::nullopt};
            changed = true;
        }
    }
    return changed;
}

std::vector<Statistics> daily_statistics(const ReadingSessionRecords& records,
                                         std::string_view title, int reset_minutes,
                                         const QTimeZone& zone) {
    std::vector<Statistics> rows;
    for (const ReadingSessionDay& day : session_days(records, reset_minutes, zone)) {
        const ReadingTotal total = day.total();
        if (!total.has_activity()) {
            continue;
        }
        const int speed = total.reading_speed();
        Statistics row;
        row.title = std::string(title);
        row.date_key = day.date_key;
        row.characters_read = total.characters_read;
        row.reading_time = total.reading_time;
        row.min_reading_speed = speed;
        row.alt_min_reading_speed = speed;
        row.last_reading_speed = speed;
        row.max_reading_speed = speed;
        row.last_statistic_modified = day.last_modified();
        rows.push_back(std::move(row));
    }
    return rows;
}

std::string legacy_session_id(std::string_view key, std::string_view date_key) {
    const QString normalized =
        QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()))
            .normalized(QString::NormalizationForm_C);
    QByteArray bytes = normalized.toUtf8();
    bytes.append('\n');
    bytes.append(QByteArray(date_key.data(), static_cast<qsizetype>(date_key.size())));
    bytes.append("\nlegacy");

    // UUID(uuid:) reads the digest in byte order and formats it as 8-4-4-4-12
    // uppercase hexadecimal; the version and variant bits stay as hashed.
    const QByteArray digest =
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).left(16);
    QString text = QString::fromLatin1(digest.toHex().toUpper());
    text.insert(8, QLatin1Char('-'));
    text.insert(13, QLatin1Char('-'));
    text.insert(18, QLatin1Char('-'));
    text.insert(23, QLatin1Char('-'));
    return text.toStdString();
}

std::optional<ReadingSession> legacy_session(const Statistics& statistic, int reset_minutes,
                                             const QTimeZone& zone) {
    const std::optional<QDate> day = parse_date_key(statistic.date_key);
    if (!day.has_value()) {
        return std::nullopt;
    }

    const int reset = normalized_reset_minutes(reset_minutes);
    const QDateTime boundary(*day, QTime(reset / 60, reset % 60), zone);
    if (!boundary.isValid()) {
        return std::nullopt;
    }

    // TTU only stores the last modification, so the end is that instant and the
    // start is the end minus the reading time. The estimate is used only when
    // both ends fall on the row's reporting day; otherwise the reset boundary
    // positions the session. Fractional seconds round to the nearest
    // millisecond, matching StatisticsClock.milliseconds.
    const double reading_time = std::max(statistic.reading_time, 0.0);
    const std::int64_t end_milliseconds = statistic.last_statistic_modified;
    const std::int64_t estimated_start = static_cast<std::int64_t>(
        std::llround(static_cast<double>(end_milliseconds) - reading_time * 1000.0));

    QDateTime start = boundary;
    if (date_key(QDateTime::fromMSecsSinceEpoch(estimated_start, zone), reset) ==
            statistic.date_key &&
        date_key(QDateTime::fromMSecsSinceEpoch(end_milliseconds, zone), reset) ==
            statistic.date_key) {
        start = QDateTime::fromMSecsSinceEpoch(estimated_start, zone);
    }

    ReadingSession session;
    session.started_at = start.toMSecsSinceEpoch();
    session.ended_at = static_cast<std::int64_t>(
        std::llround(static_cast<double>(session.started_at) + reading_time * 1000.0));
    session.characters_read = std::max<std::int64_t>(statistic.characters_read, 0);
    session.reading_time = reading_time;
    return session;
}

ReadingSessionRecords legacy_sessions(const std::vector<Statistics>& statistics,
                                      std::string_view key, int reset_minutes,
                                      const QTimeZone& zone) {
    ReadingSessionRecords records;
    for (const Statistics& statistic : visible_statistics(statistics)) {
        const std::optional<ReadingSession> session =
            legacy_session(statistic, reset_minutes, zone);
        if (!session.has_value()) {
            continue;
        }
        records[legacy_session_id(key, statistic.date_key)] =
            TimestampedSession{statistic.last_statistic_modified, session};
    }
    return records;
}

StatisticsTargetSettings StatisticsTargetSettings::clamped() const {
    StatisticsTargetSettings result = *this;
    result.daily_character_target = snap_character_target(daily_character_target);
    result.daily_duration_target_minutes = snap_duration_target(daily_duration_target_minutes);
    result.weekly_target_days =
        std::clamp(weekly_target_days, weekly_target_days_min, weekly_target_days_max);
    return result;
}

int StatisticsTargetSettings::snap_character_target(int value) {
    return snap_to_step(value, character_target_min, character_target_max, character_target_step);
}

int StatisticsTargetSettings::snap_duration_target(int value) {
    return snap_to_step(value, duration_target_minutes_min, duration_target_minutes_max,
                        duration_target_minutes_step);
}

int target_percent(const StatisticsTargetSettings& settings, int characters,
                   double reading_time) {
    double ratio = 0.0;
    switch (settings.daily_target_type) {
    case DailyTargetType::characters:
        if (settings.daily_character_target > 0) {
            ratio = static_cast<double>(characters) /
                    static_cast<double>(settings.daily_character_target);
        }
        break;
    case DailyTargetType::duration: {
        const double target_seconds =
            static_cast<double>(settings.daily_duration_target_minutes) * 60.0;
        if (target_seconds > 0.0) {
            ratio = reading_time / target_seconds;
        }
        break;
    }
    }
    // StatisticsDashboard's percent rounds half away from zero.
    return static_cast<int>(std::round(ratio * 100.0));
}

bool should_persist_statistics(
    int model_chapter_index, int model_character,
    const std::optional<ReaderStatisticsBookmarkSnapshot>& persisted_bookmark) {
    if (!persisted_bookmark.has_value()) {
        return true;
    }
    return persisted_bookmark->chapter_index == model_chapter_index &&
           persisted_bookmark->character_count == model_character;
}

}  // namespace niratan::statistics
