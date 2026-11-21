// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/reading_tracker.h"

#include <QDate>
#include <QDateTime>
#include <QTest>
#include <QTime>
#include <QTimeZone>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using niratan::reader::ReadingTracker;
using niratan::reader::StatisticsStore;
using niratan::statistics::ReadingSession;
using niratan::statistics::ReadingSessionRecords;
using niratan::statistics::StatisticsAutostartMode;
using niratan::statistics::TimestampedSession;

// NativeReaderModel's statistics state machine: autostart, context pause and
// resume, time and character credit, day rotation, save conditions and the
// read-only totals. Every instant is fixed in UTC; the store is an in-memory
// double, so no file or clock is touched.
namespace {

QTimeZone utc_zone() { return QTimeZone(QTimeZone::UTC); }

std::int64_t ms_at(int year, int month, int day, int hour, int minute, int second = 0) {
    return QDateTime(QDate(year, month, day), QTime(hour, minute, second), utc_zone())
        .toMSecsSinceEpoch();
}

TimestampedSession stored_session(std::int64_t modified, std::int64_t started_at,
                                  int characters, double reading_time) {
    ReadingSession session;
    session.started_at = started_at;
    session.ended_at = started_at + static_cast<std::int64_t>(reading_time * 1000.0);
    session.characters_read = characters;
    session.reading_time = reading_time;

    TimestampedSession record;
    record.modified = modified;
    record.value = session;
    return record;
}

class MemoryStore final : public StatisticsStore {
   public:
    ReadingSessionRecords load(int, const QTimeZone&) override {
        ++load_count;
        return records;
    }

    bool save(const ReadingSessionRecords& next, std::string_view title, int, const QTimeZone&,
              std::string* error) override {
        if (fail_saves) {
            if (error != nullptr) {
                *error = "memory store refused the write";
            }
            return false;
        }
        records = next;
        saved_titles.emplace_back(title);
        ++save_count;
        return true;
    }

    ReadingSessionRecords records;
    bool fail_saves = false;
    int load_count = 0;
    int save_count = 0;
    std::vector<std::string> saved_titles;
};

void begin_tracking(ReadingTracker& tracker, std::int64_t now_ms) {
    tracker.configure(true, StatisticsAutostartMode::on, 0);
    tracker.set_context_active(true, now_ms);
    tracker.start(now_ms);
}

}  // namespace

class ReadingTrackerTest : public QObject {
    Q_OBJECT

   private slots:
    void disabled_never_starts();
    void autostart_off_never_starts();
    void autostart_on_starts_at_start();
    void autostart_on_starts_paused_without_context();
    void autostart_page_turn_waits_for_navigation();
    void manual_navigation_only_triggers_page_turn_mode();
    void credits_only_active_time_across_pause();
    void flush_credits_time_and_refuses_while_paused();
    void negative_delta_clamps_at_zero();
    void non_positive_time_delta_keeps_the_baseline();
    void forward_jump_is_credited_and_moves_the_baseline();
    void stop_flushes_before_leaving_tracking();
    void start_keeps_a_session_with_activity();
    void day_rotation_saves_the_old_session();
    void day_rotation_honours_reset_minutes();
    void day_rotation_reuses_the_saved_title();
    void save_requires_activity_and_change();
    void save_failure_is_retried();
    void save_adopts_a_newer_external_session();
    void save_starts_a_fresh_session_after_a_tombstone();
    void totals_add_the_live_session_once();
    void configure_regroups_for_a_new_reset_minute();
    void configure_regroups_after_a_disabled_reset_change();
};

void ReadingTrackerTest::disabled_never_starts() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);

    tracker.configure(false, StatisticsAutostartMode::on, 0);
    tracker.set_context_active(true, t0);
    tracker.start(t0);
    QVERIFY(!tracker.is_tracking());

    tracker.configure(false, StatisticsAutostartMode::page_turn, 0);
    tracker.on_manual_navigation(t0 + 1000);
    QVERIFY(!tracker.is_tracking());

    tracker.update_position(100, t0 + 1000);
    QCOMPARE(tracker.session_total().characters_read, 0);
    QVERIFY(!tracker.save(t0 + 2000, "Book"));
    QCOMPARE(store.load_count, 0);
    QCOMPARE(store.save_count, 0);
}

void ReadingTrackerTest::autostart_off_never_starts() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);

    tracker.configure(true, StatisticsAutostartMode::off, 0);
    tracker.set_context_active(true, t0);
    tracker.start(t0);
    QVERIFY(!tracker.is_tracking());

    tracker.on_manual_navigation(t0 + 1000);
    QVERIFY(!tracker.is_tracking());

    tracker.update_position(100, t0 + 2000);
    QCOMPARE(tracker.session_total().characters_read, 0);
}

void ReadingTrackerTest::autostart_on_starts_at_start() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);

    tracker.configure(true, StatisticsAutostartMode::on, 0);
    QVERIFY(!tracker.is_tracking());

    tracker.set_context_active(true, t0);
    tracker.start(t0);
    QVERIFY(tracker.is_tracking());
    QVERIFY(!tracker.is_paused());
    QCOMPARE(store.load_count, 1);
}

void ReadingTrackerTest::autostart_on_starts_paused_without_context() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);

    tracker.configure(true, StatisticsAutostartMode::on, 0);
    tracker.start(t0);
    QVERIFY(tracker.is_tracking());
    QVERIFY(tracker.is_paused());

    tracker.update_position(100, t0 + 1000);
    QCOMPARE(tracker.session_total().reading_time, 0.0);

    tracker.set_context_active(true, t0 + 2000);
    QVERIFY(!tracker.is_paused());
    tracker.update_position(100, t0 + 3000);
    QCOMPARE(tracker.session_total().reading_time, 1.0);
    QCOMPARE(tracker.session_total().characters_read, 0);
}

void ReadingTrackerTest::autostart_page_turn_waits_for_navigation() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);

    tracker.configure(true, StatisticsAutostartMode::page_turn, 0);
    tracker.set_context_active(true, t0);
    tracker.start(t0);
    QVERIFY(!tracker.is_tracking());

    tracker.on_manual_navigation(t0 + 1000);
    QVERIFY(tracker.is_tracking());
    QVERIFY(!tracker.is_paused());
    QCOMPARE(tracker.session().started_at, t0 + 1000);
}

void ReadingTrackerTest::manual_navigation_only_triggers_page_turn_mode() {
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);

    MemoryStore on_store;
    ReadingTracker on_tracker(on_store, utc_zone());
    on_tracker.configure(true, StatisticsAutostartMode::on, 0);
    on_tracker.set_context_active(true, t0);
    on_tracker.start(t0);
    on_tracker.stop(t0 + 1000);
    QVERIFY(!on_tracker.is_tracking());
    on_tracker.on_manual_navigation(t0 + 2000);
    QVERIFY(!on_tracker.is_tracking());

    MemoryStore page_store;
    ReadingTracker page_tracker(page_store, utc_zone());
    page_tracker.configure(true, StatisticsAutostartMode::page_turn, 0);
    page_tracker.set_context_active(true, t0);
    page_tracker.start(t0);
    QVERIFY(!page_tracker.is_tracking());
    page_tracker.on_manual_navigation(t0 + 1000);
    QVERIFY(page_tracker.is_tracking());
    page_tracker.stop(t0 + 2000);
    QVERIFY(!page_tracker.is_tracking());
    page_tracker.on_manual_navigation(t0 + 3000);
    QVERIFY(page_tracker.is_tracking());
}

void ReadingTrackerTest::credits_only_active_time_across_pause() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);

    tracker.update_position(100, t0);
    tracker.update_position(140, t0 + 1000);
    QCOMPARE(tracker.session().characters_read, 40);
    QCOMPARE(tracker.session().reading_time, 1.0);

    tracker.set_context_active(false, t0 + 2000);
    QVERIFY(tracker.is_paused());
    QCOMPARE(tracker.session().reading_time, 2.0);

    tracker.update_position(500, t0 + 12000);
    QCOMPARE(tracker.session().reading_time, 2.0);

    tracker.set_context_active(true, t0 + 13000);
    QVERIFY(!tracker.is_paused());
    tracker.update_position(500, t0 + 14000);
    QCOMPARE(tracker.session().reading_time, 3.0);
    QCOMPARE(tracker.session().characters_read, 40);

    tracker.update_position(530, t0 + 15000);
    QCOMPARE(tracker.session().reading_time, 4.0);
    QCOMPARE(tracker.session().characters_read, 70);
}

void ReadingTrackerTest::flush_credits_time_and_refuses_while_paused() {
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);

    MemoryStore idle_store;
    ReadingTracker idle(idle_store, utc_zone());
    idle.configure(true, StatisticsAutostartMode::on, 0);
    idle.flush(t0);
    QCOMPARE(idle.session_total().reading_time, 0.0);

    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    begin_tracking(tracker, t0);
    tracker.update_position(100, t0);
    tracker.update_position(140, t0 + 1000);

    tracker.flush(t0 + 3000);
    QCOMPARE(tracker.session().reading_time, 3.0);
    QCOMPARE(tracker.session().characters_read, 40);

    tracker.set_context_active(false, t0 + 4000);
    QCOMPARE(tracker.session().reading_time, 4.0);
    tracker.flush(t0 + 9000);
    QCOMPARE(tracker.session().reading_time, 4.0);

    tracker.stop(t0 + 10000);
    QVERIFY(!tracker.is_tracking());
    QCOMPARE(tracker.session().reading_time, 4.0);
}

void ReadingTrackerTest::negative_delta_clamps_at_zero() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);

    tracker.update_position(100, t0);
    tracker.update_position(150, t0 + 1000);
    QCOMPARE(tracker.session().characters_read, 50);

    tracker.update_position(120, t0 + 2000);
    QCOMPARE(tracker.session().characters_read, 20);

    tracker.update_position(0, t0 + 3000);
    QCOMPARE(tracker.session().characters_read, 0);

    tracker.update_position(10, t0 + 4000);
    QCOMPARE(tracker.session().characters_read, 10);
    QCOMPARE(tracker.session().reading_time, 4.0);
}

void ReadingTrackerTest::non_positive_time_delta_keeps_the_baseline() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);

    tracker.update_position(100, t0);
    tracker.update_position(200, t0);
    QCOMPARE(tracker.session().characters_read, 0);
    QCOMPARE(tracker.session().reading_time, 0.0);

    tracker.update_position(200, t0 + 1000);
    QCOMPARE(tracker.session().characters_read, 100);
    QCOMPARE(tracker.session().reading_time, 1.0);

    tracker.update_position(250, t0 + 500);
    QCOMPARE(tracker.session().characters_read, 100);
    QCOMPARE(tracker.session().reading_time, 1.0);

    tracker.update_position(250, t0 + 2000);
    QCOMPARE(tracker.session().characters_read, 150);
    QCOMPARE(tracker.session().reading_time, 2.0);
}

void ReadingTrackerTest::forward_jump_is_credited_and_moves_the_baseline() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);

    tracker.update_position(100, t0);
    tracker.update_position(500, t0 + 1000);
    QCOMPARE(tracker.session().characters_read, 400);

    tracker.update_position(510, t0 + 2000);
    QCOMPARE(tracker.session().characters_read, 410);
    QCOMPARE(tracker.session().reading_time, 2.0);
}

void ReadingTrackerTest::stop_flushes_before_leaving_tracking() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);

    tracker.update_position(100, t0);
    tracker.update_position(140, t0 + 1000);
    QCOMPARE(tracker.session().reading_time, 1.0);

    tracker.stop(t0 + 2000);
    QVERIFY(!tracker.is_tracking());
    QVERIFY(!tracker.is_paused());
    QCOMPARE(tracker.session().reading_time, 2.0);

    tracker.update_position(200, t0 + 3000);
    QCOMPARE(tracker.session().characters_read, 40);
    QCOMPARE(tracker.session().reading_time, 2.0);
}

void ReadingTrackerTest::start_keeps_a_session_with_activity() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);

    tracker.update_position(100, t0);
    tracker.update_position(140, t0 + 1000);
    const std::int64_t started = tracker.session().started_at;
    tracker.stop(t0 + 1000);
    QVERIFY(!tracker.is_tracking());

    tracker.start(t0 + 5000);
    QVERIFY(tracker.is_tracking());
    QCOMPARE(tracker.session().started_at, started);
    QCOMPARE(tracker.session().characters_read, 40);

    tracker.update_position(140, t0 + 6000);
    QCOMPARE(tracker.session().characters_read, 40);
    QCOMPARE(tracker.session().reading_time, 2.0);
}

void ReadingTrackerTest::day_rotation_saves_the_old_session() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t before = ms_at(2026, 1, 1, 23, 59, 50);
    const std::int64_t after = ms_at(2026, 1, 2, 0, 0, 10);
    begin_tracking(tracker, before);

    tracker.update_position(0, before);
    tracker.update_position(100, before + 1000);

    tracker.update_position(200, after);
    QCOMPARE(store.save_count, 1);
    QCOMPARE(store.records.size(), static_cast<std::size_t>(1));
    const TimestampedSession& record = store.records.begin()->second;
    QVERIFY(record.value.has_value());
    QCOMPARE(record.value->characters_read, 100);
    QCOMPARE(record.value->reading_time, 1.0);
    QCOMPARE(record.modified, after);

    QCOMPARE(tracker.session().characters_read, 0);
    QCOMPARE(tracker.session().reading_time, 0.0);
    QCOMPARE(tracker.session().started_at, after);

    tracker.update_position(300, after + 1000);
    QCOMPARE(tracker.session().characters_read, 100);
    QCOMPARE(tracker.session().reading_time, 1.0);
    QCOMPARE(tracker.today_total(after + 1000).characters_read, 100);
    QCOMPARE(tracker.all_time_total().characters_read, 200);
    QCOMPARE(tracker.all_time_total().reading_time, 2.0);
}

void ReadingTrackerTest::day_rotation_honours_reset_minutes() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t before = ms_at(2026, 1, 2, 2, 59, 50);
    const std::int64_t after = ms_at(2026, 1, 2, 3, 0, 10);

    tracker.configure(true, StatisticsAutostartMode::on, 3 * 60);
    tracker.set_context_active(true, before);
    tracker.start(before);
    tracker.update_position(0, before);
    tracker.update_position(100, before + 1000);
    QCOMPARE(store.save_count, 0);

    tracker.update_position(200, after);
    QCOMPARE(store.save_count, 1);
    QCOMPARE(tracker.session().started_at, after);
    QCOMPARE(tracker.session().characters_read, 0);
}

void ReadingTrackerTest::day_rotation_reuses_the_saved_title() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t before = ms_at(2026, 1, 1, 23, 59, 0);
    const std::int64_t after = ms_at(2026, 1, 2, 0, 0, 10);
    begin_tracking(tracker, before);

    tracker.update_position(0, before);
    tracker.update_position(100, before + 1000);
    QVERIFY(tracker.save(before + 2000, "Book Title"));
    QCOMPARE(store.saved_titles.back(), std::string("Book Title"));

    tracker.update_position(150, before + 3000);
    tracker.update_position(200, after);
    QCOMPARE(store.save_count, 2);
    QCOMPARE(store.saved_titles.back(), std::string("Book Title"));
}

void ReadingTrackerTest::save_requires_activity_and_change() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);

    tracker.update_position(100, t0);
    QVERIFY(!tracker.save(t0 + 1000, "Book"));
    QCOMPARE(store.save_count, 0);

    tracker.update_position(140, t0 + 1000);
    QVERIFY(tracker.save(t0 + 2000, "Book"));
    QCOMPARE(store.save_count, 1);
    QCOMPARE(store.records.size(), static_cast<std::size_t>(1));
    const TimestampedSession& first = store.records.begin()->second;
    QVERIFY(first.value.has_value());
    QCOMPARE(first.modified, t0 + 2000);
    QCOMPARE(first.value->characters_read, 40);
    QCOMPARE(first.value->reading_time, 1.0);

    QVERIFY(!tracker.save(t0 + 3000, "Book"));
    QCOMPARE(store.save_count, 1);

    tracker.update_position(150, t0 + 4000);
    QVERIFY(tracker.save(t0 + 1500, "Book"));
    QCOMPARE(store.save_count, 2);
    QCOMPARE(store.records.size(), static_cast<std::size_t>(1));
    const TimestampedSession& second = store.records.begin()->second;
    QVERIFY(second.value.has_value());
    QCOMPARE(second.modified, t0 + 2001);
    QCOMPARE(second.value->characters_read, 50);
    QCOMPARE(second.value->reading_time, 4.0);
}

void ReadingTrackerTest::save_failure_is_retried() {
    MemoryStore store;
    store.fail_saves = true;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);
    tracker.update_position(100, t0);
    tracker.update_position(140, t0 + 1000);

    QVERIFY(!tracker.save(t0 + 2000, "Book"));
    QCOMPARE(store.save_count, 0);
    QCOMPARE(store.records.size(), static_cast<std::size_t>(0));

    store.fail_saves = false;
    QVERIFY(tracker.save(t0 + 3000, "Book"));
    QCOMPARE(store.save_count, 1);
    QCOMPARE(store.records.size(), static_cast<std::size_t>(1));
}

void ReadingTrackerTest::save_adopts_a_newer_external_session() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);
    tracker.update_position(100, t0);
    tracker.update_position(140, t0 + 1000);
    QVERIFY(tracker.save(t0 + 2000, "Book"));

    const std::string id = store.records.begin()->first;
    const std::int64_t saved_modified = store.records.begin()->second.modified;
    const TimestampedSession external = stored_session(saved_modified + 100, t0, 500, 5.0);
    store.records[id] = external;

    QVERIFY(!tracker.save(t0 + 3000, "Book"));
    QCOMPARE(tracker.session().characters_read, 500);
    QCOMPARE(tracker.session().reading_time, 5.0);
    QCOMPARE(store.save_count, 1);
}

void ReadingTrackerTest::save_starts_a_fresh_session_after_a_tombstone() {
    MemoryStore store;
    ReadingTracker tracker(store, utc_zone());
    const std::int64_t t0 = ms_at(2026, 3, 10, 12, 0);
    begin_tracking(tracker, t0);
    tracker.update_position(100, t0);
    tracker.update_position(140, t0 + 1000);
    QVERIFY(tracker.save(t0 + 2000, "Book"));

    const std::string id = store.records.begin()->first;
    store.records[id].modified = store.records[id].modified + 10;
    store.records[id].value = std::nullopt;

    const std::int64_t later = t0 + 9000;
    QVERIFY(!tracker.save(later, "Book"));
    QCOMPARE(tracker.session().characters_read, 0);
    QCOMPARE(tracker.session().reading_time, 0.0);
    QCOMPARE(tracker.session().started_at, later);
    QCOMPARE(store.records.size(), static_cast<std::size_t>(1));
    QVERIFY(!store.records.begin()->second.value.has_value());
    QCOMPARE(store.save_count, 1);
}

void ReadingTrackerTest::totals_add_the_live_session_once() {
    MemoryStore store;
    const std::int64_t today = ms_at(2026, 3, 10, 12, 0);
    store.records["old-today"] = stored_session(100, today - 3600 * 1000, 100, 10.0);
    store.records["old-yesterday"] =
        stored_session(101, ms_at(2026, 3, 9, 12, 0), 50, 5.0);

    ReadingTracker tracker(store, utc_zone());
    tracker.configure(true, StatisticsAutostartMode::on, 0);
    QCOMPARE(tracker.today_total(today).characters_read, 100);
    QCOMPARE(tracker.today_total(today).reading_time, 10.0);
    QCOMPARE(tracker.all_time_total().characters_read, 150);
    QCOMPARE(tracker.all_time_total().reading_time, 15.0);

    tracker.set_context_active(true, today);
    tracker.start(today);
    tracker.update_position(0, today);
    tracker.update_position(100, today + 1000);
    QCOMPARE(tracker.today_total(today + 1000).characters_read, 200);
    QCOMPARE(tracker.all_time_total().characters_read, 250);

    QVERIFY(tracker.save(today + 2000, "Book"));
    QCOMPARE(store.records.size(), static_cast<std::size_t>(3));
    QCOMPARE(tracker.today_total(today + 2000).characters_read, 200);
    QCOMPARE(tracker.all_time_total().characters_read, 250);
    QCOMPARE(tracker.all_time_total().reading_time, 16.0);
}

void ReadingTrackerTest::configure_regroups_for_a_new_reset_minute() {
    MemoryStore store;
    const std::int64_t session_at = ms_at(2026, 1, 2, 4, 0);
    const std::int64_t query_at = ms_at(2026, 1, 2, 2, 0);
    store.records["early"] = stored_session(10, session_at, 100, 10.0);

    ReadingTracker tracker(store, utc_zone());
    tracker.configure(true, StatisticsAutostartMode::on, 0);
    QCOMPARE(tracker.today_total(query_at).characters_read, 100);

    tracker.configure(true, StatisticsAutostartMode::on, 3 * 60);
    QCOMPARE(tracker.today_total(query_at).characters_read, 0);
    QCOMPARE(tracker.all_time_total().characters_read, 100);
}

void ReadingTrackerTest::configure_regroups_after_a_disabled_reset_change() {
    MemoryStore store;
    const std::int64_t session_at = ms_at(2026, 1, 2, 2, 0);
    const std::int64_t query_at = ms_at(2026, 1, 1, 12, 0);
    store.records["early"] = stored_session(10, session_at, 100, 10.0);

    ReadingTracker tracker(store, utc_zone());
    tracker.configure(true, StatisticsAutostartMode::on, 0);
    QCOMPARE(tracker.today_total(query_at).characters_read, 0);

    // The session at 02:00 moves to the previous reporting day at a 03:00
    // reset; a disabled configure must re-key the loaded history at once.
    tracker.configure(false, StatisticsAutostartMode::on, 3 * 60);
    QCOMPARE(tracker.today_total(query_at).characters_read, 100);

    tracker.configure(true, StatisticsAutostartMode::on, 3 * 60);
    QCOMPARE(tracker.today_total(query_at).characters_read, 100);
}

QTEST_APPLESS_MAIN(ReadingTrackerTest)
#include "tst_reading_tracker.moc"
