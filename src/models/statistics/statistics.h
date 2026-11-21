// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reading statistics models, following Models/Statistics.swift
// (StatisticsAutostartMode, StatisticsSyncMode, StatisticsResetTimePreference,
// StatisticsDayBoundary, Statistics, StatisticsEditor, StatisticsClock,
// ReadingSession, ReadingTotal, ReadingSessionEntry, ReadingSessionDay,
// ReadingSessionLog, Timestamped), the target settings in
// Features/Bookshelf/StatisticsDashboardModels.swift (DailyTargetType,
// StatisticsTargetSettings, StatisticsDayAggregate.targetRatio and the
// dashboard's rounded percent) and
// Features/Reader/ReaderStatisticsPersistencePolicy.swift.
//
// Representation
// --------------
// Session timestamps are std::int64_t milliseconds since the Unix epoch,
// which is StatisticsClock.milliseconds: an instant is written as
// date.toMSecsSinceEpoch() and read back with
// QDateTime::fromMSecsSinceEpoch(ms, zone) in whatever zone the caller works
// in. A day boundary is a local calendar day, so it is a QDate; nothing here
// reads the system clock, and every date is supplied by the caller. A date
// key is the "yyyy-MM-dd" text of the reporting day, produced by date_key().
//
// The reporting day is the local calendar day of the instant, shifted back
// one day when the instant's local minute-of-day is below the reset minute.
// The reset minute is 0..1439 and is normalized before use. The instant's
// own time zone is the local zone for all calendar arithmetic, so the caller
// decides which zone "local" means by the QDateTime or QTimeZone it passes.
//
// Known deviations from the Swift source
// --------------------------------------
// * UIKit-free arithmetic on impossible inputs: an impossible date key such
//   as "2026-02-30" converts to no session here, while Foundation's
//   Calendar rolls an overflowing DateComponents into the next month. Such a
//   key cannot be produced by date_key(), so only corrupt input reaches it.
// * Integer overflow in a speed, a session total or a tracked character count
//   traps in Swift and is undefined here; the values that reach a card or a
//   stored file are far below those bounds.
// * Session stamps are whole milliseconds. Only legacy_session derives an
//   instant from a fractional reading time, rounded half away from zero, so
//   the derived end can differ from last_statistic_modified by up to 1 ms.
// * The legacy session id normalizes the key to NFC with Qt's Unicode tables,
//   while Swift uses precomposedStringWithCanonicalMapping; for a character
//   whose normalization changed between Unicode versions the two ids can
//   differ.
// * Strings are UTF-8 std::string. Swift's String ordering is used only for
//   date keys, which are ASCII digits and hyphens, so byte order and Swift
//   order agree there.
// * Statistics day/week targets follow the dashboard model only: the percent
//   is the rounded day ratio, not a rendered dashboard row.

#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTimeZone>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::statistics {

// StatisticsAutostartMode: the persisted raw values are "Off", "Page Turn" and
// "On", in that declaration order.
enum class StatisticsAutostartMode { off, page_turn, on };

std::string_view autostart_mode_id(StatisticsAutostartMode mode);

// StatisticsSyncMode: the persisted raw values are "Merge" and "Replace".
enum class StatisticsSyncMode { merge, replace };

std::string_view sync_mode_id(StatisticsSyncMode mode);

// DailyTargetType is Codable by raw value: "characters" and "duration".
enum class DailyTargetType { characters, duration };

std::string_view daily_target_type_id(DailyTargetType type);

// StatisticsDayBoundary.minutesPerDay.
inline constexpr int minutes_per_day = 24 * 60;

// StatisticsDayBoundary.normalizedResetMinutes: clamps to 0..1439.
int normalized_reset_minutes(int reset_minutes);

// StatisticsDayBoundary.reportingDay: returns the reporting day of `date` as
// a start-of-day QDate. The minute-of-day and the previous local calendar day
// are taken in date.timeZone(); subtracting one calendar day keeps the local
// wall-clock time, as Foundation does.
QDate reporting_day(const QDateTime& date, int reset_minutes);

// StatisticsDayBoundary.dateKey: "yyyy-MM-dd" of reporting_day().
std::string date_key(const QDateTime& date, int reset_minutes);

// The raw stored form StatisticsResetTimePreference reads and writes: the
// stored integer (absent when the key was never written or does not hold an
// integer) and whether the hour-to-minute migration already ran.
struct StoredResetPreference {
    std::optional<int> stored_value;
    bool migrated_to_minutes = false;

    bool operator==(const StoredResetPreference&) const = default;
};

// The result of StatisticsResetTimePreference.load, made pure so that the
// caller owns the defaults store: the reset minute to use, the value to write
// back when normalization changed it, and whether the migration flag has to
// be set. An absent stored value yields 0 and writes nothing.
struct ResetPreferenceLoad {
    int minutes = 0;
    std::optional<int> value_to_store;
    bool mark_migrated = false;

    bool operator==(const ResetPreferenceLoad&) const = default;
};

ResetPreferenceLoad load_reset_preference(const StoredResetPreference& stored);

// One TTU statistics.json daily row, following Statistics in
// Models/Statistics.swift. The field order matches the synthesized Codable
// keys; characters_read and last_statistic_modified are Swift's Int, and the
// speeds are stored as int.
struct Statistics {
    std::string title;
    std::string date_key;
    qint64 characters_read = 0;
    double reading_time = 0.0;
    int min_reading_speed = 0;
    int alt_min_reading_speed = 0;
    int last_reading_speed = 0;
    int max_reading_speed = 0;
    std::int64_t last_statistic_modified = 0;

    bool operator==(const Statistics&) const = default;
};

// ReadingTotal.speed: truncating int(characters / time * 3600), and 0 when
// the time is not positive.
int reading_speed(std::int64_t characters_read, double reading_time);

// StatisticsEditor.deduplicated: one row per date key, keeping the row with
// the greater last_statistic_modified; on a tie the first row in input order
// stays. The result order is unspecified, as Swift's dictionary order is.
std::vector<Statistics> deduplicated_statistics(std::vector<Statistics> statistics);

// StatisticsEditor.visibleStatistics: deduplicated rows that read something
// (characters or time positive), sorted by ascending date key.
std::vector<Statistics> visible_statistics(std::vector<Statistics> statistics);

// StatisticsEditor.updating: clamps characters and time at 0, writes the
// truncating speed into all four speed fields, keeps the title of an existing
// row with this date key (the argument title is only used for a new row),
// replaces or appends, and sorts by ascending date key.
std::vector<Statistics> updating_statistics(std::string_view date_key, std::string_view title,
                                            std::int64_t characters_read, double reading_time,
                                            std::int64_t modified_at,
                                            std::vector<Statistics> statistics);

// StatisticsEditor.deleting: updating_statistics with zero characters and
// time, which leaves a zero-valued tombstone for a new date key too.
std::vector<Statistics> deleting_statistics(std::string_view date_key, std::string_view title,
                                            std::int64_t modified_at,
                                            std::vector<Statistics> statistics);

// StatisticsEditor.deletingAll: writes a zero-valued tombstone for every
// deduplicated date key, keeping each row's own title unless it is empty, and
// sorts by ascending date key.
std::vector<Statistics> deleting_all_statistics(std::string_view title,
                                                std::int64_t modified_at,
                                                std::vector<Statistics> statistics);

struct ReadingSession;

// ReadingTotal: a character and time sum with a derived speed.
struct ReadingTotal {
    qint64 characters_read = 0;
    double reading_time = 0.0;

    int reading_speed() const;
    bool has_activity() const;
    // Characters divided by the speed per hour; 0 when the speed is 0 and
    // the character count is clamped at 0.
    double time_to_read(int characters) const;
    void add(const ReadingSession& session);
    void add(const ReadingTotal& total);

    bool operator==(const ReadingTotal&) const = default;
};

// ReadingSession: one reading stint. track() adds the characters with a floor
// at zero, adds the time unvalidated (a negative time lowers the sum) and
// stamps ended_at from `until`; the start is unchanged.
struct ReadingSession {
    std::int64_t started_at = 0;
    std::int64_t ended_at = 0;
    qint64 characters_read = 0;
    double reading_time = 0.0;

    bool has_activity() const;
    int reading_speed() const;
    // The start instant as a QDateTime in `zone`, the equivalent of
    // StatisticsClock.date(startedAt) with the caller's calendar.
    QDateTime start_date(const QTimeZone& zone) const;
    ReadingTotal total() const;

    void track(std::int64_t characters, double time, const QDateTime& until);
    static ReadingSession starting_at(const QDateTime& date);

    bool operator==(const ReadingSession&) const = default;
};

// Timestamped<ReadingSession?>: a modification time and an optional value. A
// missing value is a tombstone that keeps the modification time.
struct TimestampedSession {
    std::int64_t modified = 0;
    std::optional<ReadingSession> value;

    bool operator==(const TimestampedSession&) const = default;
};

// ReadingSessionRecords. Swift's dictionary is unordered; this map orders by
// id, which is only the tie-break entries() already applies.
using ReadingSessionRecords = std::map<std::string, TimestampedSession, std::less<>>;

// ReadingSessionEntry: one live session with its record id and modification
// time.
struct ReadingSessionEntry {
    std::string id;
    ReadingSession session;
    std::int64_t modified = 0;

    bool operator==(const ReadingSessionEntry&) const = default;
};

// ReadingSessionDay: the live sessions whose start falls on one reporting
// day.
struct ReadingSessionDay {
    std::string date_key;
    std::vector<ReadingSessionEntry> sessions;

    ReadingTotal total() const;
    // The newest session modification, 0 for a day without sessions.
    std::int64_t last_modified() const;

    bool operator==(const ReadingSessionDay&) const = default;
};

// ReadingSessionLog.entries: drops tombstones and orders by started_at, then
// by id when two sessions start at the same millisecond.
std::vector<ReadingSessionEntry> session_entries(const ReadingSessionRecords& records);

// ReadingSessionLog.dateKey(for:resetMinutes:calendar:) for one session,
// using the start instant in `zone`.
std::string session_date_key(const ReadingSession& session, int reset_minutes,
                             const QTimeZone& zone);

// ReadingSessionLog.days: groups the live sessions by the reporting day of
// their start instant, keeps each day's entries in entries() order, and sorts
// the days by ascending date key.
std::vector<ReadingSessionDay> session_days(const ReadingSessionRecords& records,
                                            int reset_minutes, const QTimeZone& zone);

// StatisticsBookDetailPanel and ReadingSessionLog.editing/deleting. Empty
// days are hidden. Editing only the count preserves fractional seconds;
// deleting retains tombstones and never restamps an existing tombstone.
std::vector<ReadingSessionDay> detail_days(const ReadingSessionRecords& records,
                                          int reset_minutes, const QTimeZone& zone);
bool edit_detail_session(ReadingSessionRecords& records, std::string_view id, int characters,
                         int hours, int minutes, std::int64_t modified);
bool delete_detail_sessions(ReadingSessionRecords& records, const std::vector<std::string>& ids,
                            std::int64_t modified);

// ReadingSessionLog.dailyStatistics: one row per day with activity, carrying
// the day totals, the day's speed in all four speed fields and the newest
// session modification. Days without activity produce no row. Rows are sorted
// by ascending date key.
std::vector<Statistics> daily_statistics(const ReadingSessionRecords& records,
                                         std::string_view title, int reset_minutes,
                                         const QTimeZone& zone);

// ReadingSessionLog.legacyID. The id is stable for the same key and date key:
//   bytes = UTF-8 of NFC(key) + "\n" + dateKey + "\n" + "legacy"
//   digest = SHA-256(bytes), first 16 bytes in digest order
//   text = those bytes as 8-4-4-4-12 uppercase hexadecimal (UUID.uuidString)
// The version and variant bits are not adjusted, so the result is not an
// RFC 4122 name-based UUID. `key` is the UTF-8 book folder name; the date key
// is used as given.
std::string legacy_session_id(std::string_view key, std::string_view date_key);

// ReadingSessionLog's private session(for:resetMinutes:calendar:). The date
// key is split on '-' and empty components are skipped; a component that does
// not parse as an Int is dropped rather than rejecting the key, so
// "2026-xx-08-02" still reads as 2026-08-02. The key is read when exactly
// three values remain, so an extra numeric component leaves four and yields
// nullopt, as do fewer. Those three values must name a valid local date here;
// an impossible date such as "2026-02-30" yields nullopt, where Foundation
// rolls the components over. The start is the local reset hour and minute of
// that day; when the row's modification time minus its reading time starts and
// ends on the same reporting day, that estimated start replaces the reset
// boundary. Characters and reading time are clamped at 0, and ended_at is the
// start plus the reading time.
std::optional<ReadingSession> legacy_session(const Statistics& statistic, int reset_minutes,
                                             const QTimeZone& zone);

// ReadingSessionLog.legacySessions: converts the visible daily rows into one
// session per date key, keyed by legacy_session_id(key, dateKey) and modified
// with the row's last_statistic_modified. Rows that cannot be converted are
// skipped.
ReadingSessionRecords legacy_sessions(const std::vector<Statistics>& statistics,
                                      std::string_view key, int reset_minutes,
                                      const QTimeZone& zone);

// StatisticsTargetSettings: the daily target kind plus the character,
// duration and weekly-days targets with their allowed ranges and steps.
struct StatisticsTargetSettings {
    DailyTargetType daily_target_type = DailyTargetType::characters;
    int daily_character_target = 5000;
    int daily_duration_target_minutes = 30;
    int weekly_target_days = 4;

    static constexpr int character_target_min = 500;
    static constexpr int character_target_max = 50000;
    static constexpr int character_target_step = 500;

    static constexpr int duration_target_minutes_min = 5;
    static constexpr int duration_target_minutes_max = 240;
    static constexpr int duration_target_minutes_step = 5;

    static constexpr int weekly_target_days_min = 1;
    static constexpr int weekly_target_days_max = 7;

    // Snaps every field into its range and step, leaving the target kind.
    StatisticsTargetSettings clamped() const;

    // Range clamp, then round to the nearest step (half a step away from zero
    // through integer arithmetic), then clamp again.
    static int snap_character_target(int value);
    static int snap_duration_target(int value);

    bool operator==(const StatisticsTargetSettings&) const = default;
};

// StatisticsDayAggregate.targetRatio followed by the dashboard's percent:
// characters divided by the character target, or reading time divided by the
// duration target in seconds; 0 when the chosen target is not positive, then
// rounded to a whole percent half away from zero. The ratio is not clamped.
int target_percent(const StatisticsTargetSettings& settings, int characters,
                   double reading_time);

// ReaderStatisticsBookmarkSnapshot: the persisted bookmark a reader model is
// compared against.
struct ReaderStatisticsBookmarkSnapshot {
    int chapter_index = 0;
    int character_count = 0;

    bool operator==(const ReaderStatisticsBookmarkSnapshot&) const = default;
};

// ReaderStatisticsPersistencePolicy.shouldPersist: a book without a persisted
// bookmark always writes; otherwise the model's chapter index and character
// count must equal the bookmark's, so a stale model cannot overwrite newer
// statistics.
bool should_persist_statistics(int model_chapter_index, int model_character,
                               const std::optional<ReaderStatisticsBookmarkSnapshot>& persisted_bookmark);

}  // namespace niratan::statistics
