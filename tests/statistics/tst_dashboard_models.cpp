// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/statistics/dashboard_models.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QTest>
#include <QTime>
#include <QTimeZone>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using niratan::books::BookShelf;
using niratan::statistics::DailyTargetType;
using niratan::statistics::StatisticsBookContribution;
using niratan::statistics::StatisticsBookDayRow;
using niratan::statistics::StatisticsBookRankingMetric;
using niratan::statistics::StatisticsBookRankingRow;
using niratan::statistics::StatisticsBookRecord;
using niratan::statistics::StatisticsBookSnapshotInput;
using niratan::statistics::StatisticsDashboardSnapshot;
using niratan::statistics::StatisticsDateRange;
using niratan::statistics::StatisticsDayAggregate;
using niratan::statistics::StatisticsRangeMode;
using niratan::statistics::StatisticsRangeSummary;
using niratan::statistics::StatisticsShelfComparisonRow;
using niratan::statistics::StatisticsSpeedSummary;
using niratan::statistics::StatisticsTargetSettings;
using niratan::statistics::StatisticsTodaySummary;
using niratan::statistics::StatisticsTrendBookBreakdown;
using niratan::statistics::StatisticsTrendGrain;
using niratan::statistics::StatisticsTrendMetric;
using niratan::statistics::StatisticsTrendPoint;
using niratan::statistics::StatisticsWeekSummary;
using niratan::statistics::book_ranking_rows;
using niratan::statistics::build_snapshot;
using niratan::statistics::calendar_heat_factor;
using niratan::statistics::calendar_heat_max_characters;
using niratan::statistics::daily_goal_streak;
using niratan::statistics::local_day;
using niratan::statistics::monday_start_of_week;
using niratan::statistics::range_summary;
using niratan::statistics::selected_range;
using niratan::statistics::shelf_comparison_rows;
using niratan::statistics::speed_summary;
using niratan::statistics::today_summary;
using niratan::statistics::trend_points;
using niratan::statistics::week_summary;
using niratan::statistics::weekly_goal_streak;
using niratan::statistics::weighted_speed_per_hour;

// Features/Bookshelf/StatisticsDashboardModels.swift defines the snapshot,
// range and calculator rules; StatisticsDashboardView.swift defines the
// heatmap scale. Each test names the rule it holds, including the places where
// the port deliberately differs from macOS: code-point title order instead of
// localizedStandardCompare, a newest-row-wins rule for duplicate date keys,
// and a met-target count that includes a future day of the current week.
namespace {

QDate on(int year, int month, int day_of_month) {
    return QDate(year, month, day_of_month);
}

StatisticsBookDayRow row(std::string key, int characters, double time,
                         std::int64_t modified = 0) {
    StatisticsBookDayRow value;
    value.date_key = std::move(key);
    value.characters = characters;
    value.reading_time = time;
    value.last_modified = modified;
    return value;
}

StatisticsBookContribution contribution(std::string book_id, std::string title, int characters,
                                        double time) {
    StatisticsBookContribution value;
    value.book_id = std::move(book_id);
    value.title = std::move(title);
    value.characters = characters;
    value.reading_time = time;
    return value;
}

StatisticsDayAggregate aggregate(QDate date,
                                 std::vector<StatisticsBookContribution> contributions) {
    StatisticsDayAggregate value;
    value.date = date;
    for (const StatisticsBookContribution& item : contributions) {
        value.characters += item.characters;
        value.reading_time += item.reading_time;
    }
    value.book_contributions = std::move(contributions);
    return value;
}

StatisticsDayAggregate plain_day(QDate date, int characters, double time) {
    StatisticsDayAggregate value;
    value.date = date;
    value.characters = characters;
    value.reading_time = time;
    return value;
}

StatisticsTargetSettings character_target(int target) {
    StatisticsTargetSettings settings;
    settings.daily_target_type = DailyTargetType::characters;
    settings.daily_character_target = target;
    return settings;
}

StatisticsTargetSettings duration_target(int minutes) {
    StatisticsTargetSettings settings;
    settings.daily_target_type = DailyTargetType::duration;
    settings.daily_duration_target_minutes = minutes;
    return settings;
}

void check_optional_int(const std::optional<int>& value, int expected) {
    QVERIFY(value.has_value());
    QCOMPARE(*value, expected);
}

std::vector<std::string> titles(const std::vector<StatisticsTrendBookBreakdown>& rows) {
    std::vector<std::string> result;
    result.reserve(rows.size());
    for (const StatisticsTrendBookBreakdown& row_value : rows) {
        result.push_back(row_value.title);
    }
    return result;
}

std::vector<std::string> ranking_titles(const std::vector<StatisticsBookRankingRow>& rows) {
    std::vector<std::string> result;
    result.reserve(rows.size());
    for (const StatisticsBookRankingRow& row_value : rows) {
        result.push_back(row_value.title);
    }
    return result;
}

std::vector<std::string> ranking_ids(const std::vector<StatisticsBookRankingRow>& rows) {
    std::vector<std::string> result;
    result.reserve(rows.size());
    for (const StatisticsBookRankingRow& row_value : rows) {
        result.push_back(row_value.id);
    }
    return result;
}

std::vector<std::string> shelf_ids(const std::vector<StatisticsShelfComparisonRow>& rows) {
    std::vector<std::string> result;
    result.reserve(rows.size());
    for (const StatisticsShelfComparisonRow& row_value : rows) {
        result.push_back(row_value.id);
    }
    return result;
}

}  // namespace

class DashboardModelsTest : public QObject {
    Q_OBJECT

private slots:
    void snapshot_builds_days_and_books();
    void ranges_window_and_selection();
    void day_aggregate_targets_and_streak();
    void week_summary_days_and_streaks();
    void range_summary_totals_and_single_day_percent();
    void trend_points_by_grain();
    void speed_summary_samples_and_change();
    void book_ranking_metric_and_order();
    void shelf_comparison_groups_books();
    void calendar_heat_scale();
    void local_day_resolves_the_caller_zone();
};

void DashboardModelsTest::snapshot_builds_days_and_books() {
    StatisticsBookSnapshotInput beta;
    beta.id = "b";
    beta.title = "Beta";
    beta.cover_path = std::string("beta.jpg");
    beta.total_characters = 900;
    beta.rows.push_back(row("2026-11-02", 300, 120.0, 1));
    beta.rows.push_back(row("2026-11-01", 200, 60.0, 1));
    beta.rows.push_back(row("2026-13-01", 500, 60.0, 1));

    StatisticsBookSnapshotInput gamma;
    gamma.id = "g";
    gamma.title = "Gamma";
    gamma.rows.push_back(row("2026-11-01", 0, 30.0, 4));
    gamma.rows.push_back(row("2026-11-03", 0, 0.0, 9));

    StatisticsBookSnapshotInput alpha;
    alpha.id = "a";
    alpha.title = "Alpha";
    alpha.total_characters = 500;
    alpha.rows.push_back(row("2026-11-01", 100, 60.0, 2));
    alpha.rows.push_back(row("2026-11-01", 150, 90.0, 5));
    alpha.rows.push_back(row("2026-11-01", 999, 999.0, 3));

    StatisticsBookSnapshotInput twin_z;
    twin_z.id = "z";
    twin_z.title = "Twin";
    StatisticsBookSnapshotInput twin_y;
    twin_y.id = "y";
    twin_y.title = "Twin";

    const StatisticsDashboardSnapshot snapshot =
        build_snapshot({beta, gamma, alpha, twin_z, twin_y});

    // Books sort by title, and a title tie falls to the id.
    QCOMPARE(snapshot.books.size(), 5u);
    QCOMPARE(snapshot.books[0].title, std::string("Alpha"));
    QCOMPARE(snapshot.books[0].total_characters, 500);
    QCOMPARE(snapshot.books[1].title, std::string("Beta"));
    QCOMPARE(snapshot.books[2].title, std::string("Gamma"));
    QCOMPARE(snapshot.books[3].id, std::string("y"));
    QCOMPARE(snapshot.books[4].id, std::string("z"));
    QVERIFY(snapshot.books[1].cover_path.has_value());
    QCOMPARE(snapshot.books[1].cover_path.value(), std::string("beta.jpg"));

    // Days ascend by date; the unparsable key and the idle row are dropped.
    QCOMPARE(snapshot.days.size(), 2u);
    QCOMPARE(snapshot.days[0].date, on(2026, 11, 1));
    QCOMPARE(snapshot.days[0].characters, 350);
    QCOMPARE(snapshot.days[0].reading_time, 180.0);
    QCOMPARE(snapshot.days[0].active_book_count(), 3);
    QCOMPARE(snapshot.days[0].book_contributions.size(), 3u);
    QCOMPARE(snapshot.days[0].book_contributions[0].title, std::string("Alpha"));
    QCOMPARE(snapshot.days[0].book_contributions[0].characters, 150);
    QCOMPARE(snapshot.days[0].book_contributions[0].reading_time, 90.0);
    QCOMPARE(snapshot.days[0].book_contributions[1].title, std::string("Beta"));
    QCOMPARE(snapshot.days[0].book_contributions[1].characters, 200);
    QCOMPARE(snapshot.days[0].book_contributions[2].title, std::string("Gamma"));
    QCOMPARE(snapshot.days[0].book_contributions[2].characters, 0);
    QCOMPARE(snapshot.days[0].book_contributions[2].reading_time, 30.0);

    QCOMPARE(snapshot.days[1].date, on(2026, 11, 2));
    QCOMPARE(snapshot.days[1].characters, 300);
    QCOMPARE(snapshot.days[1].reading_time, 120.0);
    QCOMPARE(snapshot.days[1].book_contributions.size(), 1u);
    QVERIFY(snapshot.days[1].book_contributions[0].cover_path.has_value());
    QCOMPARE(snapshot.days[1].book_contributions[0].cover_path.value(),
             std::string("beta.jpg"));

    const StatisticsDashboardSnapshot empty = build_snapshot({});
    QVERIFY(empty.days.empty());
    QVERIFY(empty.books.empty());
}

void DashboardModelsTest::ranges_window_and_selection() {
    const StatisticsDateRange window = StatisticsDateRange::recent_year(on(2026, 11, 20));
    QCOMPARE(window.start, on(2025, 11, 21));
    QCOMPARE(window.end, on(2026, 11, 20));
    QCOMPARE(window.day_count(), 365);
    QVERIFY(window.contains(on(2025, 11, 21)));
    QVERIFY(window.contains(on(2026, 11, 20)));
    QVERIFY(!window.contains(on(2025, 11, 20)));
    QVERIFY(!window.contains(on(2026, 11, 21)));

    // A leap day maps to February 28 before the day is added.
    const StatisticsDateRange leap = StatisticsDateRange::recent_year(on(2024, 2, 29));
    QCOMPARE(leap.start, on(2023, 3, 1));
    QCOMPARE(leap.end, on(2024, 2, 29));
    QCOMPARE(leap.day_count(), 366);

    QCOMPARE(monday_start_of_week(on(2026, 11, 18)), on(2026, 11, 16));
    QCOMPARE(monday_start_of_week(on(2026, 11, 16)), on(2026, 11, 16));
    QCOMPARE(StatisticsDateRange::monday_week(on(2026, 11, 18)).end, on(2026, 11, 22));

    QCOMPARE(window.coerce(on(2020, 1, 1)), window.start);
    QCOMPARE(window.coerce(on(2030, 1, 1)), window.end);
    QCOMPARE(window.coerce(on(2026, 5, 4)), on(2026, 5, 4));

    const StatisticsDateRange year =
        selected_range(StatisticsRangeMode::year, on(2026, 5, 4), window);
    QCOMPARE(year, window);

    const StatisticsDateRange month =
        selected_range(StatisticsRangeMode::month, on(2026, 11, 20), window);
    QCOMPARE(month.start, on(2026, 11, 1));
    QCOMPARE(month.end, on(2026, 11, 20));
    QCOMPARE(month.day_count(), 20);

    // The anchor is coerced into the window before its month is taken.
    const StatisticsDateRange start_month =
        selected_range(StatisticsRangeMode::month, on(2020, 1, 1), window);
    QCOMPARE(start_month.start, on(2025, 11, 21));
    QCOMPARE(start_month.end, on(2025, 11, 30));

    const StatisticsDateRange week =
        selected_range(StatisticsRangeMode::week, on(2026, 11, 18), window);
    QCOMPARE(week.start, on(2026, 11, 16));
    QCOMPARE(week.end, on(2026, 11, 20));

    const StatisticsDateRange clipped_week =
        selected_range(StatisticsRangeMode::week, on(2025, 11, 21), window);
    QCOMPARE(clipped_week.start, on(2025, 11, 21));
    QCOMPARE(clipped_week.end, on(2025, 11, 23));

    const StatisticsDateRange single =
        selected_range(StatisticsRangeMode::day, on(2020, 1, 1), window);
    QCOMPARE(single.start, on(2025, 11, 21));
    QCOMPARE(single.end, on(2025, 11, 21));
    QCOMPARE(single.day_count(), 1);
}

void DashboardModelsTest::day_aggregate_targets_and_streak() {
    std::vector<StatisticsBookContribution> contributions;
    contributions.push_back(contribution("a", "A", 0, 90.0));
    contributions.push_back(contribution("b", "B", 3000, 0.0));
    const StatisticsDayAggregate day = aggregate(on(2026, 11, 20), contributions);

    QCOMPARE(day.active_book_count(), 2);
    QCOMPARE(day.characters, 3000);
    QCOMPARE(day.reading_time, 90.0);

    const StatisticsTargetSettings characters = character_target(5000);
    QVERIFY(qFuzzyCompare(day.target_ratio(characters), 0.6));
    const std::vector<StatisticsDayAggregate> today_days = {day};
    QCOMPARE(today_summary(today_days, on(2026, 11, 20), characters).target_percent, 60);

    const StatisticsTargetSettings duration = duration_target(30);
    QVERIFY(qFuzzyCompare(day.target_ratio(duration), 0.05));
    QCOMPARE(today_summary(today_days, on(2026, 11, 20), duration).target_percent, 5);

    // The percent rounds half away from zero.
    const std::vector<StatisticsDayAggregate> one_character = {
        plain_day(on(2026, 11, 20), 1, 0.0)};
    const std::vector<StatisticsDayAggregate> two_characters = {
        plain_day(on(2026, 11, 20), 2, 0.0)};
    QCOMPARE(today_summary(one_character, on(2026, 11, 20), character_target(3)).target_percent,
             33);
    QCOMPARE(today_summary(two_characters, on(2026, 11, 20), character_target(3)).target_percent,
             67);

    // A non-positive target has no ratio.
    const StatisticsTargetSettings no_target = character_target(0);
    QCOMPARE(day.target_ratio(no_target), 0.0);
    QCOMPARE(today_summary(today_days, on(2026, 11, 20), no_target).target_percent, 0);

    std::vector<StatisticsDayAggregate> days;
    days.push_back(plain_day(on(2026, 11, 17), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 18), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 19), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 20), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 21), 500, 300.0));

    // Today is below the target, so the streak starts at yesterday.
    const StatisticsTargetSettings goal = character_target(1000);
    QCOMPARE(daily_goal_streak(days, on(2026, 11, 21), goal), 4);

    // A met today extends the streak, but yesterday below the target ends it.
    days.push_back(plain_day(on(2026, 11, 22), 1000, 600.0));
    QCOMPARE(daily_goal_streak(days, on(2026, 11, 22), goal), 1);

    std::vector<StatisticsDayAggregate> contiguous;
    contiguous.push_back(plain_day(on(2026, 11, 18), 1000, 600.0));
    contiguous.push_back(plain_day(on(2026, 11, 19), 1000, 600.0));
    contiguous.push_back(plain_day(on(2026, 11, 20), 1000, 600.0));
    contiguous.push_back(plain_day(on(2026, 11, 21), 1000, 600.0));
    QCOMPARE(daily_goal_streak(contiguous, on(2026, 11, 21), goal), 4);

    // A missing day ends the streak.
    std::vector<StatisticsDayAggregate> gapped;
    gapped.push_back(plain_day(on(2026, 11, 18), 1000, 600.0));
    gapped.push_back(plain_day(on(2026, 11, 19), 1000, 600.0));
    gapped.push_back(plain_day(on(2026, 11, 21), 500, 300.0));
    QCOMPARE(daily_goal_streak(gapped, on(2026, 11, 21), goal), 0);

    // Today's speed ignores a sample under one minute, and its streak is the
    // daily one.
    std::vector<StatisticsDayAggregate> today_only;
    today_only.push_back(plain_day(on(2026, 11, 21), 100, 30.0));
    const StatisticsTodaySummary short_day =
        today_summary(today_only, on(2026, 11, 21), characters);
    QCOMPARE(short_day.date, on(2026, 11, 21));
    QCOMPARE(short_day.characters, 100);
    QCOMPARE(short_day.reading_time, 30.0);
    QVERIFY(!short_day.average_speed_per_hour.has_value());
    QCOMPARE(short_day.daily_streak_days, 0);

    const StatisticsTodaySummary missing = today_summary({}, on(2026, 11, 21), characters);
    QCOMPARE(missing.characters, 0);
    QCOMPARE(missing.target_percent, 0);
    QVERIFY(!missing.average_speed_per_hour.has_value());
}

void DashboardModelsTest::week_summary_days_and_streaks() {
    StatisticsTargetSettings settings = character_target(1000);
    settings.weekly_target_days = 2;

    std::vector<StatisticsDayAggregate> days;
    days.push_back(plain_day(on(2026, 11, 9), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 10), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 15), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 16), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 17), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 18), 500, 300.0));
    days.push_back(plain_day(on(2026, 11, 19), 1000, 600.0));

    const StatisticsWeekSummary week =
        week_summary(days, on(2026, 11, 18), settings);

    QCOMPARE(week.range.start, on(2026, 11, 16));
    QCOMPARE(week.range.end, on(2026, 11, 22));
    QCOMPARE(week.elapsed_days, 3);
    QCOMPARE(week.characters, 3500);
    QCOMPARE(week.reading_time, 2100.0);
    check_optional_int(week.average_speed_per_hour, 6000);
    QCOMPARE(week.target_days, 2);
    // 11-16, 11-17 and the future 11-19 meet the target.
    QCOMPARE(week.met_target_days, 3);
    QCOMPARE(week.daily_streak_days, 3);
    QCOMPARE(week.weekly_streak_weeks, 2);
    QCOMPARE(week.average_characters_per_elapsed_day, 1167);
    QVERIFY(qFuzzyCompare(week.average_reading_time_per_elapsed_day, 700.0));

    QCOMPARE(week.days.size(), 7u);
    QCOMPARE(week.days[0].date, on(2026, 11, 16));
    QCOMPARE(week.days[0].is_today, false);
    QCOMPARE(week.days[0].is_future, false);
    check_optional_int(week.days[0].percent, 100);
    QCOMPARE(week.days[0].met_target, true);

    QCOMPARE(week.days[2].date, on(2026, 11, 18));
    QCOMPARE(week.days[2].is_today, true);
    QCOMPARE(week.days[2].is_future, false);
    check_optional_int(week.days[2].percent, 50);
    QCOMPARE(week.days[2].met_target, false);

    QCOMPARE(week.days[3].date, on(2026, 11, 19));
    QCOMPARE(week.days[3].is_future, true);
    QVERIFY(!week.days[3].percent.has_value());
    QCOMPARE(week.days[3].met_target, false);

    QCOMPARE(week.days[6].date, on(2026, 11, 22));
    QCOMPARE(week.days[6].is_future, true);
    QVERIFY(!week.days[6].percent.has_value());

    // Elapsed days are clamped to 1..7.
    const std::vector<StatisticsDayAggregate> no_days;
    QCOMPARE(week_summary(no_days, on(2026, 11, 16), settings).elapsed_days, 1);
    QCOMPARE(week_summary(no_days, on(2026, 11, 22), settings).elapsed_days, 7);

    const StatisticsWeekSummary blank = week_summary(no_days, on(2026, 11, 18), settings);
    QCOMPARE(blank.characters, 0);
    QCOMPARE(blank.days.size(), 7u);
    QVERIFY(!blank.average_speed_per_hour.has_value());
    QVERIFY(!blank.days[0].percent.has_value());
    QCOMPARE(blank.met_target_days, 0);
    QCOMPARE(blank.weekly_streak_weeks, 0);

    // A weekly target below 1 never builds a streak.
    StatisticsTargetSettings open = settings;
    open.weekly_target_days = 0;
    QCOMPARE(weekly_goal_streak(days, on(2026, 11, 18), open), 0);
}

void DashboardModelsTest::range_summary_totals_and_single_day_percent() {
    std::vector<StatisticsDayAggregate> days;
    days.push_back(plain_day(on(2026, 10, 31), 5000, 600.0));
    days.push_back(plain_day(on(2026, 11, 1), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 3), 2000, 1200.0));

    const StatisticsTargetSettings settings = character_target(1000);
    const StatisticsDateRange range{on(2026, 11, 1), on(2026, 11, 3)};
    const StatisticsRangeSummary summary = range_summary(days, range, settings);

    QCOMPARE(summary.characters, 3000);
    QCOMPARE(summary.reading_time, 1800.0);
    check_optional_int(summary.average_speed_per_hour, 6000);
    QCOMPARE(summary.target_days, 2);
    QCOMPARE(summary.target_progress_percent, 0);

    const StatisticsDateRange single{on(2026, 11, 3), on(2026, 11, 3)};
    const StatisticsRangeSummary single_summary = range_summary(days, single, settings);
    QCOMPARE(single_summary.characters, 2000);
    QCOMPARE(single_summary.target_days, 1);
    QCOMPARE(single_summary.target_progress_percent, 200);

    const StatisticsDateRange missing{on(2026, 11, 4), on(2026, 11, 4)};
    const StatisticsRangeSummary missing_summary = range_summary(days, missing, settings);
    QCOMPARE(missing_summary.characters, 0);
    QCOMPARE(missing_summary.target_days, 0);
    QCOMPARE(missing_summary.target_progress_percent, 0);
    QVERIFY(!missing_summary.average_speed_per_hour.has_value());
}

void DashboardModelsTest::trend_points_by_grain() {
    std::vector<StatisticsBookContribution> first_day;
    first_day.push_back(contribution("a", "A", 100, 60.0));
    first_day.push_back(contribution("b", "B", 0, 60.0));

    std::vector<StatisticsBookContribution> middle_day;
    middle_day.push_back(contribution("c", "C", 200, 120.0));
    middle_day.push_back(contribution("d", "D", 100, 60.0));

    std::vector<StatisticsBookContribution> wide_day;
    wide_day.push_back(contribution("t1", "T1", 60, 120.0));
    wide_day.push_back(contribution("t2", "T2", 50, 120.0));
    wide_day.push_back(contribution("t3", "T3", 40, 120.0));
    wide_day.push_back(contribution("t4", "T4", 30, 120.0));
    wide_day.push_back(contribution("t5", "T5", 20, 120.0));
    wide_day.push_back(contribution("t6", "T6", 10, 120.0));
    wide_day.push_back(contribution("tz", "Z", 0, 120.0));

    std::vector<StatisticsDayAggregate> days;
    days.push_back(plain_day(on(2026, 10, 31), 500, 300.0));
    days.push_back(aggregate(on(2026, 11, 1), first_day));
    days.push_back(aggregate(on(2026, 11, 3), middle_day));
    days.push_back(aggregate(on(2026, 11, 5), wide_day));
    days.push_back(plain_day(on(2026, 11, 10), 50, 30.0));

    const StatisticsDateRange range{on(2026, 11, 1), on(2026, 11, 10)};
    const std::vector<StatisticsTrendPoint> daily =
        trend_points(StatisticsTrendGrain::day, range, days);
    QCOMPARE(daily.size(), 10u);
    QCOMPARE(daily[0].id, std::string("2026-11-01"));
    QCOMPARE(daily[0].label, std::string("11/1"));
    QCOMPARE(daily[0].characters, 100);
    QCOMPARE(daily[0].reading_time, 120.0);
    check_optional_int(daily[0].average_speed_per_hour, 6000);
    // B read no characters, so it is not part of the breakdown.
    const std::vector<std::string> first_titles = {"A"};
    QCOMPARE(titles(daily[0].book_breakdown), first_titles);

    QCOMPARE(daily[1].id, std::string("2026-11-02"));
    QCOMPARE(daily[1].label, std::string("11/2"));
    QCOMPARE(daily[1].characters, 0);
    QVERIFY(!daily[1].average_speed_per_hour.has_value());
    QVERIFY(daily[1].book_breakdown.empty());

    QCOMPARE(daily[4].id, std::string("2026-11-05"));
    QCOMPARE(daily[4].characters, 210);
    check_optional_int(daily[4].average_speed_per_hour, 1050);
    QCOMPARE(daily[4].book_breakdown.size(), 5u);
    const std::vector<std::string> wide_titles = {"T1", "T2", "T3", "T4", "T5"};
    QCOMPARE(titles(daily[4].book_breakdown), wide_titles);

    QCOMPARE(daily[9].id, std::string("2026-11-10"));
    QCOMPARE(daily[9].characters, 50);
    QVERIFY(!daily[9].average_speed_per_hour.has_value());

    const std::optional<double> characters = daily[0].value(StatisticsTrendMetric::characters);
    QVERIFY(characters.has_value());
    QCOMPARE(*characters, 100.0);
    const std::optional<double> duration = daily[0].value(StatisticsTrendMetric::duration);
    QVERIFY(duration.has_value());
    QCOMPARE(*duration, 120.0);
    const std::optional<double> speed = daily[0].value(StatisticsTrendMetric::speed);
    QVERIFY(speed.has_value());
    QCOMPARE(*speed, 6000.0);
    QVERIFY(!daily[1].value(StatisticsTrendMetric::speed).has_value());

    const std::vector<StatisticsTrendPoint> weekly =
        trend_points(StatisticsTrendGrain::week, range, days);
    QCOMPARE(weekly.size(), 3u);
    QCOMPARE(weekly[0].id, std::string("2026-W44"));
    QCOMPARE(weekly[0].label, std::string("2026-W44"));
    QCOMPARE(weekly[0].characters, 100);
    QCOMPARE(weekly[1].id, std::string("2026-W45"));
    QCOMPARE(weekly[1].characters, 510);
    QCOMPARE(weekly[1].reading_time, 1020.0);
    check_optional_int(weekly[1].average_speed_per_hour, 2040);
    const std::vector<std::string> week_titles = {"C", "D", "T1", "T2", "T3"};
    QCOMPARE(titles(weekly[1].book_breakdown), week_titles);
    QCOMPARE(weekly[2].id, std::string("2026-W46"));
    QCOMPARE(weekly[2].characters, 50);

    const StatisticsDateRange months{on(2026, 10, 15), on(2026, 11, 10)};
    const std::vector<StatisticsTrendPoint> monthly =
        trend_points(StatisticsTrendGrain::month, months, days);
    QCOMPARE(monthly.size(), 2u);
    QCOMPARE(monthly[0].id, std::string("2026-10"));
    QCOMPARE(monthly[0].characters, 500);
    QCOMPARE(monthly[1].id, std::string("2026-11"));
    QCOMPARE(monthly[1].characters, 660);

    const StatisticsDateRange empty_range{on(2026, 12, 1), on(2026, 12, 31)};
    QVERIFY(trend_points(StatisticsTrendGrain::day, empty_range, days).empty());
}

void DashboardModelsTest::speed_summary_samples_and_change() {
    std::vector<StatisticsDayAggregate> days;
    days.push_back(plain_day(on(2026, 10, 31), 1000, 600.0));
    days.push_back(plain_day(on(2026, 11, 1), 300, 600.0));
    days.push_back(plain_day(on(2026, 11, 2), 600, 600.0));
    days.push_back(plain_day(on(2026, 11, 3), 600, 600.0));
    days.push_back(plain_day(on(2026, 11, 4), 600, 600.0));
    days.push_back(plain_day(on(2026, 11, 5), 600, 600.0));
    days.push_back(plain_day(on(2026, 11, 6), 600, 600.0));
    days.push_back(plain_day(on(2026, 11, 7), 600, 600.0));
    days.push_back(plain_day(on(2026, 11, 8), 600, 600.0));
    days.push_back(plain_day(on(2026, 11, 9), 100, 30.0));

    std::vector<StatisticsBookContribution> mixed;
    mixed.push_back(contribution("x", "X", 1000, 600.0));
    mixed.push_back(contribution("y", "Y", 500, 30.0));
    days.push_back(aggregate(on(2026, 11, 10), mixed));

    const StatisticsDateRange range{on(2026, 11, 1), on(2026, 11, 30)};
    const StatisticsSpeedSummary summary = speed_summary(days, range);

    // The day outside the range, the short day and the short contribution
    // are not samples.
    check_optional_int(summary.weighted_average_speed_per_hour, 3667);
    check_optional_int(summary.median_day_speed_per_hour, 3600);
    check_optional_int(summary.recent_active_day_speed_per_hour, 3943);
    QVERIFY(!summary.speed_change_percent.has_value());
    QVERIFY(summary.best_day.has_value());
    QCOMPARE(summary.best_day->date, on(2026, 11, 10));
    QCOMPARE(summary.best_day->speed_per_hour, 6000);
    QVERIFY(summary.worst_day.has_value());
    QCOMPARE(summary.worst_day->date, on(2026, 11, 1));
    QCOMPARE(summary.worst_day->speed_per_hour, 1800);

    // An even number of day speeds averages the two middle values.
    std::vector<StatisticsDayAggregate> pair;
    pair.push_back(plain_day(on(2026, 11, 1), 1000, 3600.0));
    pair.push_back(plain_day(on(2026, 11, 2), 2001, 3600.0));
    const StatisticsSpeedSummary pair_summary =
        speed_summary(pair, StatisticsDateRange{on(2026, 11, 1), on(2026, 11, 30)});
    check_optional_int(pair_summary.median_day_speed_per_hour, 1501);

    // Equal speeds keep the earlier date at both ends.
    std::vector<StatisticsDayAggregate> equal;
    equal.push_back(plain_day(on(2026, 11, 2), 600, 600.0));
    equal.push_back(plain_day(on(2026, 11, 1), 600, 600.0));
    const StatisticsSpeedSummary equal_summary =
        speed_summary(equal, StatisticsDateRange{on(2026, 11, 1), on(2026, 11, 30)});
    QVERIFY(equal_summary.best_day.has_value());
    QVERIFY(equal_summary.worst_day.has_value());
    QCOMPARE(equal_summary.best_day->date, on(2026, 11, 1));
    QCOMPARE(equal_summary.worst_day->date, on(2026, 11, 1));

    // 28 active days compare the first fourteen with the last fourteen.
    std::vector<StatisticsDayAggregate> monthly;
    for (int day_number = 1; day_number <= 28; ++day_number) {
        const int characters = day_number <= 14 ? 600 : 1200;
        monthly.push_back(plain_day(on(2026, 11, day_number), characters, 600.0));
    }
    const StatisticsSpeedSummary change =
        speed_summary(monthly, StatisticsDateRange{on(2026, 11, 1), on(2026, 11, 30)});
    check_optional_int(change.speed_change_percent, 100);
    check_optional_int(change.weighted_average_speed_per_hour, 5400);
    check_optional_int(change.recent_active_day_speed_per_hour, 7200);
    check_optional_int(change.median_day_speed_per_hour, 5400);
    QVERIFY(change.best_day.has_value());
    QVERIFY(change.worst_day.has_value());
    QCOMPARE(change.best_day->date, on(2026, 11, 15));
    QCOMPARE(change.worst_day->date, on(2026, 11, 1));

    // One day fewer is not enough for a change.
    monthly.pop_back();
    const StatisticsSpeedSummary short_month =
        speed_summary(monthly, StatisticsDateRange{on(2026, 11, 1), on(2026, 11, 30)});
    QVERIFY(!short_month.speed_change_percent.has_value());

    // A zero early average yields no change either.
    std::vector<StatisticsDayAggregate> slow_start;
    for (int day_number = 1; day_number <= 28; ++day_number) {
        if (day_number <= 14) {
            slow_start.push_back(plain_day(on(2026, 11, day_number), 1, 1e9));
        } else {
            slow_start.push_back(plain_day(on(2026, 11, day_number), 600, 600.0));
        }
    }
    const StatisticsSpeedSummary slow =
        speed_summary(slow_start, StatisticsDateRange{on(2026, 11, 1), on(2026, 11, 30)});
    QVERIFY(!slow.speed_change_percent.has_value());
}

void DashboardModelsTest::book_ranking_metric_and_order() {
    std::vector<StatisticsBookContribution> first;
    first.push_back(contribution("b1", "Beta", 100, 300.0));
    first.push_back(contribution("a1", "Alpha", 100, 300.0));
    first.push_back(contribution("c1", "Gamma", 50, 600.0));
    first.push_back(contribution("d1", "Delta", 0, 600.0));
    first.push_back(contribution("g1", "Alpha Two", 100, 300.0));

    std::vector<StatisticsBookContribution> second;
    second.push_back(contribution("b1", "Beta", 200, 300.0));
    second.push_back(contribution("c1", "Gamma", 0, 600.0));
    second.push_back(contribution("e1", "Epsilon", 30, 10.0));

    std::vector<StatisticsDayAggregate> days;
    days.push_back(plain_day(on(2026, 10, 31), 999, 60.0));
    days.push_back(aggregate(on(2026, 11, 1), first));
    days.push_back(aggregate(on(2026, 11, 2), second));

    const StatisticsDateRange range{on(2026, 11, 1), on(2026, 11, 2)};

    const std::vector<StatisticsBookRankingRow> by_characters =
        book_ranking_rows(days, range, StatisticsBookRankingMetric::characters);
    QCOMPARE(by_characters.size(), 5u);
    const std::vector<std::string> character_ids = {"b1", "a1", "g1", "c1", "e1"};
    QCOMPARE(ranking_ids(by_characters), character_ids);
    const std::vector<std::string> character_titles = {"Beta", "Alpha", "Alpha Two", "Gamma",
                                                       "Epsilon"};
    QCOMPARE(ranking_titles(by_characters), character_titles);
    QCOMPARE(by_characters[0].characters, 300);
    QCOMPARE(by_characters[0].reading_time, 600.0);
    check_optional_int(by_characters[0].average_speed_per_hour, 1800);
    check_optional_int(by_characters[3].average_speed_per_hour, 300);
    QVERIFY(!by_characters[4].average_speed_per_hour.has_value());

    const std::vector<StatisticsBookRankingRow> by_duration =
        book_ranking_rows(days, range, StatisticsBookRankingMetric::duration);
    QCOMPARE(by_duration.size(), 6u);
    const std::vector<std::string> duration_titles = {"Gamma", "Beta",  "Delta",
                                                      "Alpha", "Alpha Two", "Epsilon"};
    QCOMPARE(ranking_titles(by_duration), duration_titles);

    const std::vector<StatisticsBookRankingRow> by_speed =
        book_ranking_rows(days, range, StatisticsBookRankingMetric::speed);
    QCOMPARE(by_speed.size(), 4u);
    const std::vector<std::string> speed_titles = {"Beta", "Alpha", "Alpha Two", "Gamma"};
    QCOMPARE(ranking_titles(by_speed), speed_titles);

    // An equal title and metric fall to the book id.
    std::vector<StatisticsBookContribution> tied_contributions;
    tied_contributions.push_back(contribution("z", "Same", 40, 300.0));
    tied_contributions.push_back(contribution("a", "Same", 40, 300.0));
    std::vector<StatisticsDayAggregate> tied_days;
    tied_days.push_back(aggregate(on(2026, 11, 1), tied_contributions));
    const std::vector<StatisticsBookRankingRow> tied =
        book_ranking_rows(tied_days, range, StatisticsBookRankingMetric::characters);
    const std::vector<std::string> tied_ids = {"a", "z"};
    QCOMPARE(ranking_ids(tied), tied_ids);
}

void DashboardModelsTest::shelf_comparison_groups_books() {
    std::vector<StatisticsBookRecord> books;
    StatisticsBookRecord a;
    a.id = "a";
    a.title = "A";
    a.total_characters = 1000;
    StatisticsBookRecord b;
    b.id = "b";
    b.title = "B";
    b.total_characters = 2000;
    StatisticsBookRecord c;
    c.id = "c";
    c.title = "C";
    c.total_characters = 3000;
    StatisticsBookRecord d;
    d.id = "d";
    d.title = "D";
    d.total_characters = 4000;
    books.push_back(a);
    books.push_back(b);
    books.push_back(c);
    books.push_back(d);

    std::vector<BookShelf> shelves;
    BookShelf fiction;
    fiction.name = "Fiction";
    fiction.book_ids = {"a", "b", "x"};
    BookShelf empty;
    empty.name = "Empty";
    empty.book_ids = {"x"};
    BookShelf shared;
    shared.name = "Shared";
    shared.book_ids = {"b", "c"};
    shelves.push_back(fiction);
    shelves.push_back(empty);
    shelves.push_back(shared);

    std::vector<StatisticsBookContribution> contributions;
    contributions.push_back(contribution("a", "A", 100, 300.0));
    contributions.push_back(contribution("b", "B", 200, 300.0));
    contributions.push_back(contribution("c", "C", 0, 120.0));

    std::vector<StatisticsDayAggregate> days;
    days.push_back(aggregate(on(2026, 10, 31), {contribution("d", "D", 500, 600.0)}));
    days.push_back(aggregate(on(2026, 11, 1), contributions));

    const StatisticsDateRange range{on(2026, 11, 1), on(2026, 11, 1)};
    const std::vector<StatisticsShelfComparisonRow> rows =
        shelf_comparison_rows(books, shelves, days, range, "Unshelved");

    QCOMPARE(rows.size(), 3u);
    const std::vector<std::string> row_ids = {"shelf:Fiction", "shelf:Shared", "unshelved"};
    QCOMPARE(shelf_ids(rows), row_ids);
    QCOMPARE(rows[0].name, std::string("Fiction"));
    QCOMPARE(rows[0].book_count, 2);
    QCOMPARE(rows[0].total_characters, 3000);
    QCOMPARE(rows[0].recorded_characters, 300);
    QCOMPARE(rows[0].reading_time, 600.0);
    check_optional_int(rows[0].average_speed_per_hour, 1800);

    QCOMPARE(rows[1].name, std::string("Shared"));
    QCOMPARE(rows[1].book_count, 2);
    QCOMPARE(rows[1].total_characters, 5000);
    QCOMPARE(rows[1].recorded_characters, 200);
    QCOMPARE(rows[1].reading_time, 420.0);
    check_optional_int(rows[1].average_speed_per_hour, 2400);

    QCOMPARE(rows[2].name, std::string("Unshelved"));
    QCOMPARE(rows[2].book_count, 1);
    QCOMPARE(rows[2].total_characters, 4000);
    QCOMPARE(rows[2].recorded_characters, 0);
    QCOMPARE(rows[2].reading_time, 0.0);
    QVERIFY(!rows[2].average_speed_per_hour.has_value());

    // Every book on a shelf leaves no unshelved row.
    std::vector<BookShelf> covering;
    BookShelf all;
    all.name = "All";
    all.book_ids = {"a", "b", "c", "d"};
    covering.push_back(all);
    QCOMPARE(shelf_comparison_rows(books, covering, days, range, "Unshelved").size(), 1u);

    // Equal recorded characters sort by name.
    std::vector<StatisticsBookRecord> pair_books;
    pair_books.push_back(a);
    pair_books.push_back(c);
    std::vector<BookShelf> pair_shelves;
    BookShelf zeta;
    zeta.name = "Zeta";
    zeta.book_ids = {"a"};
    BookShelf beta_shelf;
    beta_shelf.name = "Beta";
    beta_shelf.book_ids = {"c"};
    pair_shelves.push_back(zeta);
    pair_shelves.push_back(beta_shelf);

    std::vector<StatisticsDayAggregate> pair_days;
    std::vector<StatisticsBookContribution> pair_contributions;
    pair_contributions.push_back(contribution("a", "A", 100, 60.0));
    pair_contributions.push_back(contribution("c", "C", 100, 60.0));
    pair_days.push_back(aggregate(on(2026, 11, 1), pair_contributions));

    const std::vector<StatisticsShelfComparisonRow> tied =
        shelf_comparison_rows(pair_books, pair_shelves, pair_days, range, "Unshelved");
    const std::vector<std::string> tied_ids = {"shelf:Beta", "shelf:Zeta"};
    QCOMPARE(shelf_ids(tied), tied_ids);
}

void DashboardModelsTest::calendar_heat_scale() {
    std::vector<StatisticsDayAggregate> days;
    days.push_back(plain_day(on(2026, 11, 1), 100, 0.0));
    days.push_back(plain_day(on(2026, 11, 2), 400, 0.0));
    days.push_back(plain_day(on(2026, 11, 3), 0, 0.0));

    QCOMPARE(calendar_heat_max_characters(days), 400);
    QVERIFY(qFuzzyCompare(calendar_heat_factor(400, 400), 0.9));
    QVERIFY(qFuzzyCompare(calendar_heat_factor(100, 400), 0.36));
    QVERIFY(qFuzzyCompare(calendar_heat_factor(10, 400), 0.3096));
    QCOMPARE(calendar_heat_factor(0, 400), 0.0);

    const std::vector<StatisticsDayAggregate> no_days;
    const std::vector<StatisticsDayAggregate> quiet = {plain_day(on(2026, 11, 1), 0, 0.0)};
    QCOMPARE(calendar_heat_max_characters(no_days), 1);
    QCOMPARE(calendar_heat_max_characters(quiet), 1);
    QCOMPARE(calendar_heat_factor(100, 0), 0.0);
}

// Features/Bookshelf/StatisticsDashboardModels.swift takes `today` and the
// user's calendar as arguments; the port resolves an instant once, in the
// called zone, and every other rule works on calendar days.
void DashboardModelsTest::local_day_resolves_the_caller_zone() {
    const QTimeZone utc = QTimeZone::UTC;
    const QDateTime late = QDateTime(on(2026, 11, 20), QTime(23, 30), utc);
    QCOMPARE(local_day(late, 0, utc), on(2026, 11, 20));

    const QTimeZone tokyo(QByteArray("Asia/Tokyo"));
    const QDateTime crossing = QDateTime(on(2026, 11, 20), QTime(23, 30), utc);
    QCOMPARE(local_day(crossing, 0, tokyo), on(2026, 11, 21));
    // A reset time moves the reporting day back.
    QCOMPARE(local_day(crossing, 4 * 60, tokyo), on(2026, 11, 21));
    QCOMPARE(local_day(QDateTime(on(2026, 11, 21), QTime(3, 0), tokyo), 4 * 60, tokyo),
             on(2026, 11, 20));
}

QTEST_APPLESS_MAIN(DashboardModelsTest)

#include "tst_dashboard_models.moc"
