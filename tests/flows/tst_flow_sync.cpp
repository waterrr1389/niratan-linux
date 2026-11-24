// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Sync/GoogleDriveHandler.swift: ensureBookFolder, getStatisticsFileName.
// Also follows findRootFolder, performRequest and DriveFileList.

#include "fakes/fake_drive_server.h"
#include "services/sync/drive_client.h"
#include "services/sync/drive_files.h"
#include "services/sync/ttu_naming.h"

#include <QtTest>

class SyncFlowTest : public QObject {
    Q_OBJECT

   private slots:
    void locates_ttu_root_on_drive() {
        using namespace niratan::sync;
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({200, R"({"files":[{"id":"folder-1","name":"ttu-reader-data"}]})"});
        QUrlQuery query;
        query.addQueryItem(
            QStringLiteral("q"),
            QStringLiteral(
                "trashed=false and 'root' in parents and "
                "mimeType='application/vnd.google-apps.folder' and name = 'ttu-reader-data'"));
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id, name)"));
        std::optional<DriveResponse> result;
        DriveTransport transport(server.base_url(), [] { return DriveToken{"flow-token", {}}; },
                                 {});
        transport.request(DriveMethod::get, "/drive/v3/files", query, {}, {},
                          [&](DriveResponse response) { result = std::move(response); });
        QTRY_VERIFY(result.has_value());
        QCOMPARE(server.requests().size(), 1);
        const auto& request = server.requests().front();
        QCOMPARE(request.method, "GET");
        QCOMPARE(request.path, QStringLiteral("/drive/v3/files"));
        QCOMPARE(request.query.queryItems(QUrl::FullyDecoded),
                 query.queryItems(QUrl::FullyDecoded));
        QCOMPARE(request.headers.value("authorization"), "Bearer flow-token");
        QVERIFY(result->ok());
        DriveFileList files;
        std::string error;
        QVERIFY2(parse_drive_file_list(*result->body, files, error), error.c_str());
        QCOMPARE(files.files.size(), 1U);
        QCOMPARE(files.files.front().id, "folder-1");
        QCOMPARE(files.files.front().name, "ttu-reader-data");
    }

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

QTEST_GUILESS_MAIN(SyncFlowTest)
#include "tst_flow_sync.moc"
