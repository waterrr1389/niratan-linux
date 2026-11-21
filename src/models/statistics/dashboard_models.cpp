// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/statistics/dashboard_models.h"

#include <QDate>
#include <QDateTime>
#include <QLatin1Char>
#include <QString>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace niratan::statistics {
namespace {

double target_ratio_of(const StatisticsTargetSettings& settings, int characters,
                       double reading_time) {
    switch (settings.daily_target_type) {
    case DailyTargetType::characters:
        if (settings.daily_character_target <= 0) {
            return 0.0;
        }
        return static_cast<double>(characters) /
               static_cast<double>(settings.daily_character_target);
    case DailyTargetType::duration: {
        const double target_seconds =
            static_cast<double>(settings.daily_duration_target_minutes) * 60.0;
        if (target_seconds <= 0.0) {
            return 0.0;
        }
        return reading_time / target_seconds;
    }
    }
    return 0.0;
}

std::optional<QDate> parse_day(std::string_view key) {
    const QDate day = QDate::fromString(QString::fromStdString(std::string(key)),
                                        QStringLiteral("yyyy-MM-dd"));
    if (!day.isValid()) {
        return std::nullopt;
    }
    return day;
}

std::vector<StatisticsBookDayRow> deduplicate_rows(
    const std::vector<StatisticsBookDayRow>& rows) {
    std::vector<StatisticsBookDayRow> unique;
    std::map<std::string, std::size_t, std::less<>> positions;
    for (const StatisticsBookDayRow& row : rows) {
        const auto [position, inserted] = positions.try_emplace(row.date_key, unique.size());
        if (inserted) {
            unique.push_back(row);
        } else if (row.last_modified > unique[position->second].last_modified) {
            unique[position->second] = row;
        }
    }
    return unique;
}

bool is_speed_sample(int characters, double reading_time) {
    return characters > 0 && reading_time >= minimum_speed_sample_seconds;
}

struct SpeedSample {
    double characters;
    double time;
};

void append_speed_samples(const StatisticsDayAggregate& day, std::vector<SpeedSample>& samples) {
    if (day.book_contributions.empty()) {
        if (is_speed_sample(day.characters, day.reading_time)) {
            samples.push_back(
                {static_cast<double>(day.characters), day.reading_time});
        }
        return;
    }
    for (const StatisticsBookContribution& contribution : day.book_contributions) {
        if (is_speed_sample(contribution.characters, contribution.reading_time)) {
            samples.push_back({static_cast<double>(contribution.characters),
                               contribution.reading_time});
        }
    }
}

bool has_speed_sample(const StatisticsDayAggregate& day) {
    if (day.book_contributions.empty()) {
        return is_speed_sample(day.characters, day.reading_time);
    }
    return std::ranges::any_of(day.book_contributions, [](const StatisticsBookContribution& item) {
        return is_speed_sample(item.characters, item.reading_time);
    });
}

std::optional<int> speed_of_samples(const std::vector<SpeedSample>& samples) {
    if (samples.empty()) {
        return std::nullopt;
    }
    double characters = 0.0;
    double time = 0.0;
    for (const SpeedSample& sample : samples) {
        characters += sample.characters;
        time += sample.time;
    }
    return static_cast<int>(std::llround(characters / time * 3600.0));
}

std::optional<int> weighted_speed(const std::vector<const StatisticsDayAggregate*>& days) {
    std::vector<SpeedSample> samples;
    for (const StatisticsDayAggregate* day : days) {
        append_speed_samples(*day, samples);
    }
    return speed_of_samples(samples);
}

std::optional<int> contribution_speed(
    const std::vector<const StatisticsBookContribution*>& contributions) {
    std::vector<SpeedSample> samples;
    for (const StatisticsBookContribution* contribution : contributions) {
        if (is_speed_sample(contribution->characters, contribution->reading_time)) {
            samples.push_back({static_cast<double>(contribution->characters),
                               contribution->reading_time});
        }
    }
    return speed_of_samples(samples);
}

std::map<QDate, const StatisticsDayAggregate*> index_days(
    const std::vector<StatisticsDayAggregate>& days) {
    std::map<QDate, const StatisticsDayAggregate*> index;
    for (const StatisticsDayAggregate& day : days) {
        if (day.date.isValid()) {
            index.try_emplace(day.date, &day);
        }
    }
    return index;
}

std::optional<int> median(std::vector<int> values) {
    if (values.empty()) {
        return std::nullopt;
    }
    std::ranges::sort(values);
    const std::size_t middle = values.size() / 2;
    if (values.size() % 2 == 0) {
        return static_cast<int>(std::llround(
            (static_cast<double>(values[middle - 1]) + static_cast<double>(values[middle])) /
            2.0));
    }
    return values[middle];
}

QDate month_start(QDate day) {
    return QDate(day.year(), day.month(), 1);
}

std::string day_id(QDate day) {
    return day.toString(QStringLiteral("yyyy-MM-dd")).toStdString();
}

std::string day_label(QDate day) {
    return QStringLiteral("%1/%2").arg(day.month()).arg(day.day()).toStdString();
}

std::string month_label(QDate day) {
    return day.toString(QStringLiteral("yyyy-MM")).toStdString();
}

std::string iso_week_label(QDate monday) {
    int year = 0;
    const int week = monday.weekNumber(&year);
    return QStringLiteral("%1-W%2")
        .arg(year, 4, 10, QLatin1Char('0'))
        .arg(week, 2, 10, QLatin1Char('0'))
        .toStdString();
}

bool week_met(const std::map<QDate, const StatisticsDayAggregate*>& index, QDate week_start,
              const StatisticsTargetSettings& settings) {
    int met_days = 0;
    for (int offset = 0; offset < 7; ++offset) {
        const auto position = index.find(week_start.addDays(offset));
        if (position != index.end() &&
            target_ratio_of(settings, position->second->characters,
                            position->second->reading_time) >= 1.0) {
            ++met_days;
        }
    }
    return met_days >= settings.weekly_target_days;
}

StatisticsTrendPoint make_trend_point(const std::string& id, const std::string& label,
                                      const std::vector<const StatisticsDayAggregate*>& days) {
    StatisticsTrendPoint point;
    point.id = id;
    point.label = label;
    for (const StatisticsDayAggregate* day : days) {
        point.characters += day->characters;
        point.reading_time += day->reading_time;
    }
    point.average_speed_per_hour = weighted_speed(days);

    std::map<std::string, int, std::less<>> grouped;
    for (const StatisticsDayAggregate* day : days) {
        for (const StatisticsBookContribution& contribution : day->book_contributions) {
            if (contribution.characters > 0) {
                grouped[contribution.title] += contribution.characters;
            }
        }
    }
    std::vector<StatisticsTrendBookBreakdown> breakdown;
    breakdown.reserve(grouped.size());
    for (const auto& [title, characters] : grouped) {
        StatisticsTrendBookBreakdown row;
        row.title = title;
        row.characters = characters;
        breakdown.push_back(std::move(row));
    }
    std::ranges::sort(breakdown, [](const StatisticsTrendBookBreakdown& left,
                                    const StatisticsTrendBookBreakdown& right) {
        if (left.characters != right.characters) {
            return left.characters > right.characters;
        }
        return left.title < right.title;
    });
    if (breakdown.size() > 5) {
        breakdown.resize(5);
    }
    point.book_breakdown = std::move(breakdown);
    return point;
}

double ranking_value(const StatisticsBookRankingRow& row,
                     StatisticsBookRankingMetric metric) {
    switch (metric) {
    case StatisticsBookRankingMetric::characters:
        return static_cast<double>(row.characters);
    case StatisticsBookRankingMetric::duration:
        return row.reading_time;
    case StatisticsBookRankingMetric::speed:
        return static_cast<double>(row.average_speed_per_hour.value_or(0));
    }
    return 0.0;
}

}  // namespace

int StatisticsDayAggregate::active_book_count() const {
    return static_cast<int>(
        std::ranges::count_if(book_contributions, [](const StatisticsBookContribution& item) {
            return item.characters > 0 || item.reading_time > 0;
        }));
}

double StatisticsDayAggregate::target_ratio(const StatisticsTargetSettings& settings) const {
    return target_ratio_of(settings, characters, reading_time);
}

StatisticsDashboardSnapshot build_snapshot(const std::vector<StatisticsBookSnapshotInput>& books) {
    std::map<QDate, std::vector<StatisticsBookContribution>> contributions_by_date;
    StatisticsDashboardSnapshot snapshot;

    for (const StatisticsBookSnapshotInput& book : books) {
        StatisticsBookRecord record;
        record.id = book.id;
        record.title = book.title;
        record.cover_path = book.cover_path;
        record.total_characters = book.total_characters;
        snapshot.books.push_back(std::move(record));

        for (const StatisticsBookDayRow& row : deduplicate_rows(book.rows)) {
            if (!(row.characters > 0 || row.reading_time > 0)) {
                continue;
            }
            const std::optional<QDate> day = parse_day(row.date_key);
            if (!day.has_value()) {
                continue;
            }
            StatisticsBookContribution contribution;
            contribution.book_id = book.id;
            contribution.title = book.title;
            contribution.cover_path = book.cover_path;
            contribution.characters = row.characters;
            contribution.reading_time = row.reading_time;
            contributions_by_date[*day].push_back(std::move(contribution));
        }
    }

    std::ranges::sort(snapshot.books, [](const StatisticsBookRecord& left,
                                         const StatisticsBookRecord& right) {
        if (left.title != right.title) {
            return left.title < right.title;
        }
        return left.id < right.id;
    });

    for (auto& [date, contributions] : contributions_by_date) {
        StatisticsDayAggregate aggregate;
        aggregate.date = date;
        std::ranges::sort(contributions, [](const StatisticsBookContribution& left,
                                            const StatisticsBookContribution& right) {
            if (left.title != right.title) {
                return left.title < right.title;
            }
            return left.book_id < right.book_id;
        });
        for (const StatisticsBookContribution& contribution : contributions) {
            aggregate.characters += contribution.characters;
            aggregate.reading_time += contribution.reading_time;
        }
        aggregate.book_contributions = std::move(contributions);
        snapshot.days.push_back(std::move(aggregate));
    }

    return snapshot;
}

bool StatisticsDateRange::contains(QDate day) const {
    return day >= start && day <= end;
}

QDate StatisticsDateRange::coerce(QDate day) const {
    if (day < start) {
        return start;
    }
    if (day > end) {
        return end;
    }
    return day;
}

int StatisticsDateRange::day_count() const {
    if (!start.isValid() || !end.isValid() || end < start) {
        return 0;
    }
    return static_cast<int>(start.daysTo(end)) + 1;
}

StatisticsDateRange StatisticsDateRange::recent_year(QDate today) {
    const QDate one_year_ago = today.addYears(-1);
    return {one_year_ago.addDays(1), today};
}

StatisticsDateRange StatisticsDateRange::monday_week(QDate day) {
    const QDate start = monday_start_of_week(day);
    return {start, start.addDays(6)};
}

QDate monday_start_of_week(QDate day) {
    return day.addDays(1 - day.dayOfWeek());
}

StatisticsDateRange selected_range(StatisticsRangeMode mode, QDate anchor,
                                   const StatisticsDateRange& window) {
    const QDate coerced = window.coerce(anchor);
    StatisticsDateRange unclipped;
    switch (mode) {
    case StatisticsRangeMode::year:
        unclipped = window;
        break;
    case StatisticsRangeMode::month: {
        const QDate start = month_start(coerced);
        unclipped = {start, start.addMonths(1).addDays(-1)};
        break;
    }
    case StatisticsRangeMode::week:
        unclipped = StatisticsDateRange::monday_week(coerced);
        break;
    case StatisticsRangeMode::day:
        unclipped = {coerced, coerced};
        break;
    }
    return {std::max(unclipped.start, window.start), std::min(unclipped.end, window.end)};
}

QDate local_day(const QDateTime& instant, int reset_minutes, const QTimeZone& zone) {
    return reporting_day(instant.toTimeZone(zone), reset_minutes);
}

std::optional<int> weighted_speed_per_hour(const std::vector<StatisticsDayAggregate>& days) {
    std::vector<const StatisticsDayAggregate*> pointers;
    pointers.reserve(days.size());
    for (const StatisticsDayAggregate& day : days) {
        pointers.push_back(&day);
    }
    return weighted_speed(pointers);
}

int daily_goal_streak(const std::vector<StatisticsDayAggregate>& days, QDate today,
                      const StatisticsTargetSettings& settings) {
    const std::map<QDate, const StatisticsDayAggregate*> index = index_days(days);
    QDate cursor = today;
    const auto today_position = index.find(today);
    const double today_ratio =
        today_position == index.end()
            ? 0.0
            : target_ratio_of(settings, today_position->second->characters,
                              today_position->second->reading_time);
    if (today_ratio < 1.0) {
        cursor = cursor.addDays(-1);
    }

    int streak = 0;
    while (true) {
        const auto position = index.find(cursor);
        if (position == index.end() ||
            target_ratio_of(settings, position->second->characters,
                            position->second->reading_time) < 1.0) {
            break;
        }
        ++streak;
        cursor = cursor.addDays(-1);
    }
    return streak;
}

int weekly_goal_streak(const std::vector<StatisticsDayAggregate>& days, QDate today,
                       const StatisticsTargetSettings& settings) {
    if (settings.weekly_target_days < 1) {
        return 0;
    }
    const std::map<QDate, const StatisticsDayAggregate*> index = index_days(days);
    QDate week_start = monday_start_of_week(today);
    if (!week_met(index, week_start, settings)) {
        week_start = week_start.addDays(-7);
    }

    int streak = 0;
    while (week_met(index, week_start, settings)) {
        ++streak;
        week_start = week_start.addDays(-7);
    }
    return streak;
}

StatisticsTodaySummary today_summary(const std::vector<StatisticsDayAggregate>& days, QDate today,
                                     const StatisticsTargetSettings& settings) {
    StatisticsTodaySummary summary;
    summary.date = today;
    const std::map<QDate, const StatisticsDayAggregate*> index = index_days(days);
    const auto position = index.find(today);
    if (position != index.end()) {
        summary.characters = position->second->characters;
        summary.reading_time = position->second->reading_time;
        summary.average_speed_per_hour = weighted_speed({position->second});
    }
    summary.target_percent = target_percent(settings, summary.characters, summary.reading_time);
    summary.daily_streak_days = daily_goal_streak(days, today, settings);
    return summary;
}

StatisticsWeekSummary week_summary(const std::vector<StatisticsDayAggregate>& days, QDate today,
                                   const StatisticsTargetSettings& settings) {
    StatisticsWeekSummary summary;
    summary.range = StatisticsDateRange::monday_week(today);
    summary.elapsed_days =
        std::clamp(static_cast<int>(summary.range.start.daysTo(today)) + 1, 1, 7);
    summary.target_days = settings.weekly_target_days;

    const std::map<QDate, const StatisticsDayAggregate*> index = index_days(days);
    std::vector<const StatisticsDayAggregate*> present;
    for (int offset = 0; offset < 7; ++offset) {
        const QDate date = summary.range.start.addDays(offset);
        const auto position = index.find(date);
        const StatisticsDayAggregate* day =
            position == index.end() ? nullptr : position->second;
        const bool is_future = date > today;
        const double ratio =
            day == nullptr ? 0.0 : target_ratio_of(settings, day->characters, day->reading_time);
        if (day != nullptr) {
            summary.characters += day->characters;
            summary.reading_time += day->reading_time;
            present.push_back(day);
            if (ratio >= 1.0) {
                ++summary.met_target_days;
            }
        }

        StatisticsWeekDaySummary day_summary;
        day_summary.date = date;
        day_summary.is_today = date == today;
        day_summary.is_future = is_future;
        if (day != nullptr && !is_future) {
            day_summary.percent = target_percent(settings, day->characters, day->reading_time);
        }
        day_summary.met_target = !is_future && ratio >= 1.0;
        summary.days.push_back(day_summary);
    }

    summary.average_speed_per_hour = weighted_speed(present);
    summary.daily_streak_days = daily_goal_streak(days, today, settings);
    summary.weekly_streak_weeks = weekly_goal_streak(days, today, settings);
    summary.average_characters_per_elapsed_day = static_cast<int>(std::llround(
        static_cast<double>(summary.characters) / static_cast<double>(summary.elapsed_days)));
    summary.average_reading_time_per_elapsed_day =
        summary.reading_time / static_cast<double>(summary.elapsed_days);
    return summary;
}

StatisticsRangeSummary range_summary(const std::vector<StatisticsDayAggregate>& days,
                                     const StatisticsDateRange& range,
                                     const StatisticsTargetSettings& settings) {
    StatisticsRangeSummary summary;
    const std::map<QDate, const StatisticsDayAggregate*> index = index_days(days);
    std::vector<const StatisticsDayAggregate*> present;
    for (QDate date = range.start; date <= range.end; date = date.addDays(1)) {
        const auto position = index.find(date);
        if (position == index.end()) {
            continue;
        }
        const StatisticsDayAggregate* day = position->second;
        summary.characters += day->characters;
        summary.reading_time += day->reading_time;
        present.push_back(day);
        if (target_ratio_of(settings, day->characters, day->reading_time) >= 1.0) {
            ++summary.target_days;
        }
    }
    summary.average_speed_per_hour = weighted_speed(present);
    if (range.day_count() == 1) {
        summary.target_progress_percent =
            target_percent(settings, summary.characters, summary.reading_time);
    }
    return summary;
}

std::optional<double> StatisticsTrendPoint::value(StatisticsTrendMetric metric) const {
    switch (metric) {
    case StatisticsTrendMetric::characters:
        return static_cast<double>(characters);
    case StatisticsTrendMetric::duration:
        return reading_time;
    case StatisticsTrendMetric::speed:
        if (average_speed_per_hour.has_value()) {
            return static_cast<double>(*average_speed_per_hour);
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::vector<StatisticsTrendPoint> trend_points(StatisticsTrendGrain grain,
                                               const StatisticsDateRange& range,
                                               const std::vector<StatisticsDayAggregate>& days) {
    std::vector<const StatisticsDayAggregate*> days_in_range;
    for (const StatisticsDayAggregate& day : days) {
        if (range.contains(day.date)) {
            days_in_range.push_back(&day);
        }
    }
    if (days_in_range.empty()) {
        return {};
    }
    std::ranges::sort(days_in_range,
                      [](const StatisticsDayAggregate* left, const StatisticsDayAggregate* right) {
                          return left->date < right->date;
                      });
    const QDate trend_start = days_in_range.front()->date;
    const QDate trend_end = days_in_range.back()->date;

    std::vector<StatisticsTrendPoint> points;
    switch (grain) {
    case StatisticsTrendGrain::day: {
        const std::map<QDate, const StatisticsDayAggregate*> index = index_days(days);
        for (QDate date = trend_start; date <= trend_end; date = date.addDays(1)) {
            std::vector<const StatisticsDayAggregate*> point_days;
            const auto position = index.find(date);
            if (position != index.end()) {
                point_days.push_back(position->second);
            }
            points.push_back(make_trend_point(day_id(date), day_label(date), point_days));
        }
        break;
    }
    case StatisticsTrendGrain::week: {
        std::map<QDate, std::vector<const StatisticsDayAggregate*>> grouped;
        for (const StatisticsDayAggregate* day : days_in_range) {
            grouped[monday_start_of_week(day->date)].push_back(day);
        }
        for (QDate cursor = monday_start_of_week(trend_start); cursor <= trend_end;
             cursor = cursor.addDays(7)) {
            const std::string label = iso_week_label(cursor);
            const auto position = grouped.find(cursor);
            if (position == grouped.end()) {
                points.push_back(make_trend_point(label, label, {}));
            } else {
                points.push_back(make_trend_point(label, label, position->second));
            }
        }
        break;
    }
    case StatisticsTrendGrain::month: {
        std::map<QDate, std::vector<const StatisticsDayAggregate*>> grouped;
        for (const StatisticsDayAggregate* day : days_in_range) {
            grouped[month_start(day->date)].push_back(day);
        }
        for (QDate cursor = month_start(trend_start), end = month_start(trend_end);
             cursor <= end; cursor = cursor.addMonths(1)) {
            const std::string label = month_label(cursor);
            const auto position = grouped.find(cursor);
            if (position == grouped.end()) {
                points.push_back(make_trend_point(label, label, {}));
            } else {
                points.push_back(make_trend_point(label, label, position->second));
            }
        }
        break;
    }
    }
    return points;
}

StatisticsSpeedSummary speed_summary(const std::vector<StatisticsDayAggregate>& days,
                                     const StatisticsDateRange& range) {
    std::vector<const StatisticsDayAggregate*> active;
    for (const StatisticsDayAggregate& day : days) {
        if (range.contains(day.date) && has_speed_sample(day)) {
            active.push_back(&day);
        }
    }
    std::ranges::sort(active,
                      [](const StatisticsDayAggregate* left, const StatisticsDayAggregate* right) {
                          return left->date < right->date;
                      });

    StatisticsSpeedSummary summary;
    summary.weighted_average_speed_per_hour = weighted_speed(active);

    std::vector<int> day_speeds;
    day_speeds.reserve(active.size());
    for (const StatisticsDayAggregate* day : active) {
        day_speeds.push_back(weighted_speed({day}).value_or(0));
    }
    summary.median_day_speed_per_hour = median(day_speeds);

    if (active.size() > 7) {
        const std::vector<const StatisticsDayAggregate*> recent(active.end() - 7, active.end());
        summary.recent_active_day_speed_per_hour = weighted_speed(recent);
    } else {
        summary.recent_active_day_speed_per_hour = weighted_speed(active);
    }

    for (std::size_t index = 0; index < active.size(); ++index) {
        StatisticsSpeedDay speed_day;
        speed_day.date = active[index]->date;
        speed_day.speed_per_hour = day_speeds[index];
        if (!summary.best_day.has_value() ||
            speed_day.speed_per_hour > summary.best_day->speed_per_hour) {
            summary.best_day = speed_day;
        }
        if (!summary.worst_day.has_value() ||
            speed_day.speed_per_hour < summary.worst_day->speed_per_hour) {
            summary.worst_day = speed_day;
        }
    }

    if (active.size() >= 28) {
        const std::vector<const StatisticsDayAggregate*> early(active.begin(), active.begin() + 14);
        const std::vector<const StatisticsDayAggregate*> recent(active.end() - 14, active.end());
        const std::optional<int> early_speed = weighted_speed(early);
        const std::optional<int> recent_speed = weighted_speed(recent);
        if (early_speed.has_value() && recent_speed.has_value() && *early_speed > 0) {
            summary.speed_change_percent = static_cast<int>(std::llround(
                (static_cast<double>(*recent_speed - *early_speed) /
                 static_cast<double>(*early_speed)) *
                100.0));
        }
    }
    return summary;
}

std::vector<StatisticsBookRankingRow> book_ranking_rows(
    const std::vector<StatisticsDayAggregate>& days, const StatisticsDateRange& range,
    StatisticsBookRankingMetric metric) {
    std::map<std::string, std::vector<const StatisticsBookContribution*>, std::less<>> grouped;
    for (const StatisticsDayAggregate& day : days) {
        if (!range.contains(day.date)) {
            continue;
        }
        for (const StatisticsBookContribution& contribution : day.book_contributions) {
            if (contribution.characters > 0 || contribution.reading_time > 0) {
                grouped[contribution.book_id].push_back(&contribution);
            }
        }
    }

    std::vector<StatisticsBookRankingRow> rows;
    rows.reserve(grouped.size());
    for (const auto& [book_id, contributions] : grouped) {
        if (contributions.empty()) {
            continue;
        }
        StatisticsBookRankingRow row;
        row.id = book_id;
        row.title = contributions.front()->title;
        for (const StatisticsBookContribution* contribution : contributions) {
            row.characters += contribution->characters;
            row.reading_time += contribution->reading_time;
        }
        row.average_speed_per_hour = contribution_speed(contributions);
        if (ranking_value(row, metric) > 0.0) {
            rows.push_back(std::move(row));
        }
    }

    std::ranges::sort(rows, [metric](const StatisticsBookRankingRow& left,
                                     const StatisticsBookRankingRow& right) {
        const double left_value = ranking_value(left, metric);
        const double right_value = ranking_value(right, metric);
        if (left_value != right_value) {
            return left_value > right_value;
        }
        if (left.title != right.title) {
            return left.title < right.title;
        }
        return left.id < right.id;
    });
    return rows;
}

std::vector<StatisticsShelfComparisonRow> shelf_comparison_rows(
    const std::vector<StatisticsBookRecord>& books,
    const std::vector<niratan::books::BookShelf>& shelves,
    const std::vector<StatisticsDayAggregate>& days, const StatisticsDateRange& range,
    std::string_view unshelved_name) {
    std::set<std::string, std::less<>> known_ids;
    for (const StatisticsBookRecord& book : books) {
        known_ids.insert(book.id);
    }

    std::map<std::string, std::vector<const StatisticsBookContribution*>, std::less<>>
        contributions_by_book;
    for (const StatisticsDayAggregate& day : days) {
        if (!range.contains(day.date)) {
            continue;
        }
        for (const StatisticsBookContribution& contribution : day.book_contributions) {
            if (contribution.characters > 0 || contribution.reading_time > 0) {
                contributions_by_book[contribution.book_id].push_back(&contribution);
            }
        }
    }

    const auto build_row =
        [&books, &contributions_by_book](const std::string& id, const std::string& name,
                                         const std::set<std::string, std::less<>>& ids) {
            StatisticsShelfComparisonRow row;
            row.id = id;
            row.name = name;
            for (const StatisticsBookRecord& book : books) {
                if (ids.contains(book.id)) {
                    ++row.book_count;
                    row.total_characters += book.total_characters;
                }
            }
            std::vector<const StatisticsBookContribution*> shelf_contributions;
            for (const std::string& book_id : ids) {
                const auto position = contributions_by_book.find(book_id);
                if (position != contributions_by_book.end()) {
                    shelf_contributions.insert(shelf_contributions.end(),
                                               position->second.begin(),
                                               position->second.end());
                }
            }
            for (const StatisticsBookContribution* contribution : shelf_contributions) {
                row.recorded_characters += contribution->characters;
                row.reading_time += contribution->reading_time;
            }
            row.average_speed_per_hour = contribution_speed(shelf_contributions);
            return row;
        };

    std::vector<StatisticsShelfComparisonRow> rows;
    std::set<std::string, std::less<>> shelved_ids;
    for (const niratan::books::BookShelf& shelf : shelves) {
        std::set<std::string, std::less<>> ids;
        for (const std::string& book_id : shelf.book_ids) {
            if (known_ids.contains(book_id)) {
                ids.insert(book_id);
            }
        }
        shelved_ids.insert(ids.begin(), ids.end());
        rows.push_back(build_row("shelf:" + shelf.name, shelf.name, ids));
    }

    std::set<std::string, std::less<>> unshelved_ids;
    for (const std::string& book_id : known_ids) {
        if (!shelved_ids.contains(book_id)) {
            unshelved_ids.insert(book_id);
        }
    }
    if (!unshelved_ids.empty()) {
        rows.push_back(build_row("unshelved", std::string(unshelved_name), unshelved_ids));
    }

    std::erase_if(rows, [](const StatisticsShelfComparisonRow& row) {
        return row.book_count == 0;
    });
    std::ranges::sort(rows, [](const StatisticsShelfComparisonRow& left,
                               const StatisticsShelfComparisonRow& right) {
        if (left.recorded_characters != right.recorded_characters) {
            return left.recorded_characters > right.recorded_characters;
        }
        if (left.name != right.name) {
            return left.name < right.name;
        }
        return left.id < right.id;
    });
    return rows;
}

int calendar_heat_max_characters(const std::vector<StatisticsDayAggregate>& days) {
    int maximum = 0;
    for (const StatisticsDayAggregate& day : days) {
        maximum = std::max(maximum, day.characters);
    }
    return std::max(maximum, 1);
}

double calendar_heat_factor(int characters, int max_characters) {
    if (characters <= 0 || max_characters <= 0) {
        return 0.0;
    }
    const double ratio = std::clamp(
        static_cast<double>(characters) / static_cast<double>(max_characters), 0.18, 1.0);
    return 0.18 + 0.72 * ratio;
}

}  // namespace niratan::statistics
