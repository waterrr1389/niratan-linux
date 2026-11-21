// SPDX-License-Identifier: GPL-3.0-or-later
//
// The state and projections behind the statistics dashboard, following
// Features/Bookshelf/StatisticsDashboardView.swift (the state the view derives
// from UserConfig and the snapshot) and
// StatisticsDashboardRepository.loadSnapshot in
// Features/Bookshelf/StatisticsDashboardModels.swift.
//
// The aggregation itself lives in models/statistics/dashboard_models.h. This
// class only loads the snapshot input, keeps the view state and projects the
// audited results into QML values; it never re-derives a percent, a streak, a
// speed sample, a trend bucket or a ranking order.
//
// Loading
// -------
// refresh() reads every book under Books/ with BookStorage: its title, cover
// and bookinfo character total, plus the daily rows derived by
// services/statistics/statistics_storage read_daily_statistics. A book without
// statistics still contributes its record, so an empty library is
// distinguishable from a library without reading records. Nothing is cached
// and no placeholder data is shown.
//
// `today` is the reporting day of the wall clock in the local zone, read once
// per refresh(). The reset minute is 0 because this port has no
// statisticsResetTime store yet; every storage and dashboard call receives it
// as an argument, so a later setting changes only that constant.
//
// Settings
// --------
// Target settings live in <data root>/settings.json under the Swift
// UserDefaults key names:
//   dailyStatisticsTargetType              "characters" | "duration"
//   dailyStatisticsCharacterTarget         500..50000, step 500
//   dailyStatisticsDurationTargetMinutes   5..240, step 5
//   weeklyStatisticsTargetDays             1..7
// Other keys in the file are preserved. Values are clamped and snapped by
// StatisticsTargetSettings::clamped() on read and on write. A failed write
// keeps the in-memory value and is not reported.
//
// Projections
// -----------
// Each map and list is a QVariant projection of one model result:
//   today            StatisticsTodaySummary
//   week             StatisticsWeekSummary, including the seven day chips
//   selectedRange    StatisticsRangeSummary
//   speed            StatisticsSpeedSummary
//   calendarDays     every day of the recent-year window, one cell per day
//   trend            StatisticsTrendPoint
//   ranking          StatisticsBookRankingRow, first rankingLimit rows
//   shelfComparison  StatisticsShelfComparisonRow
// An absent optional integer is projected as -1 (speeds, target percent,
// speed change), and an absent speed day has an empty date string. -1 is never
// a real value.
//
// Deviations from the Swift view
// ------------------------------
// * No snapshot cache and no loading placeholder: refresh() is synchronous.
// * The trend chart style and the book-ranking metric selector are not state
//   here. The ranking is ordered by characters; the page draws bars.
// * The corrupt-book warning has no input: build_snapshot() reports no skipped
//   books.
// * The unshelved comparison row is named with tr("Unshelved"); every other
//   user-visible string is the page's.

#pragma once

#include <QDate>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::viewmodels {

class StatisticsViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool hasBooks READ has_books NOTIFY dataChanged)
    Q_PROPERTY(bool hasStatistics READ has_statistics NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap today READ today NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap week READ week NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap selectedRange READ selected_range NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap speed READ speed NOTIFY dataChanged)
    Q_PROPERTY(QVariantList calendarDays READ calendar_days NOTIFY dataChanged)
    Q_PROPERTY(QVariantList trend READ trend NOTIFY dataChanged)
    Q_PROPERTY(QVariantList ranking READ ranking NOTIFY dataChanged)
    Q_PROPERTY(bool hasMoreBooks READ has_more_books NOTIFY dataChanged)
    Q_PROPERTY(QVariantList shelfComparison READ shelf_comparison NOTIFY dataChanged)
    Q_PROPERTY(QDate rangeStart READ range_start NOTIFY dataChanged)
    Q_PROPERTY(QDate rangeEnd READ range_end NOTIFY dataChanged)
    Q_PROPERTY(int rangeMode READ range_mode WRITE setRangeMode NOTIFY viewChanged)
    Q_PROPERTY(int trendGrain READ trend_grain WRITE setTrendGrain NOTIFY viewChanged)
    Q_PROPERTY(int trendMetric READ trend_metric WRITE setTrendMetric NOTIFY viewChanged)
    Q_PROPERTY(QDate anchorDate READ anchor_date WRITE setAnchorDate NOTIFY viewChanged)
    Q_PROPERTY(QDate selectedDate READ selected_date WRITE setSelectedDate NOTIFY viewChanged)
    Q_PROPERTY(int targetType READ target_type WRITE setTargetType NOTIFY settingsChanged)
    Q_PROPERTY(int characterTarget READ character_target WRITE setCharacterTarget NOTIFY
                   settingsChanged)
    Q_PROPERTY(int durationTargetMinutes READ duration_target_minutes WRITE
                   setDurationTargetMinutes NOTIFY settingsChanged)
    Q_PROPERTY(int weeklyTargetDays READ weekly_target_days WRITE setWeeklyTargetDays NOTIFY
                   settingsChanged)
    Q_PROPERTY(QVariantMap targetLimits READ target_limits CONSTANT)

   public:
    explicit StatisticsViewModel(QObject* parent = nullptr);
    ~StatisticsViewModel() override;

    // The dashboard keeps its range selection across pages, so one instance
    // serves the process and QML does not own it.
    static StatisticsViewModel* instance();
    static StatisticsViewModel* create(QQmlEngine* engine, QJSEngine* js_engine);

    bool has_books() const;
    bool has_statistics() const;
    QVariantMap today() const;
    QVariantMap week() const;
    QVariantMap selected_range() const;
    QVariantMap speed() const;
    QVariantList calendar_days() const;
    QVariantList trend() const;
    QVariantList ranking() const;
    bool has_more_books() const;
    QVariantList shelf_comparison() const;
    QDate range_start() const;
    QDate range_end() const;

    int range_mode() const;
    void setRangeMode(int mode);
    int trend_grain() const;
    void setTrendGrain(int grain);
    int trend_metric() const;
    void setTrendMetric(int metric);
    QDate anchor_date() const;
    void setAnchorDate(QDate date);
    QDate selected_date() const;
    void setSelectedDate(QDate date);

    int target_type() const;
    void setTargetType(int type);
    int character_target() const;
    void setCharacterTarget(int target);
    int duration_target_minutes() const;
    void setDurationTargetMinutes(int minutes);
    int weekly_target_days() const;
    void setWeeklyTargetDays(int days);
    QVariantMap target_limits() const;

    // Reads every book and rebuilds the snapshot. The page calls this when it
    // is shown, so a reading session started elsewhere is picked up.
    Q_INVOKABLE void refresh();

    // Raises the ranking limit by one page.
    Q_INVOKABLE void showMoreBooks();

    // Selects a day from the reading calendar; its "yyyy-MM-dd" key also
    // becomes the range anchor.
    Q_INVOKABLE void selectCalendarDay(const QString& dateKey);

   signals:
    void dataChanged();
    void viewChanged();
    void settingsChanged();

   private:
    struct State;
    std::unique_ptr<State> state_;

    void project();
    void reset_selection();
    void persist_settings();
};

}  // namespace niratan::viewmodels
