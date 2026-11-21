// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/statistics/statistics_storage.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QTemporaryDir>
#include <QTest>
#include <QTime>
#include <QTimeZone>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using niratan::statistics::daily_statistics;
using niratan::statistics::has_statistics;
using niratan::statistics::import_daily_statistics;
using niratan::statistics::legacy_session_id;
using niratan::statistics::legacy_sessions;
using niratan::statistics::load_statistics;
using niratan::statistics::read_daily_statistics;
using niratan::statistics::read_dashboard_statistics;
using niratan::statistics::ReadingSession;
using niratan::statistics::ReadingSessionRecords;
using niratan::statistics::save_statistics;
using niratan::statistics::Statistics;
using niratan::statistics::statistics_daily_file_name;
using niratan::statistics::statistics_sessions_file_name;
using niratan::statistics::StatisticsReadResult;
using niratan::statistics::StatisticsSyncMode;
using niratan::statistics::TimestampedSession;

// Core/StatisticsStorage.swift defines the two files, the reconciliation at
// load and the import rules; Models/Statistics.swift defines the legacy
// conversion and the derived rows. Fixtures pin every instant and the Replace
// stamp, so nothing here reads the clock.
namespace {

QTimeZone utc_zone() { return QTimeZone(QTimeZone::UTC); }

std::int64_t ms_at(int year, int month, int day, int hour, int minute) {
    return QDateTime(QDate(year, month, day), QTime(hour, minute), utc_zone())
        .toMSecsSinceEpoch();
}

std::filesystem::path make_root(const QTemporaryDir& directory) {
    const std::filesystem::path root =
        std::filesystem::path(directory.path().toStdString()) / "Book";
    std::error_code error;
    std::filesystem::create_directories(root, error);
    return root;
}

std::filesystem::path sessions_path(const std::filesystem::path& root) {
    return root / std::filesystem::path(statistics_sessions_file_name());
}

std::filesystem::path daily_path(const std::filesystem::path& root) {
    return root / std::filesystem::path(statistics_daily_file_name());
}

ReadingSession make_session(std::int64_t started_at, int characters, double reading_time) {
    ReadingSession session;
    session.started_at = started_at;
    session.ended_at =
        started_at + static_cast<std::int64_t>(reading_time * 1000.0);
    session.characters_read = characters;
    session.reading_time = reading_time;
    return session;
}

TimestampedSession make_record(std::int64_t modified,
                               std::optional<ReadingSession> value) {
    TimestampedSession record;
    record.modified = modified;
    record.value = std::move(value);
    return record;
}

Statistics day_row(std::string date_key, int characters, double reading_time,
                   std::int64_t modified, std::string title = "Book") {
    Statistics row;
    row.title = std::move(title);
    row.date_key = std::move(date_key);
    row.characters_read = characters;
    row.reading_time = reading_time;
    row.last_statistic_modified = modified;
    return row;
}

QJsonObject session_json(const ReadingSession& session) {
    QJsonObject object;
    object.insert(QLatin1StringView("startedAt"),
                  static_cast<qint64>(session.started_at));
    object.insert(QLatin1StringView("endedAt"), static_cast<qint64>(session.ended_at));
    object.insert(QLatin1StringView("charactersRead"), session.characters_read);
    object.insert(QLatin1StringView("readingTime"), session.reading_time);
    return object;
}

QJsonObject record_json(const TimestampedSession& record) {
    QJsonObject object;
    object.insert(QLatin1StringView("modified"), static_cast<qint64>(record.modified));
    QJsonValue value;
    if (record.value.has_value()) {
        value = session_json(*record.value);
    }
    object.insert(QLatin1StringView("value"), value);
    return object;
}

QJsonObject day_json(const Statistics& row) {
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

bool write_document(const std::filesystem::path& path,
                    const QJsonDocument& document) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray bytes = document.toJson(QJsonDocument::Indented);
    return file.write(bytes) == bytes.size();
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(text.data(), static_cast<qint64>(text.size())) ==
           static_cast<qint64>(text.size());
}

bool write_sessions_file(const std::filesystem::path& root,
                         const ReadingSessionRecords& records,
                         const std::map<std::string, std::int64_t>& exported) {
    QJsonObject sessions;
    for (const auto& entry : records) {
        sessions.insert(QString::fromStdString(entry.first), record_json(entry.second));
    }
    QJsonObject exported_daily;
    for (const auto& entry : exported) {
        exported_daily.insert(QString::fromStdString(entry.first),
                              static_cast<qint64>(entry.second));
    }
    QJsonObject file;
    file.insert(QLatin1StringView("sessions"), sessions);
    file.insert(QLatin1StringView("exportedDaily"), exported_daily);
    return write_document(sessions_path(root), QJsonDocument(file));
}

bool write_daily_file(const std::filesystem::path& root,
                      const std::vector<Statistics>& rows) {
    QJsonArray array;
    for (const Statistics& row : rows) {
        array.append(day_json(row));
    }
    return write_document(daily_path(root), QJsonDocument(array));
}

std::optional<QJsonDocument> read_document(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return QJsonDocument::fromJson(file.readAll());
}

QByteArray read_bytes(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

const Statistics* find_row(const std::vector<Statistics>& rows,
                           const std::string& date_key) {
    for (const Statistics& row : rows) {
        if (row.date_key == date_key) {
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

}  // namespace

class StatisticsStorageTest : public QObject {
    Q_OBJECT

   private slots:
    void first_load_converts_legacy_daily_rows();
    void sessions_round_trip_and_tombstones_stay_hidden();
    void exported_daily_controls_reimport();
    void merge_import_skips_newer_days_and_tombstones_cleared_days();
    void replace_import_hides_local_only_days();
    void save_writes_one_row_per_active_day();
    void damaged_files_are_treated_as_missing();
    void daily_view_reads_sessions_without_writing();
    void load_persists_merged_daily_rows();
    void merge_import_gives_a_taken_legacy_id_a_fresh_session();
    void load_returns_records_when_the_reconcile_write_fails();
    void save_writes_the_resolved_title();
    void import_writes_the_first_supplied_title();
    void write_order_keeps_the_sessions_when_the_daily_write_fails();
    void damaged_targets_are_replaced();
    void dashboard_read_is_null_without_reconcilable_statistics();
    void sessions_record_requires_a_value_key();
};

void StatisticsStorageTest::first_load_converts_legacy_daily_rows() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = make_root(directory);
    const QTimeZone zone = utc_zone();
    const std::int64_t first = ms_at(2026, 1, 5, 20, 0);
    const std::int64_t second = ms_at(2026, 1, 6, 21, 30);

    std::vector<Statistics> rows;
    rows.push_back(day_row("2026-01-05", 1200, 300.0, first, "My Book"));
    rows.push_back(day_row("2026-01-06", 600, 120.0, second, "My Book"));
    rows.push_back(day_row("2026-01-07", 0, 0.0, ms_at(2026, 1, 7, 12, 0), "My Book"));
    // An older duplicate of the first day must not produce a session.
    rows.push_back(day_row("2026-01-05", 1, 1.0, first - 1000, "My Book"));
    QVERIFY(write_daily_file(root, rows));

    const StatisticsReadResult loaded = load_statistics(root, 0, zone);
    QVERIFY(loaded.needs_save);

    const ReadingSessionRecords converted = legacy_sessions(rows, "Book", 0, zone);
    QCOMPARE(loaded.records.size(), converted.size());
    QCOMPARE(loaded.records.size(), std::size_t(2));
    QVERIFY(loaded.records == converted);

    const std::string first_id = legacy_session_id("Book", "2026-01-05");
    QVERIFY(loaded.records.contains(first_id));
    QCOMPARE(loaded.records.at(first_id).modified, first);
    QVERIFY(loaded.records.at(first_id).value.has_value());
    QCOMPARE(loaded.records.at(first_id).value->characters_read, 1200);
    QCOMPARE(loaded.records.at(first_id).value->reading_time, 300.0);
    QVERIFY(!loaded.records.contains(legacy_session_id("Book", "2026-01-07")));

    const std::vector<Statistics> expected_daily =
        daily_statistics(converted, "My Book", 0, zone);
    QVERIFY(loaded.daily == expected_daily);
    QCOMPARE(loaded.daily.size(), std::size_t(2));
    QCOMPARE(loaded.daily.front().date_key, std::string("2026-01-05"));
    QCOMPARE(loaded.daily.front().title, std::string("My Book"));

    // B1: StatisticsStorage.load writes a pending reconcile, so the legacy
    // daily file leaves a converted statistics_sessions.json and a re-derived
    // statistics.json (Core/StatisticsStorage.swift, load and write).
    QVERIFY(std::filesystem::exists(sessions_path(root)));
    const std::optional<QJsonDocument> sessions_document = read_document(sessions_path(root));
    QVERIFY(sessions_document.has_value());
    const QJsonObject sessions_file = sessions_document->object();
    const QJsonObject stored_sessions =
        sessions_file.value(QLatin1StringView("sessions")).toObject();
    QCOMPARE(stored_sessions.size(), qsizetype(2));
    QVERIFY(stored_sessions.contains(QString::fromStdString(first_id)));
    QVERIFY(stored_sessions.value(QString::fromStdString(first_id))
                .toObject()
                .value(QLatin1StringView("value"))
                .isObject());
    const QJsonObject stored_exported =
        sessions_file.value(QLatin1StringView("exportedDaily")).toObject();
    QCOMPARE(stored_exported.value(QLatin1StringView("2026-01-05")).toInteger(), qint64(first));
    QCOMPARE(stored_exported.value(QLatin1StringView("2026-01-06")).toInteger(), qint64(second));

    const std::optional<QJsonDocument> daily_document = read_document(daily_path(root));
    QVERIFY(daily_document.has_value());
    QCOMPARE(daily_document->array().size(), qsizetype(2));
    const QJsonObject first_written = daily_document->array().at(0).toObject();
    QCOMPARE(first_written.value(QLatin1StringView("title")).toString(), QStringLiteral("My Book"));
    QCOMPARE(first_written.value(QLatin1StringView("charactersRead")).toInt(), 1200);

    // A second load finds nothing pending and the same records: the write
    // stamped exportedDaily with the derived rows (StatisticsStorage.reconciled).
    const StatisticsReadResult again = load_statistics(root, 0, zone);
    QVERIFY(!again.needs_save);
    QVERIFY(again.records == loaded.records);
    QVERIFY(again.daily == loaded.daily);
}

void StatisticsStorageTest::sessions_round_trip_and_tombstones_stay_hidden() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = make_root(directory);
    const QTimeZone zone = utc_zone();

    ReadingSessionRecords records;
    records["session-a"] =
        make_record(1000, make_session(ms_at(2026, 1, 5, 19, 55), 900, 300.0));
    records["session-b"] = make_record(2000, std::nullopt);

    QVERIFY(save_statistics(root, records, "My Book", 0, zone));

    const std::optional<QJsonDocument> document = read_document(sessions_path(root));
    QVERIFY(document.has_value());
    const QJsonObject file = document->object();
    const QJsonObject stored = file.value(QLatin1StringView("sessions")).toObject();
    QCOMPARE(stored.size(), qsizetype(2));
    QVERIFY(stored.value(QLatin1StringView("session-b")).toObject()
                .value(QLatin1StringView("value"))
                .isNull());
    QCOMPARE(stored.value(QLatin1StringView("session-b"))
                 .toObject()
                 .value(QLatin1StringView("modified"))
                 .toInteger(),
             qint64(2000));
    QVERIFY(stored.value(QLatin1StringView("session-a"))
                .toObject()
                .value(QLatin1StringView("value"))
                .isObject());

    const QJsonObject exported =
        file.value(QLatin1StringView("exportedDaily")).toObject();
    QCOMPARE(exported.size(), qsizetype(1));
    QCOMPARE(exported.value(QLatin1StringView("2026-01-05")).toInteger(), qint64(1000));

    const std::vector<Statistics> daily = read_daily_statistics(root, 0, zone);
    QCOMPARE(daily.size(), std::size_t(1));
    QCOMPARE(daily.front().date_key, std::string("2026-01-05"));
    QCOMPARE(daily.front().characters_read, 900);
    QCOMPARE(daily.front().last_statistic_modified, std::int64_t(1000));

    const StatisticsReadResult loaded = load_statistics(root, 0, zone);
    QVERIFY(!loaded.needs_save);
    QVERIFY(loaded.records == records);
    QCOMPARE(loaded.daily.size(), std::size_t(1));
    QCOMPARE(loaded.daily.front().characters_read, 900);
}

void StatisticsStorageTest::exported_daily_controls_reimport() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = make_root(directory);
    const QTimeZone zone = utc_zone();
    const std::int64_t modified = ms_at(2026, 1, 5, 20, 0);

    ReadingSessionRecords records;
    records["session-a"] = make_record(
        modified, make_session(ms_at(2026, 1, 5, 19, 55), 1000, 300.0));
    QVERIFY(save_statistics(root, records, "My Book", 0, zone));

    // The exported stamp matches, so different totals in the daily file are
    // another writer's stale copy, not new activity.
    std::vector<Statistics> exported;
    exported.push_back(day_row("2026-01-05", 5000, 400.0, modified, "My Book"));
    QVERIFY(write_daily_file(root, exported));

    StatisticsReadResult loaded = load_statistics(root, 0, zone);
    QVERIFY(!loaded.needs_save);
    QCOMPARE(loaded.daily.size(), std::size_t(1));
    QCOMPARE(loaded.daily.front().characters_read, 1000);

    // A newer stamp means the row was written elsewhere and is merged in.
    std::vector<Statistics> external;
    external.push_back(day_row("2026-01-05", 5000, 400.0, modified + 1, "My Book"));
    QVERIFY(write_daily_file(root, external));

    loaded = load_statistics(root, 0, zone);
    QVERIFY(loaded.needs_save);
    QCOMPARE(loaded.daily.size(), std::size_t(1));
    QCOMPARE(loaded.daily.front().characters_read, 5000);
    QCOMPARE(loaded.daily.front().reading_time, 400.0);
    QCOMPARE(loaded.daily.front().last_statistic_modified, modified + 1);
    QCOMPARE(loaded.daily.front().title, std::string("My Book"));
}

void StatisticsStorageTest::merge_import_skips_newer_days_and_tombstones_cleared_days() {
    const QTimeZone zone = utc_zone();

    // A day whose sessions are newer than the imported row stays untouched.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        ReadingSessionRecords records;
        records["local"] =
            make_record(5000, make_session(ms_at(2026, 1, 5, 19, 0), 1000, 300.0));
        QVERIFY(write_sessions_file(root, records, {}));

        std::vector<Statistics> older;
        older.push_back(day_row("2026-01-05", 4000, 200.0, 4000));
        QVERIFY(import_daily_statistics(root, older, StatisticsSyncMode::merge, 0, zone));

        QVERIFY(!std::filesystem::exists(daily_path(root)));
        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QCOMPARE(loaded.records.size(), std::size_t(1));
        QVERIFY(loaded.records.contains("local"));
        QCOMPARE(loaded.records.at("local").modified, std::int64_t(5000));
        QCOMPARE(loaded.daily.size(), std::size_t(1));
        QCOMPARE(loaded.daily.front().characters_read, 1000);
    }

    // A zero-activity row tombstones a day that has sessions.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        ReadingSessionRecords records;
        records["local"] =
            make_record(100, make_session(ms_at(2026, 1, 5, 12, 0), 800, 200.0));
        QVERIFY(write_sessions_file(root, records, {}));

        std::vector<Statistics> cleared;
        cleared.push_back(day_row("2026-01-05", 0, 0.0, 200));
        QVERIFY(import_daily_statistics(root, cleared, StatisticsSyncMode::merge, 0, zone));

        QVERIFY(std::filesystem::exists(daily_path(root)));
        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(loaded.records.contains("local"));
        QVERIFY(!loaded.records.at("local").value.has_value());
        QCOMPARE(loaded.records.at("local").modified, std::int64_t(200));
        QVERIFY(loaded.daily.empty());
    }

    // A zero-activity row for a day without sessions creates nothing.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_sessions_file(root, {}, {}));

        std::vector<Statistics> cleared;
        cleared.push_back(day_row("2026-01-07", 0, 0.0, 300));
        QVERIFY(import_daily_statistics(root, cleared, StatisticsSyncMode::merge, 0, zone));

        QVERIFY(!std::filesystem::exists(daily_path(root)));
        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(loaded.records.empty());
        QVERIFY(!loaded.records.contains(legacy_session_id("Book", "2026-01-07")));
    }
}

void StatisticsStorageTest::replace_import_hides_local_only_days() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = make_root(directory);
    const QTimeZone zone = utc_zone();
    const std::int64_t now = std::int64_t(987654321);

    ReadingSessionRecords records;
    records["local-first"] =
        make_record(100, make_session(ms_at(2026, 1, 5, 19, 0), 1000, 300.0));
    records["local-second"] =
        make_record(200, make_session(ms_at(2026, 1, 6, 19, 0), 2000, 600.0));
    QVERIFY(write_sessions_file(root, records, {}));

    std::vector<Statistics> imported;
    imported.push_back(day_row("2026-01-05", 1500, 250.0, 300));
    imported.push_back(day_row("2026-01-07", 700, 200.0, 500));
    QVERIFY(import_daily_statistics(root, imported, StatisticsSyncMode::replace, 0, zone,
                                    now));

    const StatisticsReadResult loaded = load_statistics(root, 0, zone);
    QCOMPARE(loaded.records.size(), std::size_t(3));
    QVERIFY(loaded.records.contains("local-first"));
    QVERIFY(loaded.records.contains("local-second"));

    // The day keeps its existing id and takes the imported session.
    QVERIFY(loaded.records.at("local-first").value.has_value());
    QCOMPARE(loaded.records.at("local-first").value->characters_read, 1500);
    QCOMPARE(loaded.records.at("local-first").modified, now);

    // The day the import does not cover is tombstoned, so it is not visible.
    QVERIFY(!loaded.records.at("local-second").value.has_value());
    QCOMPARE(loaded.records.at("local-second").modified, now);
    QVERIFY(find_row(loaded.daily, "2026-01-06") == nullptr);

    // A day without a previous session is created under the legacy id.
    const std::string new_id = legacy_session_id("Book", "2026-01-07");
    QVERIFY(loaded.records.contains(new_id));
    QVERIFY(loaded.records.at(new_id).value.has_value());
    QCOMPARE(loaded.records.at(new_id).value->characters_read, 700);
    QCOMPARE(loaded.records.at(new_id).modified, now);

    for (const auto& entry : loaded.records) {
        QCOMPARE(entry.second.modified, now);
    }

    const std::vector<std::string> keys = row_keys(loaded.daily);
    const std::vector<std::string> expected_keys = {"2026-01-05", "2026-01-07"};
    QCOMPARE(keys, expected_keys);

    const std::optional<QJsonDocument> document = read_document(daily_path(root));
    QVERIFY(document.has_value());
    QCOMPARE(document->array().size(), qsizetype(2));
}

void StatisticsStorageTest::save_writes_one_row_per_active_day() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = make_root(directory);
    const QTimeZone zone = utc_zone();

    ReadingSessionRecords records;
    records["a"] = make_record(10, make_session(ms_at(2026, 1, 5, 10, 0), 100, 60.0));
    records["b"] = make_record(25, make_session(ms_at(2026, 1, 5, 12, 0), 200, 60.0));
    records["c"] = make_record(20, make_session(ms_at(2026, 1, 6, 10, 0), 350, 100.0));
    records["removed"] = make_record(30, std::nullopt);
    records["empty"] = make_record(40, make_session(ms_at(2026, 1, 7, 10, 0), 0, 0.0));

    QVERIFY(save_statistics(root, records, "My Book", 0, zone));

    const std::optional<QJsonDocument> daily_document = read_document(daily_path(root));
    QVERIFY(daily_document.has_value());
    const QJsonArray array = daily_document->array();
    QCOMPARE(array.size(), qsizetype(2));

    const std::vector<Statistics> expected =
        daily_statistics(records, "My Book", 0, zone);
    QCOMPARE(expected.size(), std::size_t(2));
    QCOMPARE(expected.front().date_key, std::string("2026-01-05"));
    QCOMPARE(expected.front().characters_read, 300);
    QCOMPARE(expected.front().reading_time, 120.0);
    QCOMPARE(expected.front().last_reading_speed, 9000);
    QCOMPARE(expected.front().last_statistic_modified, std::int64_t(25));
    QCOMPARE(expected.back().date_key, std::string("2026-01-06"));
    QCOMPARE(expected.back().last_reading_speed, 12600);
    QCOMPARE(expected.back().last_statistic_modified, std::int64_t(20));

    const std::optional<QJsonDocument> sessions_document =
        read_document(sessions_path(root));
    QVERIFY(sessions_document.has_value());
    const QJsonObject exported = sessions_document->object()
                                     .value(QLatin1StringView("exportedDaily"))
                                     .toObject();
    QCOMPARE(exported.size(), qsizetype(2));
    QCOMPARE(exported.value(QLatin1StringView("2026-01-05")).toInteger(), qint64(25));
    QCOMPARE(exported.value(QLatin1StringView("2026-01-06")).toInteger(), qint64(20));

    QFile sessions_file(QString::fromStdString(sessions_path(root).string()));
    QVERIFY(sessions_file.open(QIODevice::ReadOnly));
    QVERIFY(sessions_file.readAll().contains('\n'));

    const StatisticsReadResult loaded = load_statistics(root, 0, zone);
    QVERIFY(!loaded.needs_save);
    QVERIFY(loaded.records == records);
    QCOMPARE(loaded.daily.size(), std::size_t(2));
    QCOMPARE(row_keys(loaded.daily), row_keys(expected));
}

void StatisticsStorageTest::damaged_files_are_treated_as_missing() {
    const QTimeZone zone = utc_zone();

    // Not JSON at all: the file exists but contributes nothing.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(sessions_path(root), "{ not json"));

        QVERIFY(has_statistics(root));
        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(loaded.records.empty());
        QVERIFY(loaded.daily.empty());
        QVERIFY(!loaded.needs_save);
        QVERIFY(read_daily_statistics(root, 0, zone).empty());
    }

    // Valid JSON of the wrong shape: both files are rejected.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(sessions_path(root),
                           "{\"sessions\":[],\"exportedDaily\":[]}"));
        QVERIFY(write_text(daily_path(root), "{\"rows\":[]}"));

        QVERIFY(has_statistics(root));
        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(loaded.records.empty());
        QVERIFY(loaded.daily.empty());
        QVERIFY(!loaded.needs_save);
    }

    // One missing session key rejects the whole file; the daily rows migrate.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(
            sessions_path(root),
            "{\"sessions\":{\"id\":{\"modified\":1,\"value\":{\"startedAt\":0,"
            "\"endedAt\":0,\"charactersRead\":5}}},\"exportedDaily\":{}}"));
        std::vector<Statistics> rows;
        rows.push_back(day_row("2026-01-05", 1000, 300.0, ms_at(2026, 1, 5, 20, 0),
                               "My Book"));
        QVERIFY(write_daily_file(root, rows));

        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(loaded.needs_save);
        QCOMPARE(loaded.records.size(), std::size_t(1));
        QCOMPARE(loaded.daily.size(), std::size_t(1));
    }

    // An unreadable path (a directory) is present but unusable.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(std::filesystem::create_directories(sessions_path(root)));

        QVERIFY(has_statistics(root));
        QVERIFY(load_statistics(root, 0, zone).records.empty());
    }

    // Neither path exists.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(!has_statistics(root));
        QVERIFY(load_statistics(root, 0, zone).daily.empty());
    }
}

void StatisticsStorageTest::daily_view_reads_sessions_without_writing() {
    const QTimeZone zone = utc_zone();

    // B4: StatisticsStorage.dailyStatistics applies the same reconcile in
    // memory, so an external daily row is visible and nothing is written
    // (Core/StatisticsStorage.swift, reconciled and dailyStatistics).
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        ReadingSessionRecords records;
        records["live"] =
            make_record(1000, make_session(ms_at(2026, 1, 5, 12, 0), 800, 200.0));
        QVERIFY(write_sessions_file(root, records, {}));
        std::vector<Statistics> external;
        external.push_back(day_row("2026-01-06", 999, 50.0, 4000, "My Book"));
        QVERIFY(write_daily_file(root, external));

        const QByteArray sessions_before = read_bytes(sessions_path(root));
        const QByteArray daily_before = read_bytes(daily_path(root));

        const std::vector<Statistics> daily = read_daily_statistics(root, 0, zone);
        QCOMPARE(daily.size(), std::size_t(2));
        const Statistics* own_day = find_row(daily, "2026-01-05");
        QVERIFY(own_day != nullptr);
        QCOMPARE(own_day->characters_read, 800);
        const Statistics* merged_day = find_row(daily, "2026-01-06");
        QVERIFY(merged_day != nullptr);
        QCOMPARE(merged_day->characters_read, 999);
        QCOMPARE(merged_day->reading_time, 50.0);
        QCOMPARE(merged_day->last_statistic_modified, std::int64_t(4000));
        QCOMPARE(merged_day->title, std::string("My Book"));

        // The reconcile stays in memory, so both files keep their bytes.
        QCOMPARE(read_bytes(sessions_path(root)), sessions_before);
        QCOMPARE(read_bytes(daily_path(root)), daily_before);
    }

    // B4: without a sessions file the stored rows are converted and derived
    // in memory, and no sessions file appears
    // (StatisticsStorage.dailyStatistics, ReadingSessionLog.legacySessions).
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        std::vector<Statistics> rows;
        rows.push_back(day_row("2026-01-05", 700, 200.0, 5000, "My Book"));
        rows.push_back(day_row("2026-01-05", 5, 5.0, 4000, "My Book"));
        rows.push_back(day_row("2026-01-06", 0, 0.0, 6000, "My Book"));
        QVERIFY(write_daily_file(root, rows));

        const ReadingSessionRecords converted = legacy_sessions(rows, "Book", 0, zone);
        const std::vector<Statistics> expected = daily_statistics(converted, "My Book", 0, zone);
        const std::vector<Statistics> daily = read_daily_statistics(root, 0, zone);
        QCOMPARE(daily.size(), std::size_t(1));
        QCOMPARE(daily.front().date_key, std::string("2026-01-05"));
        QCOMPARE(daily.front().characters_read, 700);
        QVERIFY(daily == expected);
        QVERIFY(!std::filesystem::exists(sessions_path(root)));
    }

    // B4: a changed reset minute regroups the same sessions instead of
    // duplicating them, and its own export is not re-imported
    // (StatisticsStorage.dailyStatistics and reconciled).
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        ReadingSessionRecords records;
        records["live"] = make_record(1000, make_session(ms_at(2026, 1, 5, 12, 0), 800, 200.0));
        QVERIFY(write_text(root / "metadata.json", R"({"title":"My Book"})"));
        QVERIFY(save_statistics(root, records, "My Book", 0, zone));
        const QByteArray sessions_before = read_bytes(sessions_path(root));

        const std::vector<Statistics> daily = read_daily_statistics(root, 23 * 60, zone);
        QVERIFY(daily == daily_statistics(records, "My Book", 23 * 60, zone));
        QCOMPARE(daily.size(), std::size_t(1));
        QCOMPARE(daily.front().date_key, std::string("2026-01-04"));

        const StatisticsReadResult loaded = load_statistics(root, 23 * 60, zone);
        QVERIFY(!loaded.needs_save);
        QVERIFY(loaded.records == records);
        QCOMPARE(read_bytes(sessions_path(root)), sessions_before);
    }
}

// B2: StatisticsStorage.reconciled merges external daily rows into the
// records, and StatisticsStorage.load persists them with the sessions
// (ReadingSessionLog.importingDaily(.merge), StatisticsEditor.deduplicated).
void StatisticsStorageTest::load_persists_merged_daily_rows() {
    const QTimeZone zone = utc_zone();

    // A newer external row replaces the sessions of its day.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        ReadingSessionRecords records;
        records["local"] = make_record(1000, make_session(ms_at(2026, 1, 5, 19, 55), 800, 200.0));
        QVERIFY(write_sessions_file(root, records, {}));
        std::vector<Statistics> external;
        external.push_back(day_row("2026-01-05", 2000, 300.0, 5000, "My Book"));
        QVERIFY(write_daily_file(root, external));

        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(loaded.needs_save);
        QCOMPARE(loaded.records.size(), std::size_t(1));
        QVERIFY(loaded.records.contains("local"));
        QCOMPARE(loaded.records.at("local").modified, std::int64_t(5000));
        QVERIFY(loaded.records.at("local").value.has_value());
        QCOMPARE(loaded.records.at("local").value->characters_read, 2000);
        QCOMPARE(loaded.daily.size(), std::size_t(1));
        QCOMPARE(loaded.daily.front().characters_read, 2000);

        // The merged record and its export stamp reached the sessions file.
        const std::optional<QJsonDocument> sessions_document = read_document(sessions_path(root));
        QVERIFY(sessions_document.has_value());
        const QJsonObject stored =
            sessions_document->object().value(QLatin1StringView("sessions")).toObject();
        const QJsonObject stored_local = stored.value(QLatin1StringView("local")).toObject();
        QCOMPARE(stored_local.value(QLatin1StringView("modified")).toInteger(), qint64(5000));
        QCOMPARE(stored_local.value(QLatin1StringView("value"))
                     .toObject()
                     .value(QLatin1StringView("charactersRead"))
                     .toInt(),
                 2000);
        const QJsonObject exported =
            sessions_document->object().value(QLatin1StringView("exportedDaily")).toObject();
        QCOMPARE(exported.value(QLatin1StringView("2026-01-05")).toInteger(), qint64(5000));

        // The daily file was re-derived from the merged records.
        const std::optional<QJsonDocument> daily_document = read_document(daily_path(root));
        QVERIFY(daily_document.has_value());
        QCOMPARE(daily_document->array().size(), qsizetype(1));
        const QJsonObject merged_row = daily_document->array().at(0).toObject();
        QCOMPARE(merged_row.value(QLatin1StringView("charactersRead")).toInt(), 2000);
        QCOMPARE(merged_row.value(QLatin1StringView("title")).toString(),
                 QStringLiteral("My Book"));

        const StatisticsReadResult again = load_statistics(root, 0, zone);
        QVERIFY(!again.needs_save);
        QVERIFY(again.records == loaded.records);
        QVERIFY(again.daily == loaded.daily);
    }

    // A row older than its day is skipped, yet the reconcile is still pending.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        ReadingSessionRecords records;
        records["local"] = make_record(5000, make_session(ms_at(2026, 1, 5, 12, 0), 800, 200.0));
        QVERIFY(write_sessions_file(root, records, {}));
        std::vector<Statistics> older;
        older.push_back(day_row("2026-01-05", 4000, 200.0, 4000, "My Book"));
        QVERIFY(write_daily_file(root, older));

        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(loaded.needs_save);
        QCOMPARE(loaded.records.size(), std::size_t(1));
        QVERIFY(loaded.records.at("local").value.has_value());
        QCOMPARE(loaded.records.at("local").value->characters_read, 800);
        QCOMPARE(loaded.records.at("local").modified, std::int64_t(5000));
        QCOMPARE(loaded.daily.size(), std::size_t(1));
        QCOMPARE(loaded.daily.front().characters_read, 800);

        // The derived stamps were persisted even though no record changed.
        const std::optional<QJsonDocument> sessions_document = read_document(sessions_path(root));
        QVERIFY(sessions_document.has_value());
        const QJsonObject exported =
            sessions_document->object().value(QLatin1StringView("exportedDaily")).toObject();
        QCOMPARE(exported.value(QLatin1StringView("2026-01-05")).toInteger(), qint64(5000));
        const std::optional<QJsonDocument> daily_document = read_document(daily_path(root));
        QVERIFY(daily_document.has_value());
        QCOMPARE(daily_document->array().size(), qsizetype(1));
        QCOMPARE(daily_document->array()
                     .at(0)
                     .toObject()
                     .value(QLatin1StringView("charactersRead"))
                     .toInt(),
                 800);

        const StatisticsReadResult again = load_statistics(root, 0, zone);
        QVERIFY(!again.needs_save);
        QVERIFY(again.records == loaded.records);
        QVERIFY(again.daily == loaded.daily);
    }
}

// B2: ReadingSessionLog.importingDaily imports a foreign row whose legacy id is
// already used by a record on another day under a fresh session id.
void StatisticsStorageTest::merge_import_gives_a_taken_legacy_id_a_fresh_session() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = make_root(directory);
    const QTimeZone zone = utc_zone();
    const std::string legacy_id = legacy_session_id("Book", "2026-01-06");

    ReadingSessionRecords records;
    records[legacy_id] = make_record(1000, make_session(ms_at(2026, 1, 5, 12, 0), 800, 200.0));
    QVERIFY(write_sessions_file(root, records, {}));
    std::vector<Statistics> external;
    external.push_back(day_row("2026-01-06", 500, 100.0, 4000, "My Book"));
    QVERIFY(write_daily_file(root, external));

    const StatisticsReadResult loaded = load_statistics(root, 0, zone);
    QCOMPARE(loaded.records.size(), std::size_t(2));
    QVERIFY(loaded.records.contains(legacy_id));
    QCOMPARE(loaded.records.at(legacy_id).modified, std::int64_t(1000));
    QVERIFY(loaded.records.at(legacy_id).value.has_value());
    QCOMPARE(loaded.records.at(legacy_id).value->characters_read, 800);

    const TimestampedSession* imported = nullptr;
    for (const auto& entry : loaded.records) {
        if (entry.first != legacy_id) {
            imported = &entry.second;
        }
    }
    QVERIFY(imported != nullptr);
    QCOMPARE(imported->modified, std::int64_t(4000));
    QVERIFY(imported->value.has_value());
    QCOMPARE(imported->value->characters_read, 500);
}

// B3: StatisticsStorage.load swallows a failed reconcile write and still
// returns the reconciled records.
void StatisticsStorageTest::load_returns_records_when_the_reconcile_write_fails() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = make_root(directory);
    const QTimeZone zone = utc_zone();

    // A directory at the sessions path makes the write fail.
    QVERIFY(std::filesystem::create_directories(sessions_path(root)));
    std::vector<Statistics> rows;
    rows.push_back(day_row("2026-01-05", 1200, 300.0, ms_at(2026, 1, 5, 20, 0), "My Book"));
    QVERIFY(write_daily_file(root, rows));

    const StatisticsReadResult loaded = load_statistics(root, 0, zone);
    QVERIFY(loaded.needs_save);
    QCOMPARE(loaded.records.size(), std::size_t(1));
    QVERIFY(loaded.records.contains(legacy_session_id("Book", "2026-01-05")));
    QCOMPARE(loaded.daily.size(), std::size_t(1));
    QCOMPARE(loaded.daily.front().characters_read, 1200);

    // The failed write left the sessions target and the daily file alone.
    QVERIFY(std::filesystem::is_directory(sessions_path(root)));
    const std::optional<QJsonDocument> daily_document = read_document(daily_path(root));
    QVERIFY(daily_document.has_value());
    QCOMPARE(daily_document->array().size(), qsizetype(1));
    QCOMPARE(
        daily_document->array().at(0).toObject().value(QLatin1StringView("charactersRead")).toInt(),
        1200);
}

// B5: rows carry StatisticsStorage.title(root:daily:), the resolved title,
// not one held by the caller, through save and write.
void StatisticsStorageTest::save_writes_the_resolved_title() {
    const QTimeZone zone = utc_zone();
    ReadingSessionRecords records;
    records["a"] = make_record(10, make_session(ms_at(2026, 1, 5, 10, 0), 100, 60.0));

    // metadata.json's renamedTitle when no stored row has a title.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(root / "metadata.json",
                           R"({"renamedTitle":"Renamed Book","title":"Metadata Book"})"));
        QVERIFY(save_statistics(root, records, "Caller Title", 0, zone));
        const std::vector<Statistics> daily = read_daily_statistics(root, 0, zone);
        QVERIFY(daily == daily_statistics(records, "Renamed Book", 0, zone));
        QCOMPARE(daily.front().title, std::string("Renamed Book"));
    }

    // The first non-empty stored row title outranks metadata.json.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(root / "metadata.json",
                           R"({"renamedTitle":"Renamed Book","title":"Metadata Book"})"));
        std::vector<Statistics> stored;
        stored.push_back(day_row("2026-01-05", 5, 5.0, 100, "Stored Title"));
        QVERIFY(write_daily_file(root, stored));
        QVERIFY(save_statistics(root, records, "Caller Title", 0, zone));
        const std::vector<Statistics> daily = read_daily_statistics(root, 0, zone);
        QVERIFY(daily == daily_statistics(records, "Stored Title", 0, zone));
        QCOMPARE(daily.front().title, std::string("Stored Title"));
    }

    // metadata.json's title when renamedTitle is absent.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(root / "metadata.json", R"({"title":"Metadata Title"})"));
        QVERIFY(save_statistics(root, records, "Caller Title", 0, zone));
        const std::vector<Statistics> daily = read_daily_statistics(root, 0, zone);
        QVERIFY(daily == daily_statistics(records, "Metadata Title", 0, zone));
        QCOMPARE(daily.front().title, std::string("Metadata Title"));
    }

    // An empty title when no stored row and no metadata.json exist.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(save_statistics(root, records, "Caller Title", 0, zone));
        const std::vector<Statistics> daily = read_daily_statistics(root, 0, zone);
        QVERIFY(daily == daily_statistics(records, "", 0, zone));
        QVERIFY(daily.front().title.empty());
    }
}

// B5: StatisticsStorage.importDaily stores the first supplied row's title:
// title: statistics.first?.title ?? title(root:daily:).
void StatisticsStorageTest::import_writes_the_first_supplied_title() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = make_root(directory);

    QVERIFY(import_daily_statistics(root,
                                    {day_row("2026-01-05", 1000, 300.0, 5000, "Imported Title")},
                                    StatisticsSyncMode::merge, 0, utc_zone(), 1234));

    const std::optional<QJsonDocument> daily_document = read_document(daily_path(root));
    QVERIFY(daily_document.has_value());
    QCOMPARE(daily_document->array().size(), qsizetype(1));
    const QJsonObject row = daily_document->array().at(0).toObject();
    QCOMPARE(row.value(QLatin1StringView("title")).toString(), QStringLiteral("Imported Title"));
    QCOMPARE(row.value(QLatin1StringView("charactersRead")).toInt(), 1000);
    QCOMPARE(row.value(QLatin1StringView("lastStatisticModified")).toInteger(), qint64(5000));
}

// B6: StatisticsStorage.write writes statistics_sessions.json first, then
// statistics.json; a daily failure leaves the new sessions file and reports
// the error. StatisticsStorage.encode replaces a damaged target wholesale
// (B7), which is what makes the written sessions observable here.
void StatisticsStorageTest::write_order_keeps_the_sessions_when_the_daily_write_fails() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = make_root(directory);
    const QTimeZone zone = utc_zone();

    QVERIFY(write_text(sessions_path(root), "{ not json"));
    QVERIFY(std::filesystem::create_directories(daily_path(root)));

    ReadingSessionRecords records;
    records["live"] = make_record(1000, make_session(ms_at(2026, 1, 5, 12, 0), 800, 200.0));

    std::string error;
    QVERIFY(!save_statistics(root, records, "My Book", 0, zone, &error));
    QVERIFY(!error.empty());

    // The sessions file was replaced before the daily write failed.
    const std::optional<QJsonDocument> sessions_document = read_document(sessions_path(root));
    QVERIFY(sessions_document.has_value());
    const QJsonObject stored_sessions =
        sessions_document->object().value(QLatin1StringView("sessions")).toObject();
    QCOMPARE(stored_sessions.size(), qsizetype(1));
    QVERIFY(stored_sessions.contains(QStringLiteral("live")));
    QVERIFY(stored_sessions.value(QStringLiteral("live"))
                .toObject()
                .value(QLatin1StringView("value"))
                .isObject());
    QVERIFY(std::filesystem::is_directory(daily_path(root)));

    // A later read finds the written sessions and still returns the records.
    const StatisticsReadResult loaded = load_statistics(root, 0, zone);
    QVERIFY(loaded.records == records);
}

// B7: StatisticsStorage.encode writes through Data.write(options: .atomic),
// so a target whose bytes are not a parseable JSON document is still replaced
// by save_statistics and by a pending reconcile in load_statistics.
void StatisticsStorageTest::damaged_targets_are_replaced() {
    const QTimeZone zone = utc_zone();
    ReadingSessionRecords records;
    records["live"] = make_record(1000, make_session(ms_at(2026, 1, 5, 12, 0), 800, 200.0));

    // Both files hold bytes that are not JSON documents.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(sessions_path(root), "{ not json"));
        QVERIFY(write_text(daily_path(root), "[ not json"));

        QVERIFY(save_statistics(root, records, "My Book", 0, zone));

        const std::optional<QJsonDocument> sessions_document = read_document(sessions_path(root));
        QVERIFY(sessions_document.has_value());
        QVERIFY(sessions_document->object()
                    .value(QLatin1StringView("sessions"))
                    .toObject()
                    .contains(QStringLiteral("live")));
        const std::optional<QJsonDocument> daily_document = read_document(daily_path(root));
        QVERIFY(daily_document.has_value());
        QCOMPARE(daily_document->array().size(), qsizetype(1));

        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(!loaded.needs_save);
        QVERIFY(loaded.records == records);
        QCOMPARE(loaded.daily.size(), std::size_t(1));
    }

    // A pending reconcile replaces a damaged sessions target on load.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(sessions_path(root), "{ not json"));
        std::vector<Statistics> rows;
        rows.push_back(day_row("2026-01-05", 1200, 300.0, ms_at(2026, 1, 5, 20, 0), "My Book"));
        QVERIFY(write_daily_file(root, rows));

        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(loaded.needs_save);
        QCOMPARE(loaded.records.size(), std::size_t(1));

        const std::optional<QJsonDocument> sessions_document = read_document(sessions_path(root));
        QVERIFY(sessions_document.has_value());
        QVERIFY(sessions_document->object()
                    .value(QLatin1StringView("sessions"))
                    .toObject()
                    .contains(QString::fromStdString(legacy_session_id("Book", "2026-01-05"))));
    }
}

// B8: StatisticsStorage.dailyStatistics is nil when the reconciled records are
// empty and statistics.json did not decode; a book with neither file is an
// empty view, not corrupt.
void StatisticsStorageTest::dashboard_read_is_null_without_reconcilable_statistics() {
    const QTimeZone zone = utc_zone();

    // Neither file: the book is skipped by its has_statistics gate.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(!has_statistics(root));
        const std::optional<std::vector<Statistics>> daily =
            read_dashboard_statistics(root, 0, zone);
        QVERIFY(daily.has_value());
        QVERIFY(daily->empty());
    }

    // A valid empty sessions file and no daily file: no records and no daily
    // document, so the dashboard has nothing to show.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_sessions_file(root, {}, {}));
        QVERIFY(has_statistics(root));
        QVERIFY(!read_dashboard_statistics(root, 0, zone).has_value());
        QVERIFY(read_daily_statistics(root, 0, zone).empty());
    }

    // Only tombstones: the records dictionary is non-empty even though no
    // session is live, so the guard passes and the view is empty.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_sessions_file(root, {{"gone", make_record(7000, std::nullopt)}}, {}));
        QVERIFY(read_dashboard_statistics(root, 0, zone).has_value());
        QVERIFY(read_dashboard_statistics(root, 0, zone)->empty());
    }

    // A decodable empty daily array is still a document: an empty view, not nil.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_daily_file(root, {}));
        const std::optional<std::vector<Statistics>> daily =
            read_dashboard_statistics(root, 0, zone);
        QVERIFY(daily.has_value());
        QVERIFY(daily->empty());
    }

    // Both files damaged: nil.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(sessions_path(root), "{ not json"));
        QVERIFY(write_text(daily_path(root), "{ not json"));
        QVERIFY(!read_dashboard_statistics(root, 0, zone).has_value());
    }
}

// B9: StatisticsStorage.SessionsFile and Timestamped's synthesized Codable:
// the value key is required, null is a tombstone, a missing key rejects the
// whole sessions file, and both top-level keys are required.
void StatisticsStorageTest::sessions_record_requires_a_value_key() {
    const QTimeZone zone = utc_zone();

    // One record without the value key rejects the file; the daily rows are
    // converted instead.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(sessions_path(root),
                           "{\"sessions\":{"
                           "\"good\":{\"modified\":1,\"value\":{\"startedAt\":0,\"endedAt\":0,"
                           "\"charactersRead\":5,\"readingTime\":1}},"
                           "\"bad\":{\"modified\":2}},"
                           "\"exportedDaily\":{}}"));
        std::vector<Statistics> rows;
        rows.push_back(day_row("2026-01-05", 1000, 300.0, ms_at(2026, 1, 5, 20, 0), "My Book"));
        QVERIFY(write_daily_file(root, rows));

        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(loaded.needs_save);
        QVERIFY(!loaded.records.contains("good"));
        QVERIFY(loaded.records == legacy_sessions(rows, "Book", 0, zone));
    }

    // A missing exportedDaily key also rejects the file.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        QVERIFY(write_text(sessions_path(root),
                           "{\"sessions\":{\"good\":{\"modified\":1,\"value\":{"
                           "\"startedAt\":0,\"endedAt\":0,\"charactersRead\":5,"
                           "\"readingTime\":1}}}}"));
        std::vector<Statistics> rows;
        rows.push_back(day_row("2026-01-05", 1000, 300.0, ms_at(2026, 1, 5, 20, 0), "My Book"));
        QVERIFY(write_daily_file(root, rows));

        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(!loaded.records.contains("good"));
        QVERIFY(loaded.records == legacy_sessions(rows, "Book", 0, zone));
    }

    // A null value is a tombstone that keeps the id and its modification time.
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::filesystem::path root = make_root(directory);
        ReadingSessionRecords records;
        records["gone"] = make_record(7000, std::nullopt);
        QVERIFY(write_sessions_file(root, records, {}));

        const StatisticsReadResult loaded = load_statistics(root, 0, zone);
        QVERIFY(!loaded.needs_save);
        QCOMPARE(loaded.records.size(), std::size_t(1));
        QVERIFY(loaded.records.contains("gone"));
        QCOMPARE(loaded.records.at("gone").modified, std::int64_t(7000));
        QVERIFY(!loaded.records.at("gone").value.has_value());
        QVERIFY(loaded.daily.empty());
        QVERIFY(!std::filesystem::exists(daily_path(root)));
    }
}

QTEST_APPLESS_MAIN(StatisticsStorageTest)

#include "tst_statistics_storage.moc"
