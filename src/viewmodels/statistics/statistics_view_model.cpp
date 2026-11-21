// SPDX-License-Identifier: GPL-3.0-or-later

#include "statistics_view_model.h"

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QTimeZone>
#include <QThread>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models/statistics/dashboard_models.h"
#include "models/statistics/statistics.h"
#include "services/books/book_storage.h"
#include "services/settings/global_settings.h"
#include "services/statistics/statistics_storage.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels {
namespace {

using niratan::books::BookInfo;
using niratan::books::BookMetadata;
using niratan::books::BookShelf;
using niratan::books::BookStorage;
using niratan::statistics::DailyTargetType;
using niratan::statistics::Statistics;
using niratan::statistics::StatisticsBookRankingMetric;
using niratan::statistics::StatisticsBookRankingRow;
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
using niratan::statistics::StatisticsTrendGrain;
using niratan::statistics::StatisticsTrendMetric;
using niratan::statistics::StatisticsTrendPoint;
using niratan::statistics::StatisticsWeekSummary;

// statisticsBookRankingLimit in StatisticsDashboardView.swift.
constexpr int kRankingPageSize = 12;

StatisticsRangeMode range_mode_of(int mode) {
    switch (mode) {
        case 1:
            return StatisticsRangeMode::month;
        case 2:
            return StatisticsRangeMode::week;
        case 3:
            return StatisticsRangeMode::day;
        default:
            return StatisticsRangeMode::year;
    }
}

StatisticsTrendGrain trend_grain_of(int grain) {
    switch (grain) {
        case 1:
            return StatisticsTrendGrain::week;
        case 2:
            return StatisticsTrendGrain::month;
        default:
            return StatisticsTrendGrain::day;
    }
}

// A missing optional speed or percent is -1 in the projections.
int optional_value(const std::optional<int>& value) { return value.value_or(-1); }

QString date_key_text(QDate date) { return date.toString(QStringLiteral("yyyy-MM-dd")); }

QString cover_url(const std::optional<std::string>& path) {
    return path.has_value()
               ? QUrl::fromLocalFile(QString::fromStdString(*path)).toString()
               : QString();
}

}  // namespace

struct StatisticsViewModel::State {
    explicit State(std::filesystem::path data_root)
        : storage(data_root), global(std::move(data_root) / "settings.json") {}

    BookStorage storage;
    settings::GlobalSettings global;
    StatisticsTargetSettings settings;
    StatisticsDashboardSnapshot snapshot;
    std::vector<BookShelf> shelves;
    QFileSystemWatcher library_watch;
    std::uint64_t load_generation = 0;
    bool loading = false;
    bool has_loaded = false;
    bool has_corrupt_statistics = false;

    QDate today;
    QDate anchor;
    QDate selected;
    bool anchor_explicit = false;
    bool selected_explicit = false;

    int range_mode = 0;
    int grain = 0;
    int metric = 0;
    int ranking_metric = 0;
    int ranking_limit = kRankingPageSize;

    QDate range_start;
    QDate range_end;
    QVariantMap today_view;
    QVariantMap week_view;
    QVariantMap range_view;
    QVariantMap speed_view;
    QVariantList calendar_view;
    QVariantList trend_view;
    QVariantList ranking_view;
    QVariantList shelf_view;
    std::optional<books::BookMetadata> detail_book;
    QVariantList detail_days;
    QString detail_error;
    bool ranking_has_more = false;
};

StatisticsViewModel* StatisticsViewModel::instance() {
    static StatisticsViewModel* model = new StatisticsViewModel();
    return model;
}

StatisticsViewModel* StatisticsViewModel::create(QQmlEngine*, QJSEngine*) {
    StatisticsViewModel* model = instance();
    // The singleton outlives every engine, so QML must not take ownership.
    QJSEngine::setObjectOwnership(model, QJSEngine::CppOwnership);
    return model;
}

StatisticsViewModel::StatisticsViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>(storage::data_root())) {
    state_->settings = state_->global.statistics_targets();
    state_->today = statistics::local_day(QDateTime::currentDateTime(), load_reset_minutes(),
                                          QTimeZone::systemTimeZone());
    reset_selection();
    project();
    connect(&state_->library_watch, &QFileSystemWatcher::directoryChanged, this,
            [this] { refresh(); });
}

StatisticsViewModel::~StatisticsViewModel() = default;

bool StatisticsViewModel::has_books() const { return !state_->snapshot.books.empty(); }

bool StatisticsViewModel::has_statistics() const { return !state_->snapshot.days.empty(); }

bool StatisticsViewModel::loading() const { return state_->loading; }
bool StatisticsViewModel::has_loaded() const { return state_->has_loaded; }
bool StatisticsViewModel::has_corrupt_statistics() const { return state_->has_corrupt_statistics; }

QVariantMap StatisticsViewModel::today() const { return state_->today_view; }

QVariantMap StatisticsViewModel::week() const { return state_->week_view; }

QVariantMap StatisticsViewModel::selected_range() const { return state_->range_view; }

QVariantMap StatisticsViewModel::speed() const { return state_->speed_view; }

QVariantList StatisticsViewModel::calendar_days() const { return state_->calendar_view; }

QVariantList StatisticsViewModel::book_days() const { return state_->detail_days; }

QString StatisticsViewModel::detail_error() const { return state_->detail_error; }

bool StatisticsViewModel::openBookDetails(const QString& bookId) {
    auto books = state_->storage.load_all_books();
    const auto match = std::ranges::find(books, bookId.toStdString(), &books::BookMetadata::id);
    state_->detail_book = match == books.end() ? std::nullopt : std::optional(*match);
    state_->detail_days.clear();
    state_->detail_error.clear();
    if (state_->detail_book) {
        const auto root = state_->storage.book_directory(state_->detail_book->folder);
        if (root) {
            const int reset = load_reset_minutes();
            const auto zone = QTimeZone::systemTimeZone();
            project_details(statistics::load_statistics(*root, reset, zone).records, reset, zone);
            return true;
        }
    }
    state_->detail_error = tr("The selected book is no longer available.");
    emit detailChanged();
    return false;
}

void StatisticsViewModel::project_details(const statistics::ReadingSessionRecords& records,
                                           int reset_minutes, const QTimeZone& zone) {
    QVariantList days;
    for (const auto& day : statistics::detail_days(records, reset_minutes, zone)) {
        QVariantList sessions;
        for (const auto& entry : day.sessions) {
            const auto& session = entry.session;
            const int minutes = static_cast<int>(std::clamp(std::round(session.reading_time / 60.0),
                0.0, static_cast<double>(std::numeric_limits<int>::max())));
            sessions.append(QVariantMap{
                {QStringLiteral("id"), QString::fromStdString(entry.id)},
                {QStringLiteral("start"), session.start_date(zone)},
                {QStringLiteral("end"), QDateTime::fromMSecsSinceEpoch(session.ended_at, zone)},
                {QStringLiteral("characters"), session.characters_read},
                {QStringLiteral("duration"), session.reading_time},
                {QStringLiteral("speed"), session.reading_speed()},
                {QStringLiteral("hours"), minutes / 60},
                {QStringLiteral("minutes"), minutes % 60},
            });
        }
        const auto total = day.total();
        days.append(QVariantMap{
            {QStringLiteral("dateKey"), QString::fromStdString(day.date_key)},
            {QStringLiteral("characters"), total.characters_read},
            {QStringLiteral("duration"), total.reading_time},
            {QStringLiteral("sessions"), sessions},
        });
    }
    state_->detail_days = std::move(days);
    emit detailChanged();
}

bool StatisticsViewModel::saveBookSession(const QString& id, int characters, int hours,
                                           int minutes) {
    return change_book_sessions(id, {}, characters, hours, minutes);
}

bool StatisticsViewModel::deleteBookSessions(const QString& id, const QString& day) {
    return change_book_sessions(id, day, std::nullopt, 0, 0);
}

bool StatisticsViewModel::change_book_sessions(const QString& id, const QString& day,
                                               std::optional<int> characters, int hours,
                                               int minutes) {
    state_->detail_error.clear();
    const auto root = state_->detail_book
        ? state_->storage.book_directory(state_->detail_book->folder) : std::nullopt;
    if (!root || !state_->storage.load_metadata(state_->detail_book->folder)) {
        state_->detail_error = tr("The selected book is no longer available.");
        emit detailChanged();
        return false;
    }
    const auto zone = QTimeZone::systemTimeZone();
    const int reset = load_reset_minutes();
    auto records = statistics::load_statistics(*root, reset, zone).records;
    const auto stamp = statistics::current_time_milliseconds();
    bool changed = false;
    if (characters.has_value()) {
        changed = statistics::edit_detail_session(records, id.toStdString(), *characters, hours,
                                                   minutes, stamp);
    } else {
        std::vector<std::string> ids;
        if (!id.isEmpty()) {
            ids.push_back(id.toStdString());
        } else {
            const auto key = day.toStdString();
            for (const auto& entry : statistics::session_entries(records)) {
                if (key.empty() || statistics::session_date_key(entry.session, reset, zone) == key) {
                    ids.push_back(entry.id);
                }
            }
        }
        changed = statistics::delete_detail_sessions(records, ids, stamp);
    }

    std::string error;
    if (changed && !statistics::save_statistics(*root, records, state_->detail_book->display_title(),
                                                 reset, zone, &error)) {
        state_->detail_error = tr("Unable to save statistics: %1").arg(QString::fromStdString(error));
        project_details(statistics::load_statistics(*root, reset, zone).records, reset, zone);
        refresh();
        return false;
    }
    project_details(records, reset, zone);
    refresh();
    return true;
}

QVariantMap StatisticsViewModel::selected_calendar_day() const {
    const auto& days = state_->snapshot.days;
    const auto day = std::ranges::find(days, state_->selected, &StatisticsDayAggregate::date);
    return {
        {QStringLiteral("hasRecords"), day != days.end()},
        {QStringLiteral("characters"), day == days.end() ? 0 : day->characters},
        {QStringLiteral("duration"), day == days.end() ? 0.0 : day->reading_time},
        {QStringLiteral("books"), day == days.end() ? 0 : day->active_book_count()},
    };
}

int StatisticsViewModel::ranking_metric() const { return state_->ranking_metric; }

void StatisticsViewModel::setRankingMetric(int metric) {
    const int value = std::clamp(metric, 0, 2);
    if (value != state_->ranking_metric) {
        state_->ranking_metric = value;
        state_->ranking_limit = kRankingPageSize;
        project();
        emit viewChanged();
    }
}

QVariantList StatisticsViewModel::trend() const { return state_->trend_view; }

QVariantList StatisticsViewModel::ranking() const { return state_->ranking_view; }

bool StatisticsViewModel::has_more_books() const { return state_->ranking_has_more; }

QVariantList StatisticsViewModel::shelf_comparison() const { return state_->shelf_view; }

QDate StatisticsViewModel::range_start() const { return state_->range_start; }

QDate StatisticsViewModel::range_end() const { return state_->range_end; }

int StatisticsViewModel::range_mode() const { return state_->range_mode; }

void StatisticsViewModel::setRangeMode(int mode) {
    const int clamped = std::clamp(mode, 0, 3);
    if (state_->range_mode == clamped) {
        return;
    }
    state_->range_mode = clamped;
    state_->ranking_limit = kRankingPageSize;
    project();
    emit viewChanged();
}

int StatisticsViewModel::trend_grain() const { return state_->grain; }

void StatisticsViewModel::setTrendGrain(int grain) {
    const int clamped = std::clamp(grain, 0, 2);
    if (state_->grain == clamped) {
        return;
    }
    state_->grain = clamped;
    project();
    emit viewChanged();
}

int StatisticsViewModel::trend_metric() const { return state_->metric; }

void StatisticsViewModel::setTrendMetric(int metric) {
    const int clamped = std::clamp(metric, 0, 2);
    if (state_->metric == clamped) {
        return;
    }
    state_->metric = clamped;
    // Only the chart's plotted value depends on the metric; the projected
    // points are shared by all three.
    emit viewChanged();
}

QDate StatisticsViewModel::anchor_date() const { return state_->anchor; }

void StatisticsViewModel::setAnchorDate(QDate date) {
    if (!date.isValid() || !state_->today.isValid()) {
        return;
    }
    const StatisticsDateRange window = StatisticsDateRange::recent_year(state_->today);
    const QDate coerced = window.coerce(date);
    state_->anchor_explicit = true;
    if (state_->anchor == coerced) {
        return;
    }
    state_->anchor = coerced;
    state_->ranking_limit = kRankingPageSize;
    project();
    emit viewChanged();
}

QDate StatisticsViewModel::selected_date() const { return state_->selected; }

void StatisticsViewModel::setSelectedDate(QDate date) {
    if (!date.isValid() || !state_->today.isValid()) {
        return;
    }
    const StatisticsDateRange window = StatisticsDateRange::recent_year(state_->today);
    const QDate coerced = window.coerce(date);
    state_->selected_explicit = true;
    if (state_->selected == coerced) {
        return;
    }
    state_->selected = coerced;
    emit viewChanged();
}

int StatisticsViewModel::target_type() const {
    return state_->settings.daily_target_type == DailyTargetType::duration ? 1 : 0;
}

void StatisticsViewModel::setTargetType(int type) {
    const DailyTargetType target = type == 1 ? DailyTargetType::duration
                                             : DailyTargetType::characters;
    if (state_->settings.daily_target_type == target) {
        return;
    }
    state_->settings.daily_target_type = target;
    persist_settings();
    emit settingsChanged();
    project();
}

int StatisticsViewModel::character_target() const {
    return state_->settings.daily_character_target;
}

void StatisticsViewModel::setCharacterTarget(int target) {
    const int snapped = StatisticsTargetSettings::snap_character_target(target);
    if (state_->settings.daily_character_target == snapped) {
        return;
    }
    state_->settings.daily_character_target = snapped;
    persist_settings();
    emit settingsChanged();
    project();
}

int StatisticsViewModel::duration_target_minutes() const {
    return state_->settings.daily_duration_target_minutes;
}

void StatisticsViewModel::setDurationTargetMinutes(int minutes) {
    const int snapped = StatisticsTargetSettings::snap_duration_target(minutes);
    if (state_->settings.daily_duration_target_minutes == snapped) {
        return;
    }
    state_->settings.daily_duration_target_minutes = snapped;
    persist_settings();
    emit settingsChanged();
    project();
}

int StatisticsViewModel::weekly_target_days() const { return state_->settings.weekly_target_days; }

void StatisticsViewModel::setWeeklyTargetDays(int days) {
    const int clamped = std::clamp(days, StatisticsTargetSettings::weekly_target_days_min,
                                   StatisticsTargetSettings::weekly_target_days_max);
    if (state_->settings.weekly_target_days == clamped) {
        return;
    }
    state_->settings.weekly_target_days = clamped;
    persist_settings();
    emit settingsChanged();
    project();
}

QVariantMap StatisticsViewModel::target_limits() const {
    return QVariantMap{
        {QStringLiteral("characterMin"), StatisticsTargetSettings::character_target_min},
        {QStringLiteral("characterMax"), StatisticsTargetSettings::character_target_max},
        {QStringLiteral("characterStep"), StatisticsTargetSettings::character_target_step},
        {QStringLiteral("durationMin"), StatisticsTargetSettings::duration_target_minutes_min},
        {QStringLiteral("durationMax"), StatisticsTargetSettings::duration_target_minutes_max},
        {QStringLiteral("durationStep"), StatisticsTargetSettings::duration_target_minutes_step},
        {QStringLiteral("weeklyMin"), StatisticsTargetSettings::weekly_target_days_min},
        {QStringLiteral("weeklyMax"), StatisticsTargetSettings::weekly_target_days_max},
        {QStringLiteral("weeklyStep"), 1},
    };
}

void StatisticsViewModel::refresh() {
    struct SnapshotLoad {
        StatisticsDashboardSnapshot snapshot;
        std::vector<BookShelf> shelves;
        bool corrupt = false;
    };

    const int reset_minutes = load_reset_minutes();
    const QTimeZone zone = QTimeZone::systemTimeZone();
    const QDate today = statistics::local_day(QDateTime::currentDateTime(), reset_minutes, zone);
    const BookStorage storage = state_->storage;
    const auto books_directory = QString::fromStdString(storage.books_directory().string());
    if (!state_->library_watch.directories().contains(books_directory)) {
        state_->library_watch.addPath(books_directory);
    }
    const auto generation = ++state_->load_generation;
    const auto result = std::make_shared<SnapshotLoad>();
    state_->loading = true;
    emit viewChanged();
    auto* worker = QThread::create([storage, result, reset_minutes, zone] {
        std::vector<StatisticsBookSnapshotInput> inputs;
        for (const BookMetadata& book : storage.load_all_books()) {
            const auto root = storage.book_directory(book.folder);
            if (!root) {
                continue;
            }
            StatisticsBookSnapshotInput input;
            input.id = book.id;
            input.title = book.display_title();
            if (const auto cover = storage.cover_path(book)) {
                input.cover_path = cover->string();
            }
            if (const auto info = storage.load_book_info(book.folder)) {
                input.total_characters = info->character_count;
            }
            if (auto daily = statistics::read_dashboard_statistics(*root, reset_minutes, zone)) {
                for (const Statistics& row : *daily) {
                    input.rows.push_back({row.date_key, row.characters_read, row.reading_time,
                                          row.last_statistic_modified});
                }
            } else {
                result->corrupt = true;
            }
            inputs.push_back(std::move(input));
        }
        result->snapshot = statistics::build_snapshot(inputs);
        result->shelves = storage.load_shelves();
    });
    connect(worker, &QThread::finished, this, [this, generation, result, today] {
        if (generation != state_->load_generation) {
            return;
        }
        state_->today = today;
        state_->snapshot = std::move(result->snapshot);
        state_->shelves = std::move(result->shelves);
        state_->has_corrupt_statistics = result->corrupt;
        state_->loading = false;
        state_->has_loaded = true;
        reset_selection();
        project();
        emit viewChanged();
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void StatisticsViewModel::showMoreBooks() {
    if (!state_->ranking_has_more) {
        return;
    }
    state_->ranking_limit += kRankingPageSize;
    project();
}

void StatisticsViewModel::selectCalendarDay(const QString& dateKey) {
    const QDate day = QDate::fromString(dateKey, QStringLiteral("yyyy-MM-dd"));
    if (!day.isValid() || !state_->today.isValid()) {
        return;
    }
    const StatisticsDateRange window = StatisticsDateRange::recent_year(state_->today);
    const QDate coerced = window.coerce(day);
    state_->anchor_explicit = true;
    state_->selected_explicit = true;
    const bool range_changed = state_->anchor != coerced;
    state_->anchor = coerced;
    state_->selected = coerced;
    state_->ranking_limit = kRankingPageSize;
    if (range_changed) {
        project();
    }
    emit viewChanged();
}

void StatisticsViewModel::reset_selection() {
    if (!state_->today.isValid()) {
        return;
    }
    const StatisticsDateRange window = StatisticsDateRange::recent_year(state_->today);
    const QDate latest =
        state_->snapshot.days.empty()
            ? window.coerce(state_->today)
            : window.coerce(state_->snapshot.days.back().date);

    state_->anchor = state_->anchor_explicit ? window.coerce(state_->anchor) : latest;
    state_->selected = state_->selected_explicit ? window.coerce(state_->selected) : latest;
}

int StatisticsViewModel::load_reset_minutes() const {
    return state_->global.statistics_preferences().preferences.reset_minutes;
}

void StatisticsViewModel::persist_settings() {
    // Re-reading after the write is the reload step: the in-memory settings
    // then match what a later launch sees.
    if (state_->global.save_statistics_targets(state_->settings) == settings::StoreError::none) {
        state_->settings = state_->global.statistics_targets();
    }
}

void StatisticsViewModel::project() {
    if (!state_->today.isValid()) {
        return;
    }

    const StatisticsDashboardSnapshot& snapshot = state_->snapshot;
    const StatisticsTargetSettings& settings = state_->settings;
    const StatisticsDateRange window = StatisticsDateRange::recent_year(state_->today);
    const StatisticsDateRange range =
        statistics::selected_range(range_mode_of(state_->range_mode), state_->anchor, window);
    state_->range_start = range.start;
    state_->range_end = range.end;

    const StatisticsTodaySummary today =
        statistics::today_summary(snapshot.days, state_->today, settings);
    state_->today_view = QVariantMap{
        {QStringLiteral("characters"), today.characters},
        {QStringLiteral("duration"), today.reading_time},
        {QStringLiteral("speed"), optional_value(today.average_speed_per_hour)},
        {QStringLiteral("targetPercent"), today.target_percent},
        {QStringLiteral("streak"), today.daily_streak_days},
    };

    const StatisticsWeekSummary week =
        statistics::week_summary(snapshot.days, state_->today, settings);
    QVariantList chips;
    chips.reserve(static_cast<qsizetype>(week.days.size()));
    for (const statistics::StatisticsWeekDaySummary& day : week.days) {
        chips.append(QVariantMap{
            {QStringLiteral("dateKey"), date_key_text(day.date)},
            {QStringLiteral("percent"), optional_value(day.percent)},
            {QStringLiteral("metTarget"), day.met_target},
            {QStringLiteral("isToday"), day.is_today},
            {QStringLiteral("isFuture"), day.is_future},
        });
    }
    state_->week_view = QVariantMap{
        {QStringLiteral("characters"), week.characters},
        {QStringLiteral("duration"), week.reading_time},
        {QStringLiteral("speed"), optional_value(week.average_speed_per_hour)},
        {QStringLiteral("metTargetDays"), week.met_target_days},
        {QStringLiteral("averageCharacters"), week.average_characters_per_elapsed_day},
        {QStringLiteral("days"), chips},
    };

    const StatisticsRangeSummary summary =
        statistics::range_summary(snapshot.days, range, settings);
    state_->range_view = QVariantMap{
        {QStringLiteral("characters"), summary.characters},
        {QStringLiteral("duration"), summary.reading_time},
        {QStringLiteral("speed"), optional_value(summary.average_speed_per_hour)},
        {QStringLiteral("targetDays"), summary.target_days},
        {QStringLiteral("targetProgressPercent"), summary.target_progress_percent},
    };

    const StatisticsSpeedSummary speeds = statistics::speed_summary(snapshot.days, range);
    state_->speed_view = QVariantMap{
        {QStringLiteral("weighted"), optional_value(speeds.weighted_average_speed_per_hour)},
        {QStringLiteral("typical"), optional_value(speeds.median_day_speed_per_hour)},
        {QStringLiteral("recent"), optional_value(speeds.recent_active_day_speed_per_hour)},
        {QStringLiteral("hasChange"), speeds.speed_change_percent.has_value()},
        {QStringLiteral("change"), speeds.speed_change_percent.value_or(0)},
        {QStringLiteral("bestDate"),
         speeds.best_day.has_value() ? date_key_text(speeds.best_day->date) : QString()},
        {QStringLiteral("bestSpeed"),
         speeds.best_day.has_value() ? speeds.best_day->speed_per_hour : -1},
        {QStringLiteral("worstDate"),
         speeds.worst_day.has_value() ? date_key_text(speeds.worst_day->date) : QString()},
        {QStringLiteral("worstSpeed"),
         speeds.worst_day.has_value() ? speeds.worst_day->speed_per_hour : -1},
    };

    std::map<QDate, int> characters_by_day;
    for (const StatisticsDayAggregate& day : snapshot.days) {
        characters_by_day.insert_or_assign(day.date, day.characters);
    }
    const int heat_max = statistics::calendar_heat_max_characters(snapshot.days);
    QVariantList calendar;
    for (QDate day = window.start; day <= window.end; day = day.addDays(1)) {
        const auto entry = characters_by_day.find(day);
        const int characters = entry == characters_by_day.end() ? 0 : entry->second;
        calendar.append(QVariantMap{
            {QStringLiteral("dateKey"), date_key_text(day)},
            {QStringLiteral("characters"), characters},
            {QStringLiteral("factor"), statistics::calendar_heat_factor(characters, heat_max)},
        });
    }
    state_->calendar_view = calendar;

    QVariantList trend;
    for (const StatisticsTrendPoint& point :
         statistics::trend_points(trend_grain_of(state_->grain), range, snapshot.days)) {
        QVariantList books;
        for (const auto& book : point.book_breakdown) {
            if (books.size() == 3) {
                break;
            }
            books.append(QVariantMap{
                {QStringLiteral("title"), QString::fromStdString(book.title)},
                {QStringLiteral("characters"), book.characters},
            });
        }
        trend.append(QVariantMap{
            {QStringLiteral("id"), QString::fromStdString(point.id)},
            {QStringLiteral("label"), QString::fromStdString(point.label)},
            {QStringLiteral("characters"), point.characters},
            {QStringLiteral("time"), point.reading_time},
            {QStringLiteral("speed"), optional_value(point.average_speed_per_hour)},
            {QStringLiteral("books"), books},
        });
    }
    state_->trend_view = trend;

    std::map<std::string, QString, std::less<>> book_covers;
    for (const statistics::StatisticsBookRecord& book : snapshot.books) {
        book_covers.insert_or_assign(book.id, cover_url(book.cover_path));
    }
    const auto metric = static_cast<StatisticsBookRankingMetric>(state_->ranking_metric);
    const std::vector<StatisticsBookRankingRow> rows =
        statistics::book_ranking_rows(snapshot.days, range, metric);
    state_->ranking_has_more = rows.size() > static_cast<std::size_t>(state_->ranking_limit);
    const int shown = std::min(static_cast<int>(rows.size()), state_->ranking_limit);
    QVariantList ranking;
    for (int index = 0; index < shown; ++index) {
        const StatisticsBookRankingRow& row = rows[static_cast<std::size_t>(index)];
        const auto cover = book_covers.find(row.id);
        double value = row.characters;
        if (metric == StatisticsBookRankingMetric::duration) {
            value = row.reading_time;
        } else if (metric == StatisticsBookRankingMetric::speed) {
            value = row.average_speed_per_hour.value_or(0);
        }
        ranking.append(QVariantMap{
            {QStringLiteral("id"), QString::fromStdString(row.id)},
            {QStringLiteral("value"), value},
            {QStringLiteral("title"), QString::fromStdString(row.title)},
            {QStringLiteral("cover"),
             cover == book_covers.end() ? QString() : cover->second},
            {QStringLiteral("characters"), row.characters},
            {QStringLiteral("time"), row.reading_time},
            {QStringLiteral("speed"), optional_value(row.average_speed_per_hour)},
        });
    }
    state_->ranking_view = ranking;

    QVariantList shelves;
    for (const StatisticsShelfComparisonRow& row : statistics::shelf_comparison_rows(
             snapshot.books, state_->shelves, snapshot.days, range,
             tr("Unshelved").toStdString())) {
        shelves.append(QVariantMap{
            {QStringLiteral("name"), QString::fromStdString(row.name)},
            {QStringLiteral("books"), row.book_count},
            {QStringLiteral("totalCharacters"), row.total_characters},
            {QStringLiteral("characters"), row.recorded_characters},
            {QStringLiteral("time"), row.reading_time},
            {QStringLiteral("speed"), optional_value(row.average_speed_per_hour)},
        });
    }
    state_->shelf_view = shelves;

    emit dataChanged();
}

}  // namespace niratan::viewmodels
