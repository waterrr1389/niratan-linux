// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Sync/GoogleDriveHandler.swift: ensureBookFolder, getStatisticsFileName.

#include "services/sync/ttu_naming.h"

#include <QtTest>

class SyncFlowTest : public QObject {
    Q_OBJECT

   private slots:
    void reading_session_addresses_drive_statistics() {
        using namespace niratan;
        const std::string title = "吾輩/猫.";
        const auto date = QDateTime::fromMSecsSinceEpoch(1790812800000, QTimeZone::utc());
        statistics::ReadingSession session = statistics::ReadingSession::starting_at(date);
        const auto end = date.addSecs(600);
        session.track(1200, 600, end);
        const auto rows = statistics::daily_statistics(
            {{"session", {end.toMSecsSinceEpoch(), session}}}, title, 0, QTimeZone::utc());
        QVERIFY(!rows.empty());
        const auto path = "ttu-reader-data/" + sync::sanitize_ttu_title(title) + "/" +
            sync::ttu_statistics_file_name(rows);
        QCOMPARE(path,
                 "ttu-reader-data/吾輩%2F猫~ttu-dend~/"
                 "statistics_1_6_1790813400000_1200_600.0_7200_7200_7200.0_7200_600.0_600.0_1200.0_"
                 "1200.0_7200.0_7200.0_na.json");
    }
};

QTEST_APPLESS_MAIN(SyncFlowTest)
#include "tst_flow_sync.moc"
