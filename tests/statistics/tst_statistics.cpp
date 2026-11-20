// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/statistics/statistics.h"

#include <QCryptographicHash>
#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QTest>
#include <QTime>
#include <QTimeZone>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using niratan::statistics::DailyTargetType;
using niratan::statistics::ReadingSession;
using niratan::statistics::ReadingSessionDay;
using niratan::statistics::ReadingSessionEntry;
using niratan::statistics::ReadingSessionRecords;
using niratan::statistics::ReadingTotal;
using niratan::statistics::ReaderStatisticsBookmarkSnapshot;
using niratan::statistics::ResetPreferenceLoad;
using niratan::statistics::Statistics;
using niratan::statistics::StatisticsAutostartMode;
using niratan::statistics::StatisticsSyncMode;
using niratan::statistics::StatisticsTargetSettings;
using niratan::statistics::StoredResetPreference;
using niratan::statistics::TimestampedSession;
using niratan::statistics::autostart_mode_id;
using niratan::statistics::daily_statistics;
using niratan::statistics::daily_target_type_id;
using niratan::statistics::date_key;
using niratan::statistics::deduplicated_statistics;
using niratan::statistics::deleting_all_statistics;
using niratan::statistics::deleting_statistics;
using niratan::statistics::legacy_session;
using niratan::statistics::legacy_session_id;
using niratan::statistics::legacy_sessions;
using niratan::statistics::load_reset_preference;
using niratan::statistics::minutes_per_day;
using niratan::statistics::normalized_reset_minutes;
using niratan::statistics::reading_speed;
using niratan::statistics::reporting_day;
using niratan::statistics::session_date_key;
using niratan::statistics::session_days;
using niratan::statistics::session_entries;
using niratan::statistics::should_persist_statistics;
using niratan::statistics::sync_mode_id;
using niratan::statistics::target_percent;
using niratan::statistics::updating_statistics;
using niratan::statistics::visible_statistics;

// Models/Statistics.swift (StatisticsDayBoundary, StatisticsResetTimePreference,
// Statistics, StatisticsEditor, ReadingSession, ReadingTotal, ReadingSessionLog,
// StatisticsClock, Timestamped), the target settings in
// Features/Bookshelf/StatisticsDashboardModels.swift, and
// Features/Reader/ReaderStatisticsPersistencePolicy.swift. The dates are fixed
// instants in explicit zones so nothing depends on the machine's clock or its
// local zone.
namespace {

QTimeZone fixed_zone() {
    return QTimeZone::fromSecondsAheadOfUtc(8 * 60 * 60);
}

QTimeZone pacific_zone() {
    return QTimeZone(QByteArray("America/Los_Angeles"));
}

QDateTime at(const QTimeZone& zone, int year, int month, int day, int hour, int minute = 0) {
    return QDateTime(QDate(year, month, day), QTime(hour, minute), zone);
}

std::int64_t ms(const QDateTime& date) {
    return date.toMSecsSinceEpoch();
}

ReadingSession make_session(const QDateTime& start, int characters, double seconds) {
    ReadingSession session;
    session.started_at = ms(start);
    session.ended_at = ms(start);
    session.characters_read = characters;
    session.reading_time = seconds;
    return session;
}

TimestampedSession timestamped(std::int64_t modified, std::optional<ReadingSession> value) {
    TimestampedSession entry;
    entry.modified = modified;
    entry.value = std::move(value);
    return entry;
}

Statistics day_row(std::string key, int characters, double seconds, std::int64_t modified,
                   std::string title = std::string("Book")) {
    Statistics row;
    row.title = std::move(title);
    row.date_key = std::move(key);
    row.characters_read = characters;
    row.reading_time = seconds;
    row.last_statistic_modified = modified;
    return row;
}

const Statistics* find_row(const std::vector<Statistics>& rows, std::string_view key) {
    for (const Statistics& row : rows) {
        if (row.date_key == key) {
            return &row;
        }
    }
    return nullptr;
}

std::vector<std::string> row_keys(const std::vector<Statistics>& rows) {
    std::vector<std::string> keys;
    keys.reserve(rows.size());
    for (const Statistics& row : rows) {
        keys.push_back(row.date_key);
    }
    return keys;
}

std::vector<std::string> entry_ids(const std::vector<ReadingSessionEntry>& entries) {
    std::vector<std::string> ids;
    ids.reserve(entries.size());
    for (const ReadingSessionEntry& entry : entries) {
        ids.push_back(entry.id);
    }
    return ids;
}

// Restates the documented legacy id layout independently of the implementation:
// SHA-256 over NFC(key), a newline, the date key, a newline and "legacy", the
// first 16 digest bytes formatted as an uppercase UUID.
std::string expected_legacy_id(const QString& key, const QString& date_key_text) {
    QByteArray bytes = key.normalized(QString::NormalizationForm_C).toUtf8();
    bytes.append('\n');
    bytes.append(date_key_text.toUtf8());
    bytes.append("\nlegacy");
    const QByteArray digest =
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).left(16);
    QString text = QString::fromLatin1(digest.toHex().toUpper());
    text.insert(8, QLatin1Char('-'));
    text.insert(13, QLatin1Char('-'));
    text.insert(18, QLatin1Char('-'));
    text.insert(23, QLatin1Char('-'));
    return text.toStdString();
}

}  // namespace

class StatisticsTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();

    void stored_ids_match_the_swift_raw_values();
    void reset_minutes_are_clamped();
    void reporting_day_shifts_before_the_reset_minute();
    void date_keys_follow_local_calendar_days();
    void reset_preference_migrates_legacy_hours_once();

    void reading_speed_truncates_toward_zero();
    void reading_total_speed_and_time_to_read();
    void session_track_accumulates_and_stamps_the_end();

    void entries_drop_tombstones_and_sort_by_start_then_id();
    void days_group_sessions_by_start_day();

    void legacy_ids_are_stable_and_key_specific();
    void legacy_session_uses_the_last_modified_heuristic();
    void legacy_sessions_skip_empty_rows_and_keep_ids();
    void daily_statistics_write_one_row_per_active_day();

    void deduplicate_keeps_the_newest_modification();
    void visible_rows_hide_tombstones_and_sort();
    void updating_keeps_the_existing_title_and_recalculates_speed();
    void deleting_writes_a_zero_row();
    void deleting_all_leaves_one_zero_row_per_day();

    void target_settings_snap_to_range_and_step();
    void target_settings_clamped_keeps_the_target_kind();
    void target_percent_rounds_the_day_ratio();
    void persistence_policy_compares_the_persisted_bookmark();
};

void StatisticsTest::initTestCase() {
    QVERIFY2(pacific_zone().isValid(), "the test needs the IANA time zone database");
}

void StatisticsTest::stored_ids_match_the_swift_raw_values() {
    QCOMPARE(std::string(autostart_mode_id(StatisticsAutostartMode::off)), std::string("Off"));
    QCOMPARE(std::string(autostart_mode_id(StatisticsAutostartMode::page_turn)),
             std::string("Page Turn"));
    QCOMPARE(std::string(autostart_mode_id(StatisticsAutostartMode::on)), std::string("On"));

    QCOMPARE(std::string(sync_mode_id(StatisticsSyncMode::merge)), std::string("Merge"));
    QCOMPARE(std::string(sync_mode_id(StatisticsSyncMode::replace)), std::string("Replace"));

    QCOMPARE(std::string(daily_target_type_id(DailyTargetType::characters)),
             std::string("characters"));
    QCOMPARE(std::string(daily_target_type_id(DailyTargetType::duration)),
             std::string("duration"));
}

void StatisticsTest::reset_minutes_are_clamped() {
    QCOMPARE(minutes_per_day, 1440);
    QCOMPARE(normalized_reset_minutes(-30), 0);
    QCOMPARE(normalized_reset_minutes(0), 0);
    QCOMPARE(normalized_reset_minutes(275), 275);
    QCOMPARE(normalized_reset_minutes(1439), 1439);
    QCOMPARE(normalized_reset_minutes(2000), 1439);
}

void StatisticsTest::reporting_day_shifts_before_the_reset_minute() {
    const QTimeZone zone = fixed_zone();

    const QDate shifted = reporting_day(at(zone, 2026, 8, 10, 3, 59), 4 * 60);
    QCOMPARE(shifted, QDate(2026, 8, 9));
    QCOMPARE(reporting_day(at(zone, 2026, 8, 10, 4, 0), 4 * 60), QDate(2026, 8, 10));
    QCOMPARE(reporting_day(at(zone, 2026, 8, 10, 3, 59), 0), QDate(2026, 8, 10));

    // A minute before the final minute of the day still belongs to the day before.
    QCOMPARE(reporting_day(at(zone, 2026, 8, 10, 23, 58), 1439), QDate(2026, 8, 9));
    QCOMPARE(reporting_day(at(zone, 2026, 8, 10, 23, 59), 1439), QDate(2026, 8, 10));

    // The reset minute is normalized before it is compared.
    QCOMPARE(reporting_day(at(zone, 2026, 8, 10, 3, 59), -30), QDate(2026, 8, 10));
    QCOMPARE(reporting_day(at(zone, 2026, 8, 10, 0, 0), 0), QDate(2026, 8, 10));
}

void StatisticsTest::date_keys_follow_local_calendar_days() {
    const QTimeZone zone = fixed_zone();

    const std::string padded = date_key(at(zone, 2026, 3, 7, 3, 0), 4 * 60);
    QCOMPARE(padded, std::string("2026-03-06"));
    QCOMPARE(date_key(at(zone, 2026, 8, 10, 12, 0), 4 * 60), std::string("2026-08-10"));
    QCOMPARE(date_key(at(zone, 2026, 12, 31, 23, 59), 0), std::string("2026-12-31"));

    const QTimeZone pacific = pacific_zone();
    QCOMPARE(date_key(at(pacific, 2026, 3, 8, 3, 30), 4 * 60), std::string("2026-03-07"));
    QCOMPARE(date_key(at(pacific, 2026, 3, 8, 3, 30), 0), std::string("2026-03-08"));
    QCOMPARE(date_key(at(pacific, 2026, 11, 1, 0, 30), 4 * 60), std::string("2026-10-31"));
}

void StatisticsTest::reset_preference_migrates_legacy_hours_once() {
    const ResetPreferenceLoad absent = load_reset_preference(StoredResetPreference{});
    QCOMPARE(absent.minutes, 0);
    QVERIFY(!absent.value_to_store.has_value());
    QVERIFY(!absent.mark_migrated);

    const std::optional<int> two_hundred_forty = 240;

    StoredResetPreference legacy;
    legacy.stored_value = 4;
    const ResetPreferenceLoad migrated = load_reset_preference(legacy);
    QCOMPARE(migrated.minutes, 240);
    QCOMPARE(migrated.value_to_store, two_hundred_forty);
    QVERIFY(migrated.mark_migrated);

    StoredResetPreference legacy_out_of_range;
    legacy_out_of_range.stored_value = 99;
    const ResetPreferenceLoad clamped_hours = load_reset_preference(legacy_out_of_range);
    QCOMPARE(clamped_hours.minutes, 23 * 60);

    StoredResetPreference minutes;
    minutes.stored_value = 275;
    minutes.migrated_to_minutes = true;
    const ResetPreferenceLoad unchanged = load_reset_preference(minutes);
    QCOMPARE(unchanged.minutes, 275);
    QVERIFY(!unchanged.value_to_store.has_value());
    QVERIFY(!unchanged.mark_migrated);

    const std::optional<int> last_minute = 1439;
    const std::optional<int> midnight = 0;

    StoredResetPreference minutes_out_of_range;
    minutes_out_of_range.stored_value = 2000;
    minutes_out_of_range.migrated_to_minutes = true;
    const ResetPreferenceLoad normalized = load_reset_preference(minutes_out_of_range);
    QCOMPARE(normalized.minutes, 1439);
    QCOMPARE(normalized.value_to_store, last_minute);
    QVERIFY(!normalized.mark_migrated);

    StoredResetPreference negative;
    negative.stored_value = -5;
    negative.migrated_to_minutes = true;
    const ResetPreferenceLoad floored = load_reset_preference(negative);
    QCOMPARE(floored.minutes, 0);
    QCOMPARE(floored.value_to_store, midnight);
}

void StatisticsTest::reading_speed_truncates_toward_zero() {
    QCOMPARE(reading_speed(0, 0.0), 0);
    QCOMPARE(reading_speed(600, 600.0), 3600);
    QCOMPARE(reading_speed(1, 7.0), 514);
    QCOMPARE(reading_speed(999, 1000.0), 3596);
    QCOMPARE(reading_speed(100, -5.0), 0);
    // Characters are not clamped here, only readers that add them are.
    QCOMPARE(reading_speed(-100, 600.0), -600);
}

void StatisticsTest::reading_total_speed_and_time_to_read() {
    ReadingTotal total;
    total.characters_read = 3600;
    total.reading_time = 3600.0;
    QCOMPARE(total.reading_speed(), 3600);
    QCOMPARE(total.time_to_read(1800), 1800.0);
    QCOMPARE(total.time_to_read(-100), 0.0);
    QVERIFY(total.has_activity());

    ReadingTotal empty;
    QCOMPARE(empty.reading_speed(), 0);
    QCOMPARE(empty.time_to_read(100), 0.0);
    QVERIFY(!empty.has_activity());

    ReadingTotal slow;
    slow.characters_read = 900;
    slow.reading_time = 3600.0;
    QCOMPARE(slow.reading_speed(), 900);
    QCOMPARE(slow.time_to_read(1800), 7200.0);

    const ReadingSession session =
        make_session(at(fixed_zone(), 2026, 8, 4, 9, 0), 1000, 600.0);
    ReadingTotal sum;
    sum.add(session);
    QCOMPARE(sum.characters_read, 1000);
    QCOMPARE(sum.reading_time, 600.0);

    ReadingTotal other;
    other.characters_read = 500;
    other.reading_time = 300.0;
    sum.add(other);
    QCOMPARE(sum.characters_read, 1500);
    QCOMPARE(sum.reading_time, 900.0);

    const ReadingTotal from_session = session.total();
    QCOMPARE(from_session.characters_read, session.characters_read);
    QCOMPARE(from_session.reading_time, session.reading_time);
}

void StatisticsTest::session_track_accumulates_and_stamps_the_end() {
    const QTimeZone zone = fixed_zone();
    const QDateTime start = at(zone, 2026, 8, 4, 21, 0);

    const ReadingSession fresh = ReadingSession::starting_at(start);
    QCOMPARE(fresh.started_at, ms(start));
    QCOMPARE(fresh.ended_at, ms(start));
    QCOMPARE(fresh.characters_read, 0);
    QCOMPARE(fresh.reading_time, 0.0);
    QVERIFY(!fresh.has_activity());
    QCOMPARE(fresh.start_date(zone), start);

    ReadingSession session = fresh;
    const QDateTime first_end = at(zone, 2026, 8, 4, 21, 1);
    session.track(100, 60.0, first_end);
    QCOMPARE(session.started_at, ms(start));
    QCOMPARE(session.ended_at, ms(first_end));
    QCOMPARE(session.characters_read, 100);
    QCOMPARE(session.reading_time, 60.0);
    QVERIFY(session.has_activity());
    QCOMPARE(session.reading_speed(), 6000);

    const QDateTime second_end = at(zone, 2026, 8, 4, 21, 2);
    session.track(-250, 5.0, second_end);
    QCOMPARE(session.characters_read, 0);
    QCOMPARE(session.reading_time, 65.0);
    QCOMPARE(session.ended_at, ms(second_end));

    // Time is added unvalidated: a negative delta lowers the sum and is not
    // activity of its own.
    ReadingSession negative_time = ReadingSession::starting_at(start);
    negative_time.track(0, -10.0, first_end);
    QCOMPARE(negative_time.reading_time, -10.0);
    QVERIFY(!negative_time.has_activity());
}

void StatisticsTest::entries_drop_tombstones_and_sort_by_start_then_id() {
    const QTimeZone zone = fixed_zone();
    ReadingSessionRecords records;
    records["b"] = timestamped(3, make_session(at(zone, 2026, 8, 4, 12, 0), 10, 60.0));
    records["a"] = timestamped(2, make_session(at(zone, 2026, 8, 4, 12, 0), 20, 120.0));
    records["c"] = timestamped(1, make_session(at(zone, 2026, 8, 4, 9, 0), 30, 180.0));
    records["d"] = timestamped(4, std::nullopt);

    const std::vector<ReadingSessionEntry> entries = session_entries(records);
    QCOMPARE(entries.size(), 3u);

    const std::vector<std::string> ids = entry_ids(entries);
    const std::vector<std::string> expected = {"c", "a", "b"};
    QCOMPARE(ids, expected);

    QCOMPARE(entries[0].modified, records.at("c").modified);
    QCOMPARE(entries[1].modified, records.at("a").modified);
    QCOMPARE(entries[2].modified, records.at("b").modified);
    QCOMPARE(entries[2].session.characters_read, 10);

    const ReadingSessionRecords empty;
    QVERIFY(session_entries(empty).empty());
}

void StatisticsTest::days_group_sessions_by_start_day() {
    const QTimeZone zone = fixed_zone();
    ReadingSessionRecords records;
    records["early"] = timestamped(1, make_session(at(zone, 2026, 8, 3, 12, 0), 100, 60.0));
    records["midnight"] = timestamped(2, make_session(at(zone, 2026, 8, 4, 0, 30), 100, 60.0));
    records["late"] = timestamped(3, make_session(at(zone, 2026, 8, 4, 23, 50), 200, 1800.0));
    // Ends on the next day; a session belongs to the day it started.
    QVERIFY(records.at("late").value.has_value());
    records.at("late").value->ended_at = ms(at(zone, 2026, 8, 5, 0, 20));

    const std::vector<ReadingSessionDay> days = session_days(records, 0, zone);
    QCOMPARE(days.size(), 2u);
    QCOMPARE(days[0].date_key, std::string("2026-08-03"));
    QCOMPARE(days[0].sessions.size(), 1u);
    QCOMPARE(days[1].date_key, std::string("2026-08-04"));
    QCOMPARE(days[1].sessions.size(), 2u);

    const std::vector<std::string> day_ids = entry_ids(days[1].sessions);
    const std::vector<std::string> expected_ids = {"midnight", "late"};
    QCOMPARE(day_ids, expected_ids);

    const ReadingTotal total = days[1].total();
    QCOMPARE(total.characters_read, 300);
    QCOMPARE(total.reading_time, 1860.0);
    QCOMPARE(days[1].last_modified(), records.at("late").modified);

    // A later reset moves the session that starts before it to the day before.
    const std::vector<ReadingSessionDay> shifted = session_days(records, 60, zone);
    QCOMPARE(shifted.size(), 2u);
    QCOMPARE(shifted[0].date_key, std::string("2026-08-03"));
    QCOMPARE(shifted[0].sessions.size(), 2u);
    QCOMPARE(shifted[1].date_key, std::string("2026-08-04"));
    QCOMPARE(shifted[1].sessions.size(), 1u);

    const ReadingSession only = make_session(at(zone, 2026, 8, 4, 9, 0), 10, 10.0);
    const std::string key = session_date_key(only, 0, zone);
    QCOMPARE(key, std::string("2026-08-04"));

    const ReadingSessionRecords empty;
    QVERIFY(session_days(empty, 0, zone).empty());
}

void StatisticsTest::legacy_ids_are_stable_and_key_specific() {
    const std::string id = legacy_session_id("book", "2026-08-02");
    QCOMPARE(id, std::string("6CC8C587-61BD-F1F0-9768-DF9184DB47C6"));
    QCOMPARE(id, expected_legacy_id(QStringLiteral("book"), QStringLiteral("2026-08-02")));
    QCOMPARE(legacy_session_id("book", "2026-08-02"), id);
    QVERIFY(legacy_session_id("other", "2026-08-02") != id);
    QVERIFY(legacy_session_id("book", "2026-08-04") != id);

    // The key is normalized to NFC before it is hashed; the two spellings of
    // "e with acute" must produce the same id.
    const std::string decomposed = legacy_session_id("e\xCC\x81", "2026-08-02");
    const std::string composed = legacy_session_id("\xC3\xA9", "2026-08-02");
    QCOMPARE(decomposed, composed);
}

void StatisticsTest::legacy_session_uses_the_last_modified_heuristic() {
    const QTimeZone zone = fixed_zone();

    // The estimate fits inside the reporting day, so it wins over the reset boundary.
    const Statistics estimated = day_row("2026-08-02", 1000, 600.0,
                                         ms(at(zone, 2026, 8, 2, 21, 0)));
    const std::optional<ReadingSession> estimated_session = legacy_session(estimated, 0, zone);
    QVERIFY(estimated_session.has_value());
    QCOMPARE(estimated_session->started_at, ms(at(zone, 2026, 8, 2, 20, 50)));
    QCOMPARE(estimated_session->ended_at, ms(at(zone, 2026, 8, 2, 21, 0)));
    QCOMPARE(estimated_session->characters_read, 1000);
    QCOMPARE(estimated_session->reading_time, 600.0);

    // The estimate would start on the previous day, so the reset boundary is used.
    const Statistics crosses = day_row("2026-08-04", 300, 600.0,
                                       ms(at(zone, 2026, 8, 4, 0, 5)));
    const std::optional<ReadingSession> boundary_session = legacy_session(crosses, 0, zone);
    QVERIFY(boundary_session.has_value());
    QCOMPARE(boundary_session->started_at, ms(at(zone, 2026, 8, 4, 0, 0)));
    QCOMPARE(boundary_session->ended_at, ms(at(zone, 2026, 8, 4, 0, 10)));

    // Negative values clamp to zero; a zero duration still takes the estimate.
    const Statistics negative = day_row("2026-08-02", -100, -50.0,
                                        ms(at(zone, 2026, 8, 2, 21, 0)));
    const std::optional<ReadingSession> clamped_session = legacy_session(negative, 0, zone);
    QVERIFY(clamped_session.has_value());
    QCOMPARE(clamped_session->characters_read, 0);
    QCOMPARE(clamped_session->reading_time, 0.0);
    QCOMPARE(clamped_session->started_at, ms(at(zone, 2026, 8, 2, 21, 0)));
    QCOMPARE(clamped_session->ended_at, ms(at(zone, 2026, 8, 2, 21, 0)));

    // The reset hour and minute position the start when the estimate does not fit.
    const Statistics late_reset = day_row("2026-08-02", 300, 600.0,
                                          ms(at(zone, 2026, 8, 3, 12, 0)));
    const std::optional<ReadingSession> reset_session = legacy_session(late_reset, 275, zone);
    QVERIFY(reset_session.has_value());
    QCOMPARE(reset_session->started_at, ms(at(zone, 2026, 8, 2, 4, 35)));
    QCOMPARE(reset_session->ended_at, ms(at(zone, 2026, 8, 2, 4, 45)));

    // A date key needs three integer components after non-integers are dropped.
    QVERIFY(!legacy_session(day_row("2026-08", 1, 1.0, 1), 0, zone).has_value());
    QVERIFY(!legacy_session(day_row("not-a-date", 1, 1.0, 1), 0, zone).has_value());
    QVERIFY(!legacy_session(day_row("2026-08-02-1", 1, 1.0, 1), 0, zone).has_value());
    QVERIFY(!legacy_session(day_row("", 1, 1.0, 1), 0, zone).has_value());

    // A non-integer component is dropped, so the remaining three still parse.
    const Statistics mixed = day_row("2026-xx-08-02", 300, 600.0,
                                     ms(at(zone, 2026, 8, 3, 12, 0)));
    const std::optional<ReadingSession> mixed_session = legacy_session(mixed, 0, zone);
    QVERIFY(mixed_session.has_value());
    QCOMPARE(mixed_session->started_at, ms(at(zone, 2026, 8, 2, 0, 0)));
    QCOMPARE(mixed_session->ended_at, ms(at(zone, 2026, 8, 2, 0, 10)));
}

void StatisticsTest::legacy_sessions_skip_empty_rows_and_keep_ids() {
    const QTimeZone zone = fixed_zone();
    const std::int64_t august_second = ms(at(zone, 2026, 8, 2, 21, 0));

    std::vector<Statistics> rows;
    rows.push_back(day_row("2026-08-02", 1000, 600.0, august_second));
    rows.push_back(day_row("2026-08-03", 0, 0.0, ms(at(zone, 2026, 8, 3, 21, 0))));
    rows.push_back(day_row("2026-08-04", 300, 120.0, ms(at(zone, 2026, 8, 4, 9, 0))));
    // An older duplicate of the same day must be ignored.
    rows.push_back(day_row("2026-08-02", 5, 5.0, august_second - 1));

    const ReadingSessionRecords records = legacy_sessions(rows, "book", 0, zone);
    QCOMPARE(records.size(), 2u);

    const std::string second_id = legacy_session_id("book", "2026-08-02");
    QVERIFY(records.contains(second_id));
    QCOMPARE(records.at(second_id).modified, august_second);
    QVERIFY(records.at(second_id).value.has_value());
    QCOMPARE(records.at(second_id).value->characters_read, 1000);
    QCOMPARE(records.at(second_id).value->reading_time, 600.0);

    const std::string fourth_id = legacy_session_id("book", "2026-08-04");
    QVERIFY(records.contains(fourth_id));
    QVERIFY(records.at(fourth_id).value.has_value());
    QCOMPARE(records.at(fourth_id).value->characters_read, 300);

    QVERIFY(!records.contains(legacy_session_id("book", "2026-08-03")));
}

void StatisticsTest::daily_statistics_write_one_row_per_active_day() {
    const QTimeZone zone = fixed_zone();
    ReadingSessionRecords records;
    records["a"] = timestamped(10, make_session(at(zone, 2026, 8, 4, 9, 0), 1500, 3000.0));
    records["b"] = timestamped(12, make_session(at(zone, 2026, 8, 4, 10, 0), 1000, 1000.0));
    records["c"] = timestamped(13, make_session(at(zone, 2026, 8, 5, 9, 0), 0, 0.0));
    records["d"] = timestamped(5, make_session(at(zone, 2026, 8, 3, 9, 0), 600, 600.0));

    const std::vector<Statistics> rows = daily_statistics(records, "Book", 0, zone);
    QCOMPARE(rows.size(), 2u);

    const std::vector<std::string> keys = row_keys(rows);
    const std::vector<std::string> expected = {"2026-08-03", "2026-08-04"};
    QCOMPARE(keys, expected);

    QCOMPARE(rows[0].title, std::string("Book"));
    QCOMPARE(rows[0].characters_read, 600);
    QCOMPARE(rows[0].reading_time, 600.0);
    QCOMPARE(rows[0].last_reading_speed, 3600);
    QCOMPARE(rows[0].last_statistic_modified, records.at("d").modified);

    QCOMPARE(rows[1].characters_read, 2500);
    QCOMPARE(rows[1].reading_time, 4000.0);
    QCOMPARE(rows[1].min_reading_speed, 2250);
    QCOMPARE(rows[1].alt_min_reading_speed, 2250);
    QCOMPARE(rows[1].last_reading_speed, 2250);
    QCOMPARE(rows[1].max_reading_speed, 2250);
    QCOMPARE(rows[1].last_statistic_modified, records.at("b").modified);

    const ReadingSessionRecords empty;
    QVERIFY(daily_statistics(empty, "Book", 0, zone).empty());
}

void StatisticsTest::deduplicate_keeps_the_newest_modification() {
    std::vector<Statistics> rows;
    rows.push_back(day_row("2026-08-02", 100, 60.0, 5));
    rows.push_back(day_row("2026-08-02", 250, 120.0, 5));
    rows.push_back(day_row("2026-08-02", 900, 300.0, 7));
    rows.push_back(day_row("2026-08-03", 40, 40.0, 1));

    const std::vector<Statistics> deduplicated = deduplicated_statistics(rows);
    QCOMPARE(deduplicated.size(), 2u);

    const Statistics* second = find_row(deduplicated, "2026-08-02");
    QVERIFY(second != nullptr);
    QCOMPARE(second->characters_read, 900);
    QCOMPARE(second->last_statistic_modified, rows[2].last_statistic_modified);

    // On equal modification times the earlier row in input order stays.
    std::vector<Statistics> tied;
    tied.push_back(day_row("2026-08-02", 100, 60.0, 5));
    tied.push_back(day_row("2026-08-02", 250, 120.0, 5));
    const std::vector<Statistics> tied_rows = deduplicated_statistics(tied);
    QCOMPARE(tied_rows.size(), 1u);
    const Statistics* kept = find_row(tied_rows, "2026-08-02");
    QVERIFY(kept != nullptr);
    QCOMPARE(kept->characters_read, 100);
}

void StatisticsTest::visible_rows_hide_tombstones_and_sort() {
    std::vector<Statistics> rows;
    rows.push_back(day_row("2026-08-04", 0, 30.0, 4));
    rows.push_back(day_row("2026-08-03", 0, 0.0, 3));
    rows.push_back(day_row("2026-08-02", 100, 0.0, 1));
    rows.push_back(day_row("2026-08-02", 500, 100.0, 2));

    const std::vector<Statistics> visible = visible_statistics(rows);
    QCOMPARE(visible.size(), 2u);

    const std::vector<std::string> keys = row_keys(visible);
    const std::vector<std::string> expected = {"2026-08-02", "2026-08-04"};
    QCOMPARE(keys, expected);
    QCOMPARE(visible[0].characters_read, 500);

    std::vector<Statistics> all_zero;
    all_zero.push_back(day_row("2026-08-02", 0, 0.0, 1));
    QVERIFY(visible_statistics(all_zero).empty());
}

void StatisticsTest::updating_keeps_the_existing_title_and_recalculates_speed() {
    std::vector<Statistics> rows;
    rows.push_back(day_row("2026-08-02", 100, 60.0, 1, "Stored"));
    rows.push_back(day_row("2026-08-02", 250, 120.0, 2, "Stored"));

    const std::vector<Statistics> updated =
        updating_statistics("2026-08-02", "Book", 600, 120.0, 10, rows);
    const std::int64_t update_stamp = 10;
    QCOMPARE(updated.size(), 1u);
    QCOMPARE(updated[0].title, std::string("Stored"));
    QCOMPARE(updated[0].characters_read, 600);
    QCOMPARE(updated[0].reading_time, 120.0);
    QCOMPARE(updated[0].min_reading_speed, 18000);
    QCOMPARE(updated[0].alt_min_reading_speed, 18000);
    QCOMPARE(updated[0].last_reading_speed, 18000);
    QCOMPARE(updated[0].max_reading_speed, 18000);
    // The new modification time is the caller's argument, not a session stamp.
    QCOMPARE(updated[0].last_statistic_modified, update_stamp);

    // A new date key takes the argument title and sorts after the old row.
    const std::vector<Statistics> appended =
        updating_statistics("2026-08-03", "Book", 300, 300.0, 11, updated);
    QCOMPARE(appended.size(), 2u);
    QCOMPARE(appended[0].date_key, std::string("2026-08-02"));
    QCOMPARE(appended[1].date_key, std::string("2026-08-03"));
    QCOMPARE(appended[1].title, std::string("Book"));

    // Negative input clamps at zero before the speed is derived.
    const std::vector<Statistics> clamped =
        updating_statistics("2026-08-04", "Book", -50, 120.0, 12, appended);
    const Statistics* tombstone = find_row(clamped, "2026-08-04");
    QVERIFY(tombstone != nullptr);
    QCOMPARE(tombstone->characters_read, 0);
    QCOMPARE(tombstone->reading_time, 120.0);
    QCOMPARE(tombstone->last_reading_speed, 0);
    QCOMPARE(tombstone->max_reading_speed, 0);
}

void StatisticsTest::deleting_writes_a_zero_row() {
    std::vector<Statistics> rows;
    rows.push_back(day_row("2026-08-02", 600, 120.0, 10, "Stored"));
    rows.push_back(day_row("2026-08-03", 100, 100.0, 11));

    const std::vector<Statistics> deleted = deleting_statistics("2026-08-02", "Book", 20, rows);
    const std::int64_t delete_stamp = 20;
    QCOMPARE(deleted.size(), 2u);
    QCOMPARE(deleted[0].date_key, std::string("2026-08-02"));
    QCOMPARE(deleted[0].title, std::string("Stored"));
    QCOMPARE(deleted[0].characters_read, 0);
    QCOMPARE(deleted[0].reading_time, 0.0);
    QCOMPARE(deleted[0].min_reading_speed, 0);
    QCOMPARE(deleted[0].alt_min_reading_speed, 0);
    QCOMPARE(deleted[0].last_reading_speed, 0);
    QCOMPARE(deleted[0].max_reading_speed, 0);
    QCOMPARE(deleted[0].last_statistic_modified, delete_stamp);

    const std::vector<Statistics> visible = visible_statistics(deleted);
    QCOMPARE(visible.size(), 1u);
    QCOMPARE(visible[0].date_key, std::string("2026-08-03"));

    // Deleting a day that has no row still writes a tombstone.
    const std::vector<Statistics> missing = deleting_statistics("2026-08-05", "Book", 21, rows);
    const std::int64_t missing_stamp = 21;
    QCOMPARE(missing.size(), 3u);
    const Statistics* appended = find_row(missing, "2026-08-05");
    QVERIFY(appended != nullptr);
    QCOMPARE(appended->characters_read, 0);
    QCOMPARE(appended->title, std::string("Book"));
    QCOMPARE(appended->last_statistic_modified, missing_stamp);
}

void StatisticsTest::deleting_all_leaves_one_zero_row_per_day() {
    std::vector<Statistics> rows;
    rows.push_back(day_row("2026-08-02", 100, 60.0, 1, "Stored"));
    rows.push_back(day_row("2026-08-02", 250, 120.0, 2, "Stored"));
    rows.push_back(day_row("2026-08-03", 0, 0.0, 3, ""));
    rows.push_back(day_row("2026-08-04", 0, 0.0, 4, "Other"));

    const std::vector<Statistics> deleted = deleting_all_statistics("Book", 30, rows);
    const std::int64_t delete_stamp = 30;
    QCOMPARE(deleted.size(), 3u);

    const std::vector<std::string> keys = row_keys(deleted);
    const std::vector<std::string> expected = {"2026-08-02", "2026-08-03", "2026-08-04"};
    QCOMPARE(keys, expected);

    for (const Statistics& row : deleted) {
        QCOMPARE(row.characters_read, 0);
        QCOMPARE(row.reading_time, 0.0);
        QCOMPARE(row.min_reading_speed, 0);
        QCOMPARE(row.alt_min_reading_speed, 0);
        QCOMPARE(row.last_reading_speed, 0);
        QCOMPARE(row.max_reading_speed, 0);
        QCOMPARE(row.last_statistic_modified, delete_stamp);
    }

    QCOMPARE(deleted[0].title, std::string("Stored"));
    QCOMPARE(deleted[1].title, std::string("Book"));
    QCOMPARE(deleted[2].title, std::string("Other"));
    QVERIFY(visible_statistics(deleted).empty());
}

void StatisticsTest::target_settings_snap_to_range_and_step() {
    QCOMPARE(StatisticsTargetSettings::snap_character_target(60000), 50000);
    QCOMPARE(StatisticsTargetSettings::snap_character_target(0), 500);
    QCOMPARE(StatisticsTargetSettings::snap_character_target(500), 500);
    QCOMPARE(StatisticsTargetSettings::snap_character_target(700), 500);
    QCOMPARE(StatisticsTargetSettings::snap_character_target(800), 1000);
    QCOMPARE(StatisticsTargetSettings::snap_character_target(50000), 50000);

    QCOMPARE(StatisticsTargetSettings::snap_duration_target(30), 30);
    QCOMPARE(StatisticsTargetSettings::snap_duration_target(0), 5);
    QCOMPARE(StatisticsTargetSettings::snap_duration_target(7), 5);
    QCOMPARE(StatisticsTargetSettings::snap_duration_target(8), 10);
    QCOMPARE(StatisticsTargetSettings::snap_duration_target(237), 235);
    QCOMPARE(StatisticsTargetSettings::snap_duration_target(300), 240);
    QCOMPARE(StatisticsTargetSettings::snap_duration_target(240), 240);
}

void StatisticsTest::target_settings_clamped_keeps_the_target_kind() {
    StatisticsTargetSettings settings;
    settings.daily_target_type = DailyTargetType::duration;
    settings.daily_character_target = 60000;
    settings.daily_duration_target_minutes = 3;
    settings.weekly_target_days = 9;

    const StatisticsTargetSettings clamped = settings.clamped();
    QCOMPARE(clamped.daily_target_type, DailyTargetType::duration);
    QCOMPARE(clamped.daily_character_target, 50000);
    QCOMPARE(clamped.daily_duration_target_minutes, 5);
    QCOMPARE(clamped.weekly_target_days, 7);

    StatisticsTargetSettings defaults;
    QCOMPARE(defaults.daily_target_type, DailyTargetType::characters);
    QCOMPARE(defaults.daily_character_target, 5000);
    QCOMPARE(defaults.daily_duration_target_minutes, 30);
    QCOMPARE(defaults.weekly_target_days, 4);
    QCOMPARE(defaults.clamped(), defaults);

    defaults.weekly_target_days = 0;
    QCOMPARE(defaults.clamped().weekly_target_days, 1);
}

void StatisticsTest::target_percent_rounds_the_day_ratio() {
    StatisticsTargetSettings characters;
    QCOMPARE(target_percent(characters, 2500, 100.0), 50);
    QCOMPARE(target_percent(characters, 0, 0.0), 0);
    // Half a percent rounds away from zero.
    QCOMPARE(target_percent(characters, 25, 0.0), 1);
    // The ratio is not clamped.
    QCOMPARE(target_percent(characters, -2500, 0.0), -50);

    StatisticsTargetSettings zero_characters;
    zero_characters.daily_character_target = 0;
    QCOMPARE(target_percent(zero_characters, 2500, 100.0), 0);

    StatisticsTargetSettings duration;
    duration.daily_target_type = DailyTargetType::duration;
    QCOMPARE(target_percent(duration, 0, 900.0), 50);
    QCOMPARE(target_percent(duration, 0, 30.0), 2);

    StatisticsTargetSettings zero_duration;
    zero_duration.daily_target_type = DailyTargetType::duration;
    zero_duration.daily_duration_target_minutes = 0;
    QCOMPARE(target_percent(zero_duration, 2500, 900.0), 0);
}

void StatisticsTest::persistence_policy_compares_the_persisted_bookmark() {
    const std::optional<ReaderStatisticsBookmarkSnapshot> absent;
    QVERIFY(should_persist_statistics(17, 80791, absent));

    ReaderStatisticsBookmarkSnapshot bookmark;
    bookmark.chapter_index = 17;
    bookmark.character_count = 80791;
    const std::optional<ReaderStatisticsBookmarkSnapshot> current = bookmark;
    QVERIFY(should_persist_statistics(17, 80791, current));

    QVERIFY(!should_persist_statistics(17, 80273, current));

    ReaderStatisticsBookmarkSnapshot boundary = bookmark;
    boundary.character_count = 72340;
    const std::optional<ReaderStatisticsBookmarkSnapshot> other_chapter = boundary;
    QVERIFY(!should_persist_statistics(16, 72340, other_chapter));

    ReaderStatisticsBookmarkSnapshot backwards = bookmark;
    backwards.character_count = 79736;
    const std::optional<ReaderStatisticsBookmarkSnapshot> earlier_page = backwards;
    QVERIFY(should_persist_statistics(17, 79736, earlier_page));
}

QTEST_APPLESS_MAIN(StatisticsTest)

#include "tst_statistics.moc"
