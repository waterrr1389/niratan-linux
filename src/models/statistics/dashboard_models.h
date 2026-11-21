// SPDX-License-Identifier: GPL-3.0-or-later
//
// The statistics dashboard aggregation, following
// Features/Bookshelf/StatisticsDashboardModels.swift (StatisticsRangeMode,
// StatisticsTrendMetric, StatisticsTrendGrain, StatisticsBookRankingMetric,
// StatisticsDateRange, StatisticsBookContribution, StatisticsDayAggregate,
// StatisticsDashboardSnapshot, StatisticsBookRecord,
// StatisticsBookSnapshotInput, StatisticsTodaySummary,
// StatisticsWeekDaySummary, StatisticsWeekSummary, StatisticsRangeSummary,
// StatisticsSpeedDay, StatisticsSpeedSummary, StatisticsTrendPoint,
// StatisticsTrendBookBreakdown, StatisticsBookRankingRow,
// StatisticsShelfComparisonRow, StatisticsDashboardCalculator) and the
// heatmap scale in Features/Bookshelf/StatisticsDashboardView.swift
// (StatisticsDashboardView.heatColor).
//
// This module is pure: a caller reads book folders, book totals and daily
// rows, passes them in, and gets values back. Nothing opens a file, reads a
// clock or produces user-facing text beyond the date labels the models
// themselves carry ("yyyy-MM-dd", "M/d", "yyyy-MM", "yyyy-Www").
//
// Days are QDate values, so every range covers whole calendar days and needs
// no time zone. `today` is always an argument; derive it once from the
// caller's instant with reporting_day() or local_day(). The only zone
// parameter is local_day's, which defaults to the system zone; pass a fixed
// zone when the instant's own zone is not the one that defines the day.
//
// Ordering
// --------
// Titles compare by Unicode code point, which is UTF-8 byte order, as the
// bookshelf model does. macOS sorts titles, shelf names and breakdown titles
// with localizedStandardCompare, which follows the user's locale, ignores
// case and reads digit runs as numbers ("Vol 2" before "Vol 10"); this port
// keeps the deterministic code-point order. Every tie Swift leaves to
// dictionary order gets a deterministic break here, noted per function.
//
// Inputs that Swift's repository cannot produce
// --------------------------------------------
// * Two rows of one book with the same date key: the row with the greater
//   last_modified wins and an equal modification time keeps the first row,
//   the rule deduplicated_statistics() applies. A Swift snapshot comes from
//   a dictionary keyed by day and would trap instead.
// * A date key that is not exactly "yyyy-MM-dd" is skipped. Swift splits the
//   key on '-' and drops non-numeric components, so it also accepts unpadded
//   components such as "2026-2-3"; date_key() never writes those.
// * A row without characters and without reading time is skipped, as the
//   repository skips it. Its last_modified is kept by deduplication only.
// * The input row carries no title, so a contribution always uses the book's
//   title. Swift falls back to the daily row's title only when the book title
//   is empty.
// * Day totals and speeds sum machine integers and doubles without checks;
//   Swift traps on an overflowing Int, values that cannot reach a card.
//
// Range and target rules a caller must rely on
// --------------------------------------------
// * Speed samples need characters > 0 and at least 60 seconds. A day with
//   book contributions is sampled per contribution only; the day total is
//   used only when the day has no contributions at all, as in Swift.
// * week_summary().met_target_days counts every day of the week whose target
//   ratio reaches 1, including a day later than today; Swift's weekSummary
//   filters the same seven aggregates and does not exclude future days, while
//   its per-day rows do.
// * calendar_heat_factor is 0 for a day without characters (the view draws
//   those in the secondary color) and otherwise 0.18 + 0.72 * the ratio
//   clamped to 0.18..1, so an active day lies in 0.3096..0.9, not 0.18..0.9.

#pragma once

#include <QDate>
#include <QDateTime>
#include <QTimeZone>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/books/book.h"
#include "models/statistics/statistics.h"

namespace niratan::statistics {

enum class StatisticsRangeMode { year, month, week, day };

enum class StatisticsTrendMetric { characters, duration, speed };

enum class StatisticsTrendGrain { day, week, month };

enum class StatisticsBookRankingMetric { characters, duration, speed };

// One persisted daily row of one book, before it becomes a contribution. The
// reading time is in seconds and last_modified is the row's modification
// stamp in milliseconds since the epoch.
struct StatisticsBookDayRow {
    std::string date_key;
    int characters = 0;
    double reading_time = 0.0;
    std::int64_t last_modified = 0;

    bool operator==(const StatisticsBookDayRow&) const = default;
};

// One book handed to build_snapshot: the identity and title the shelf shows,
// the total character count read from bookinfo.json (0 when absent) and every
// daily row the book has.
struct StatisticsBookSnapshotInput {
    std::string id;
    std::string title;
    std::optional<std::string> cover_path;
    int total_characters = 0;
    std::vector<StatisticsBookDayRow> rows;

    bool operator==(const StatisticsBookSnapshotInput&) const = default;
};

// One book's share of one day.
struct StatisticsBookContribution {
    std::string book_id;
    std::string title;
    std::optional<std::string> cover_path;
    int characters = 0;
    double reading_time = 0.0;

    bool operator==(const StatisticsBookContribution&) const = default;
};

// One reporting day with every book's contribution to it.
struct StatisticsDayAggregate {
    QDate date;
    int characters = 0;
    double reading_time = 0.0;
    std::vector<StatisticsBookContribution> book_contributions;

    // Contributions with characters > 0 or reading_time > 0.
    int active_book_count() const;
    // characters / the character target, or reading_time / the duration
    // target in seconds; 0 when the chosen target is not positive. Not
    // clamped.
    double target_ratio(const StatisticsTargetSettings& settings) const;

    bool operator==(const StatisticsDayAggregate&) const = default;
};

// A book as the dashboard lists it.
struct StatisticsBookRecord {
    std::string id;
    std::string title;
    std::optional<std::string> cover_path;
    int total_characters = 0;

    bool operator==(const StatisticsBookRecord&) const = default;
};

// The loaded dashboard input: days ascending and books by title. A book with
// no active day still has a record; a day without an active contribution does
// not exist.
struct StatisticsDashboardSnapshot {
    std::vector<StatisticsDayAggregate> days;
    std::vector<StatisticsBookRecord> books;

    bool operator==(const StatisticsDashboardSnapshot&) const = default;
};

// Builds the snapshot from the books and their rows: rows without activity
// and rows with an unparsable date key are dropped, contributions on one day
// are sorted by title then book id, days ascend by date and books sort by
// title then id.
StatisticsDashboardSnapshot build_snapshot(const std::vector<StatisticsBookSnapshotInput>& books);

// An inclusive span of whole days. A range whose end precedes its start is
// empty rather than an error; nothing here builds one.
struct StatisticsDateRange {
    QDate start;
    QDate end;

    // start <= day <= end.
    bool contains(QDate day) const;
    // `day` clamped into the range.
    QDate coerce(QDate day) const;
    // end - start + 1, and 0 when end precedes start.
    int day_count() const;

    // today minus one year plus one day, through today. A leap day maps to
    // February 28 of the target year, so 2024-02-29 starts at 2023-03-01.
    static StatisticsDateRange recent_year(QDate today);
    // The Monday through Sunday of the week containing `day`.
    static StatisticsDateRange monday_week(QDate day);

    bool operator==(const StatisticsDateRange&) const = default;
};

// The Monday that starts the week containing `day`.
QDate monday_start_of_week(QDate day);

// The span `mode` names around `anchor`, clipped to `window`: year is the
// whole window, month the anchor's month, week the Monday-based week and day
// the anchor alone. The anchor is first coerced into the window, so the
// unclipped span always touches it and the result is never empty.
StatisticsDateRange selected_range(StatisticsRangeMode mode, QDate anchor,
                                   const StatisticsDateRange& window);

// The reporting day of `instant` in `zone`, which defaults to the system
// zone. This is the only place a zone is needed; every other function takes
// the QDate this returns.
QDate local_day(const QDateTime& instant, int reset_minutes,
                const QTimeZone& zone = QTimeZone::systemTimeZone());

// A speed sample needs positive characters and at least this much reading
// time.
inline constexpr double minimum_speed_sample_seconds = 60.0;

// The rounded speed over every valid sample in `days`: per contribution when
// a day has contributions, otherwise the day's own total. nullopt when no
// sample is valid.
std::optional<int> weighted_speed_per_hour(const std::vector<StatisticsDayAggregate>& days);

// Consecutive days at or above the daily target, ending at `today`. When
// today is below the target the count starts at yesterday, so an unfinished
// today does not break the streak.
int daily_goal_streak(const std::vector<StatisticsDayAggregate>& days, QDate today,
                      const StatisticsTargetSettings& settings);

// Consecutive Monday-based weeks with at least weekly_target_days days at or
// above the target, ending at the week of `today`. A current week that is not
// met is skipped the same way. A target below 1 yields 0 rather than an
// endless streak.
int weekly_goal_streak(const std::vector<StatisticsDayAggregate>& days, QDate today,
                       const StatisticsTargetSettings& settings);

struct StatisticsTodaySummary {
    QDate date;
    int target_percent = 0;
    int characters = 0;
    double reading_time = 0.0;
    std::optional<int> average_speed_per_hour;
    int daily_streak_days = 0;

    bool operator==(const StatisticsTodaySummary&) const = default;
};

// Today's totals, the rounded target percent (0 without a day), the day's
// speed and the daily streak.
StatisticsTodaySummary today_summary(const std::vector<StatisticsDayAggregate>& days, QDate today,
                                     const StatisticsTargetSettings& settings);

struct StatisticsWeekDaySummary {
    QDate date;
    bool is_today = false;
    bool is_future = false;
    // Absent for a future day and for a day without an aggregate.
    std::optional<int> percent;
    bool met_target = false;

    bool operator==(const StatisticsWeekDaySummary&) const = default;
};

struct StatisticsWeekSummary {
    StatisticsDateRange range;
    int elapsed_days = 0;
    int characters = 0;
    double reading_time = 0.0;
    std::optional<int> average_speed_per_hour;
    int target_days = 0;
    int met_target_days = 0;
    int daily_streak_days = 0;
    int weekly_streak_weeks = 0;
    int average_characters_per_elapsed_day = 0;
    double average_reading_time_per_elapsed_day = 0.0;
    std::vector<StatisticsWeekDaySummary> days;

    bool operator==(const StatisticsWeekSummary&) const = default;
};

// The Monday-based week of `today`: all seven days are aggregated, elapsed
// days are clamped to 1..7 (today through the week's start), and met days
// count every day at or above the target, including a later day of the week
// that already has data.
StatisticsWeekSummary week_summary(const std::vector<StatisticsDayAggregate>& days, QDate today,
                                   const StatisticsTargetSettings& settings);

struct StatisticsRangeSummary {
    int characters = 0;
    double reading_time = 0.0;
    std::optional<int> average_speed_per_hour;
    int target_days = 0;
    // The target percent of the single day a one-day range covers; 0 for
    // every other range.
    int target_progress_percent = 0;

    bool operator==(const StatisticsRangeSummary&) const = default;
};

// Totals over `range`, days met and the one-day target percent.
StatisticsRangeSummary range_summary(const std::vector<StatisticsDayAggregate>& days,
                                     const StatisticsDateRange& range,
                                     const StatisticsTargetSettings& settings);

struct StatisticsTrendBookBreakdown {
    std::string title;
    int characters = 0;

    bool operator==(const StatisticsTrendBookBreakdown&) const = default;
};

struct StatisticsTrendPoint {
    std::string id;
    std::string label;
    int characters = 0;
    double reading_time = 0.0;
    std::optional<int> average_speed_per_hour;
    std::vector<StatisticsTrendBookBreakdown> book_breakdown;

    // The value a chart plots: characters, reading time, or the speed when
    // the point has one.
    std::optional<double> value(StatisticsTrendMetric metric) const;

    bool operator==(const StatisticsTrendPoint&) const = default;
};

// Trend points over `range`. The span starts at the first and ends at the
// last day inside the range that has data; an empty span yields no points.
// Day grain zero-fills every day of it, week grain emits every Monday-based
// week touching it and month grain every month, each including the weeks or
// months without data. A point's breakdown sums the positive-character
// contributions by title and keeps the five largest.
std::vector<StatisticsTrendPoint> trend_points(StatisticsTrendGrain grain,
                                               const StatisticsDateRange& range,
                                               const std::vector<StatisticsDayAggregate>& days);

struct StatisticsSpeedDay {
    QDate date;
    int speed_per_hour = 0;

    bool operator==(const StatisticsSpeedDay&) const = default;
};

struct StatisticsSpeedSummary {
    std::optional<int> weighted_average_speed_per_hour;
    std::optional<int> median_day_speed_per_hour;
    std::optional<int> recent_active_day_speed_per_hour;
    // Change from the first 14 active days to the last 14, in percent. Absent
    // below 28 active days or when the early average is 0.
    std::optional<int> speed_change_percent;
    // Ties go to the earlier date.
    std::optional<StatisticsSpeedDay> best_day;
    std::optional<StatisticsSpeedDay> worst_day;

    bool operator==(const StatisticsSpeedSummary&) const = default;
};

// Speed over the active days in `range`: the weighted average, the median of
// the per-day averages, the average of the last seven active days, the change
// against the first fourteen and the fastest and slowest day.
StatisticsSpeedSummary speed_summary(const std::vector<StatisticsDayAggregate>& days,
                                     const StatisticsDateRange& range);

struct StatisticsBookRankingRow {
    std::string id;
    std::string title;
    int characters = 0;
    double reading_time = 0.0;
    std::optional<int> average_speed_per_hour;

    bool operator==(const StatisticsBookRankingRow&) const = default;
};

// One row per book with activity in `range`, keeping only rows whose chosen
// metric is positive and sorting by that metric descending, then title, then
// book id. The caller applies any limit and lookahead.
std::vector<StatisticsBookRankingRow> book_ranking_rows(
    const std::vector<StatisticsDayAggregate>& days, const StatisticsDateRange& range,
    StatisticsBookRankingMetric metric);

struct StatisticsShelfComparisonRow {
    std::string id;
    std::string name;
    int book_count = 0;
    int total_characters = 0;
    int recorded_characters = 0;
    double reading_time = 0.0;
    std::optional<int> average_speed_per_hour;

    bool operator==(const StatisticsShelfComparisonRow&) const = default;
};

// One row per shelf plus one for the books on no shelf. Shelf IDs are
// "shelf:<name>", the unshelved row is "unshelved". Only known books count;
// rows without a book are dropped, and the rest sort by recorded characters
// descending, then name, then id.
std::vector<StatisticsShelfComparisonRow> shelf_comparison_rows(
    const std::vector<StatisticsBookRecord>& books,
    const std::vector<niratan::books::BookShelf>& shelves,
    const std::vector<StatisticsDayAggregate>& days, const StatisticsDateRange& range,
    std::string_view unshelved_name);

// The largest daily character count in the snapshot, at least 1 so the
// heatmap scale is defined.
int calendar_heat_max_characters(const std::vector<StatisticsDayAggregate>& days);

// The accent opacity of one heatmap cell: 0 for a day without characters,
// otherwise 0.18 + 0.72 times the day's share of `max_characters` clamped to
// 0.18..1. Callers draw the colors.
double calendar_heat_factor(int characters, int max_characters);

}  // namespace niratan::statistics
