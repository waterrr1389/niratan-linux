// SPDX-License-Identifier: GPL-3.0-or-later
//
// Flow test: reading statistics end to end, following
// NativeMac/NativeReaderView.swift (loadBook's On autostart, startTracking,
// updateStats, nextChapter's flushStats, saveStats, stopTracking,
// sessionStatistics, todaysStatistics, allTimeStatistics,
// reconcileStatisticsFocus), Core/StatisticsStorage.swift (save and
// StatisticsStorage.dailyStatistics), Models/Statistics.swift
// (ReadingSession.track, ReadingSessionLog.days, StatisticsDayBoundary),
// Features/Reader/StatisticsView.swift (ReaderStatisticsContentView) and
// Features/Bookshelf/StatisticsDashboardModels.swift
// (StatisticsDashboardRepository.loadSnapshot, StatisticsDashboardCalculator).
//
// The test drives the reader view model with the page messages
// tests/viewmodels/tst_reader_mining_context.cpp uses, then reads the stored
// files and the dashboard view model. The clock is the system clock, so time
// assertions are relational: characters are exact, the session start and end
// fall inside the instants captured around the open and the close, and a
// session under a minute has no speed sample.
//
// Book identities are read back through BookStorage because the import
// generates the record id and the folder is the sanitized title.

#include "viewmodels/reader/reader_view_model.h"
#include "viewmodels/statistics/statistics_view_model.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>
#include <QtTest>

#include <cmath>
#include <filesystem>
#include <string>

#include "data_root.h"
#include "epub_fixture.h"
#include "services/books/book_storage.h"
#include "services/statistics/statistics_storage.h"
#include "services/storage/app_paths.h"

using niratan::viewmodels::ReaderViewModel;
using niratan::viewmodels::StatisticsViewModel;

class FlowStatisticsTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void reports_the_reading_session();
    void statistics_settings_gate_tracking();
    void hidden_reader_page_pauses_tracking();

   private:
    niratan::test::TempDataRoot data_root_;
    ReaderViewModel* reader_ = nullptr;
};

namespace {

const QString kTitle = QStringLiteral("統計の本");
const QString kFirstChapter = QStringLiteral("あいうえおかきくけこ");
const QString kSecondChapter = QStringLiteral("さしすせそたちつ");

qint64 now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

// The tracker ignores an interval whose end is not later than its baseline.
void wait_for_clock_tick() {
    const qint64 start = now_ms();
    while (now_ms() == start) {
        QTest::qWait(1);
    }
}

QString revision(const ReaderViewModel& reader) {
    return QUrlQuery(QUrl(reader.chapter_url())).queryItemValue(QStringLiteral("v"));
}

void send(ReaderViewModel& reader, const QString& name, const QJsonObject& data) {
    reader.message(name, QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
}

void complete_restore(ReaderViewModel& reader) {
    send(reader, QStringLiteral("restoreCompleted"), {{"token", revision(reader)}});
}

bool write_settings(bool enabled, const QString& autostart_mode) {
    const auto root = niratan::storage::data_root();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    const QJsonObject settings{
        {"enableStatistics", enabled},
        {"statisticsAutostartMode", autostart_mode},
        {"statisticsResetTime", 0},
        {"statisticsResetTimeMigratedToMinutes", true},
    };
    QFile file(QString::fromStdString((root / "settings.json").string()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(QJsonDocument(settings).toJson()) > 0;
}

QString book_root(const QString& folder) {
    const niratan::books::BookStorage storage(niratan::storage::data_root());
    const auto root = storage.book_directory(folder.toStdString());
    return root.has_value() ? QString::fromStdString(root->string()) : QString();
}

QString book_id(const QString& folder) {
    for (const auto& book :
         niratan::books::BookStorage(niratan::storage::data_root()).load_all_books()) {
        if (QString::fromStdString(book.folder) == folder) {
            return QString::fromStdString(book.id);
        }
    }
    return {};
}

QString sessions_file(const QString& root) {
    return root + QLatin1Char('/') +
        QString::fromStdString(std::string(niratan::statistics::statistics_sessions_file_name()));
}

QString daily_file(const QString& root) {
    return root + QLatin1Char('/') +
        QString::fromStdString(std::string(niratan::statistics::statistics_daily_file_name()));
}

QJsonDocument read_document(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll());
}

QJsonObject read_object(const QString& path) {
    return read_document(path).object();
}

QJsonArray read_array(const QString& path) {
    return read_document(path).array();
}

QString inspector_count(const ReaderViewModel& reader, const QString& period) {
    return reader.statistics_inspector().value(period).toMap().value("characters").toString();
}

bool refresh_and_wait(StatisticsViewModel& dashboard) {
    QSignalSpy loaded(&dashboard, &StatisticsViewModel::dataChanged);
    dashboard.refresh();
    return loaded.wait(10000) && !dashboard.loading();
}

int expected_speed(double reading_time) {
    return static_cast<int>(14.0 / reading_time * 3600.0);
}

}  // namespace

void FlowStatisticsTest::initTestCase() {
    QVERIFY(data_root_.isValid());
    reader_ = ReaderViewModel::instance();
}

// B1, B2, B3, B4, B5, B6, B7, B8: loadBook's On autostart, startTracking,
// updateStats, nextChapter (flushStats), saveStats, stopTracking;
// StatisticsStorage.save and ReadingSessionLog.dailyStatistics;
// StatisticsDashboardRepository.loadSnapshot and
// StatisticsDashboardCalculator.todaySummary/.weekSummary/.bookRankingRows;
// StatisticsDayAggregate, StatisticsBookDetailPanel.daysList;
// StatisticsEditor.deleting.
void FlowStatisticsTest::reports_the_reading_session() {
    QVERIFY(write_settings(true, QStringLiteral("On")));
    const QString source = data_root_.path() + "/statistics.epub";
    QVERIFY(niratan::test::write_epub(source.toStdString(), kTitle.toStdString(),
                                      kFirstChapter.toStdString(), kSecondChapter.toStdString()));

    const qint64 opened_before = now_ms();
    QVERIFY(reader_->importAndOpen(QUrl::fromLocalFile(source)));
    const qint64 opened_after = now_ms();
    const QString folder = reader_->book_folder();
    QVERIFY(!folder.isEmpty());
    QCOMPARE(reader_->chapter_count(), 2);

    reader_->setReaderActive(true);
    complete_restore(*reader_);
    QVERIFY(!reader_->restoring());
    QVERIFY(reader_->tracking());

    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.2));
    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.8));

    const QString root = book_root(folder);
    QVERIFY(!root.isEmpty());
    const QString sessions_path = sessions_file(root);
    const QString daily_path = daily_file(root);

    wait_for_clock_tick();
    reader_->handlePageTurnResult(true, QStringLiteral("limit"));
    QCOMPARE(reader_->chapter_index(), 1);

    const QJsonObject midway = read_object(sessions_path).value("sessions").toObject();
    QCOMPARE(int(midway.size()), 1);
    const QString session_key = midway.keys().first();
    QCOMPARE(midway.value(session_key)
                 .toObject()
                 .value("value")
                 .toObject()
                 .value("charactersRead")
                 .toInt(),
             10);

    complete_restore(*reader_);
    QVERIFY(!reader_->restoring());
    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.5));

    QTest::qWait(1100);
    QCOMPARE(reader_->statistics_today_characters(), 14);
    QCOMPARE(inspector_count(*reader_, "session"), QStringLiteral("14"));
    QCOMPARE(inspector_count(*reader_, "today"), QStringLiteral("14"));
    QCOMPARE(inspector_count(*reader_, "allTime"), QStringLiteral("14"));

    const qint64 closed_before = now_ms();
    reader_->closeBook();
    const qint64 closed_after = now_ms();

    const QJsonObject stored = read_object(sessions_path);
    const QJsonObject records = stored.value("sessions").toObject();
    QCOMPARE(int(records.size()), 1);
    QCOMPARE(records.keys().first(), session_key);
    const QJsonObject record = records.value(session_key).toObject();
    QVERIFY(record.value("value").isObject());
    const QJsonObject session = record.value("value").toObject();
    const qint64 started_at = static_cast<qint64>(session.value("startedAt").toDouble());
    const qint64 ended_at = static_cast<qint64>(session.value("endedAt").toDouble());
    const double reading_time = session.value("readingTime").toDouble();
    const qint64 modified = static_cast<qint64>(record.value("modified").toDouble());

    QCOMPARE(session.value("charactersRead").toInt(), 14);
    QVERIFY(reading_time > 0.0);
    QVERIFY(reading_time <= static_cast<double>(closed_after - opened_before) / 1000.0);
    QVERIFY(started_at >= opened_before);
    QVERIFY(started_at <= opened_after);
    QVERIFY(ended_at >= closed_before);
    QVERIFY(ended_at <= closed_after);

    const QJsonArray daily = read_array(daily_path);
    QCOMPARE(int(daily.size()), 1);
    const QJsonObject row = daily.first().toObject();
    const QDateTime start = QDateTime::fromMSecsSinceEpoch(started_at, QTimeZone::systemTimeZone());
    const QString day_key = QString::fromStdString(niratan::statistics::date_key(start, 0));
    QCOMPARE(row.value("dateKey").toString(), day_key);
    QCOMPARE(row.value("title").toString(), kTitle);
    QCOMPARE(row.value("charactersRead").toInt(), 14);
    QCOMPARE(row.value("readingTime").toDouble(), reading_time);
    const int speed = expected_speed(reading_time);
    for (const QString& key :
         {QStringLiteral("minReadingSpeed"), QStringLiteral("altMinReadingSpeed"),
          QStringLiteral("lastReadingSpeed"), QStringLiteral("maxReadingSpeed")}) {
        QCOMPARE(row.value(key).toInt(), speed);
    }
    QCOMPARE(static_cast<qint64>(row.value("lastStatisticModified").toDouble()), modified);
    QCOMPARE(
        static_cast<qint64>(stored.value("exportedDaily").toObject().value(day_key).toDouble()),
        modified);

    auto* dashboard = StatisticsViewModel::instance();
    QVERIFY(refresh_and_wait(*dashboard));
    QVERIFY(dashboard->has_loaded());
    QVERIFY(dashboard->has_statistics());
    QVERIFY(!dashboard->has_corrupt_statistics());

    const QVariantMap today = dashboard->today();
    QCOMPARE(today.value("characters").toInt(), 14);
    QCOMPARE(today.value("duration").toDouble(), reading_time);
    QCOMPARE(today.value("speed").toInt(), -1);

    const QVariantMap week = dashboard->week();
    QCOMPARE(week.value("characters").toInt(), 14);
    bool today_chip = false;
    for (const QVariant& chip : week.value("days").toList()) {
        const QVariantMap day = chip.toMap();
        if (day.value("isToday").toBool()) {
            today_chip = true;
            QCOMPARE(day.value("dateKey").toString(), day_key);
        }
    }
    QVERIFY(today_chip);
    QCOMPARE(dashboard->selected_range().value("characters").toInt(), 14);

    const QString id = book_id(folder);
    QVERIFY(!id.isEmpty());
    const QVariantList ranking = dashboard->ranking();
    QCOMPARE(int(ranking.size()), 1);
    const QVariantMap ranking_row = ranking.first().toMap();
    QCOMPARE(ranking_row.value("id").toString(), id);
    QCOMPARE(ranking_row.value("characters").toInt(), 14);
    QCOMPARE(ranking_row.value("time").toDouble(), reading_time);
    QCOMPARE(ranking_row.value("speed").toInt(), -1);

    dashboard->selectCalendarDay(day_key);
    const QVariantMap selected = dashboard->selected_calendar_day();
    QCOMPARE(selected.value("hasRecords").toBool(), true);
    QCOMPARE(selected.value("characters").toInt(), 14);
    QCOMPARE(selected.value("books").toInt(), 1);
    QCOMPARE(selected.value("duration").toDouble(), reading_time);

    bool heat_cell = false;
    for (const QVariant& cell : dashboard->calendar_days()) {
        const QVariantMap day = cell.toMap();
        if (day.value("dateKey").toString() == day_key) {
            heat_cell = true;
            QCOMPARE(day.value("characters").toInt(), 14);
            QVERIFY(std::abs(day.value("factor").toDouble() - 0.9) <= 1e-9);
        }
    }
    QVERIFY(heat_cell);

    QVERIFY(dashboard->openBookDetails(id));
    const QVariantList days = dashboard->book_days();
    QCOMPARE(int(days.size()), 1);
    const QVariantMap detail_day = days.first().toMap();
    QCOMPARE(detail_day.value("characters").toInt(), 14);
    const QVariantList detail_sessions = detail_day.value("sessions").toList();
    QCOMPARE(int(detail_sessions.size()), 1);
    const QVariantMap detail_session = detail_sessions.first().toMap();
    QCOMPARE(detail_session.value("characters").toInt(), 14);
    QCOMPARE(detail_session.value("duration").toDouble(), reading_time);
    QCOMPARE(detail_session.value("speed").toInt(), speed);

    QTest::qWait(2);
    QVERIFY(dashboard->deleteBookSessions(QString(), day_key));
    QVERIFY(refresh_and_wait(*dashboard));

    const QJsonObject tombstoned = read_object(sessions_path).value("sessions").toObject();
    QCOMPARE(int(tombstoned.size()), 1);
    const QJsonObject tombstone = tombstoned.value(session_key).toObject();
    QVERIFY(tombstone.value("value").isNull());
    QVERIFY(static_cast<qint64>(tombstone.value("modified").toDouble()) > modified);
    QVERIFY(QFile::exists(daily_path));
    QVERIFY(read_array(daily_path).isEmpty());

    QVERIFY(!dashboard->has_statistics());
    QCOMPARE(dashboard->today().value("characters").toInt(), 0);
    QVERIFY(dashboard->ranking().isEmpty());
    QVERIFY(dashboard->has_books());
}

// B9: NativeReaderModel.startTracking, startTrackingOnPageTurnIfNeeded and
// handleManualNavigation.
void FlowStatisticsTest::statistics_settings_gate_tracking() {
    QVERIFY(write_settings(false, QStringLiteral("On")));
    const QString source = data_root_.path() + "/gate.epub";
    QVERIFY(
        niratan::test::write_epub(source.toStdString(), "設定の本", "かきくけこ", "さしすせそ"));

    QVERIFY(reader_->importAndOpen(QUrl::fromLocalFile(source)));
    const QString folder = reader_->book_folder();
    QVERIFY(!folder.isEmpty());
    reader_->setReaderActive(true);
    complete_restore(*reader_);
    QVERIFY(!reader_->tracking());

    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.2));
    QVERIFY(!reader_->tracking());
    QCOMPARE(reader_->statistics_today_characters(), 0);
    reader_->closeBook();

    const QString root = book_root(folder);
    QVERIFY(!root.isEmpty());
    QVERIFY(!QFile::exists(sessions_file(root)));
    QVERIFY(!QFile::exists(daily_file(root)));
    QVERIFY(!niratan::statistics::has_statistics(std::filesystem::path(root.toStdString())));

    QVERIFY(write_settings(true, QStringLiteral("Page Turn")));
    QVERIFY(reader_->openBook(folder));
    QVERIFY(!reader_->tracking());
    reader_->setReaderActive(true);
    complete_restore(*reader_);
    QVERIFY(!reader_->tracking());

    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.2));
    QVERIFY(reader_->tracking());
    reader_->closeBook();
}

// B10: NativeReaderModel.reconcileStatisticsFocus and isStatisticsContextActive
// through ReaderViewModel.setReaderActive.
void FlowStatisticsTest::hidden_reader_page_pauses_tracking() {
    QVERIFY(write_settings(true, QStringLiteral("On")));
    const QString source = data_root_.path() + "/paused.epub";
    QVERIFY(niratan::test::write_epub(source.toStdString(), "停止の本", kFirstChapter.toStdString(),
                                      kSecondChapter.toStdString()));

    QVERIFY(reader_->importAndOpen(QUrl::fromLocalFile(source)));
    reader_->setReaderActive(true);
    complete_restore(*reader_);
    QVERIFY(!reader_->restoring());
    QVERIFY(reader_->tracking());

    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.2));
    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.3));
    QCOMPARE(reader_->statistics_today_characters(), 3);

    reader_->setReaderActive(false);
    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.6));
    QCOMPARE(reader_->statistics_today_characters(), 3);

    reader_->setReaderActive(true);
    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.7));
    QCOMPARE(reader_->statistics_today_characters(), 4);
    wait_for_clock_tick();
    reader_->handlePageTurnResult(false, QVariant(0.8));
    QCOMPARE(reader_->statistics_today_characters(), 5);

    reader_->closeBook();
}

QTEST_GUILESS_MAIN(FlowStatisticsTest)
#include "tst_flow_statistics.moc"
