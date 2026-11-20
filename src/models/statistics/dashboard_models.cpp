// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/statistics/dashboard_models.h"

namespace niratan::statistics {

int StatisticsDayAggregate::active_book_count() const {
    return 0;
}

double StatisticsDayAggregate::target_ratio(const StatisticsTargetSettings&) const {
    return 0.0;
}

StatisticsDashboardSnapshot build_snapshot(const std::vector<StatisticsBookSnapshotInput>&) {
    return {};
}

bool StatisticsDateRange::contains(QDate) const {
    return false;
}

QDate StatisticsDateRange::coerce(QDate) const {
    return {};
}

int StatisticsDateRange::day_count() const {
    return 0;
}

StatisticsDateRange StatisticsDateRange::recent_year(QDate) {
    return {};
}

StatisticsDateRange StatisticsDateRange::monday_week(QDate) {
    return {};
}

QDate monday_start_of_week(QDate) {
    return {};
}

StatisticsDateRange selected_range(StatisticsRangeMode, QDate, const StatisticsDateRange&) {
    return {};
}

QDate local_day(const QDateTime&, int, const QTimeZone&) {
    return {};
}

std::optional<int> weighted_speed_per_hour(const std::vector<StatisticsDayAggregate>&) {
    return std::nullopt;
}

int daily_goal_streak(const std::vector<StatisticsDayAggregate>&, QDate,
                      const StatisticsTargetSettings&) {
    return 0;
}

int weekly_goal_streak(const std::vector<StatisticsDayAggregate>&, QDate,
                       const StatisticsTargetSettings&) {
    return 0;
}

StatisticsTodaySummary today_summary(const std::vector<StatisticsDayAggregate>&, QDate,
                                     const StatisticsTargetSettings&) {
    return {};
}

StatisticsWeekSummary week_summary(const std::vector<StatisticsDayAggregate>&, QDate,
                                   const StatisticsTargetSettings&) {
    return {};
}

StatisticsRangeSummary range_summary(const std::vector<StatisticsDayAggregate>&,
                                     const StatisticsDateRange&,
                                     const StatisticsTargetSettings&) {
    return {};
}

std::optional<double> StatisticsTrendPoint::value(StatisticsTrendMetric) const {
    return std::nullopt;
}

std::vector<StatisticsTrendPoint> trend_points(StatisticsTrendGrain, const StatisticsDateRange&,
                                               const std::vector<StatisticsDayAggregate>&) {
    return {};
}

StatisticsSpeedSummary speed_summary(const std::vector<StatisticsDayAggregate>&,
                                     const StatisticsDateRange&) {
    return {};
}

std::vector<StatisticsBookRankingRow> book_ranking_rows(
    const std::vector<StatisticsDayAggregate>&, const StatisticsDateRange&,
    StatisticsBookRankingMetric) {
    return {};
}

std::vector<StatisticsShelfComparisonRow> shelf_comparison_rows(
    const std::vector<StatisticsBookRecord>&, const std::vector<niratan::books::BookShelf>&,
    const std::vector<StatisticsDayAggregate>&, const StatisticsDateRange&, std::string_view) {
    return {};
}

int calendar_heat_max_characters(const std::vector<StatisticsDayAggregate>&) {
    return 0;
}

double calendar_heat_factor(int, int) {
    return 0.0;
}

}  // namespace niratan::statistics
