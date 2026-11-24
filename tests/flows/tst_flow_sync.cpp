// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Sync/GoogleDriveHandler.swift: ensureBookFolder, getStatisticsFileName.
// Also follows findRootFolder, performRequest and DriveFileList.
// GoogleDriveAuth.swift and GoogleDriveLoopbackAuthorization.swift: sign-in and refresh.

#include "fakes/fake_drive_server.h"
#include "fakes/oauth_fixture.h"
#include "fakes/ttu_fixture.h"
#include "services/statistics/statistics_storage.h"
#include "services/sync/drive_client.h"
#include "services/sync/drive_files.h"
#include "services/sync/oauth_client.h"
#include "services/sync/oauth_credentials.h"
#include "services/sync/ttu_naming.h"
#include "services/sync/ttu_sync.h"

#include <QJsonDocument>
#include <QtTest>

class SyncFlowTest : public QObject {
    Q_OBJECT

   private slots:
    void imports_ttu_sidecars_then_exports_a_newer_bookmark() {
        using namespace niratan;
        using namespace niratan::sync;
        using namespace ttu_fixture;
        Library library;
        QVERIFY(library.start());
        QVERIFY(library.bookmark(100, remote_ms - 1000));
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        seed_media(server);
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        TtuSync sync(drive, library.storage, {}, 0, QTimeZone::utc());
        const TtuSyncOptions options{.sync_stats = true, .sync_audio_book = true};
        Result<TtuResult> result;
        sync.sync_book(library.book, {}, options, result.receive());
        QTRY_VERIFY(result.done);
        QCOMPARE(result.value,
                 (std::optional{TtuResult{TtuResult::Kind::imported, "Display Title", 250}}));
        QVERIFY(!result.error);
        QCOMPARE(
            library.storage.load_bookmark(library.book.folder),
            (std::optional{books::Bookmark{1, 0.0625, 250, remote_ms / 1000.0 - unix_reference}}));
        const auto rows = statistics::load_statistics(library.path(), 0, QTimeZone::utc()).daily;
        QCOMPARE(rows.size(), 1U);
        QCOMPARE(rows.front().date_key, "2023-07-22");
        QCOMPARE(rows.front().characters_read, 1200);
        QCOMPARE(library.storage.load_sasayaki_playback(library.book.folder),
                 (std::optional{sasayaki::SasayakiPlaybackData{42.5, 0.0, 1.0F, {}}}));
        const auto requests_before = server.requests().size();
        result = {};
        sync.sync_book(library.book, {}, options, result.receive());
        QTRY_VERIFY(result.done);
        QCOMPARE(result.value,
                 (std::optional{TtuResult{TtuResult::Kind::synced, "Display Title", 0}}));
        QCOMPARE(server.requests().size(), requests_before + 1);
        QCOMPARE(server.requests().back().method, "GET");
        QVERIFY(library.bookmark(600, remote_ms + 2000));
        result = {};
        sync.sync_book(library.book, {}, options, result.receive());
        QTRY_VERIFY(result.done);
        QCOMPARE(result.value,
                 (std::optional{TtuResult{TtuResult::Kind::exported, "Display Title", 600}}));
        bool uploaded_progress = false;
        for (const auto& request : server.requests()) {
            if (request.path == QStringLiteral("/upload/drive/v3/files/p") &&
                request.method == "PATCH") {
                QCOMPARE(request.metadata.value("name").toString(),
                         QStringLiteral("progress_1_6_1690000002000_0.6.json"));
                QCOMPARE(
                    QJsonDocument::fromJson(request.media),
                    QJsonDocument::fromJson(
                        R"({"dataId":7,"exploredCharCount":600,"progress":0.6,"lastBookmarkModified":1690000002000})"));
                uploaded_progress = true;
            }
        }
        QVERIFY(uploaded_progress);
    }

    void signs_in_over_loopback_and_refreshes_drive_credentials() {
        using namespace niratan::sync;
        using namespace oauth_fixture;
        AuthFixture fixture;
        QVERIFY(fixture.start());
        fixture.endpoint.enqueue({200, exchange_json});
        fixture.endpoint.enqueue({200, R"({"access_token":"SYNTHETIC-REFRESHED-ACCESS"})"});
        bool authenticated = false;
        std::optional<AuthError> error;
        std::optional<std::string> refreshed;
        GoogleDriveAuth auth(fixture.settings(), fixture.credentials(), fixture.loopback,
                             fixture.config());
        auth.authenticate([&](std::optional<AuthError> failure) {
            error = std::move(failure);
            authenticated = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(fixture.authorization.isValid(), 1000);
        const auto uri = redirect(fixture.authorization);
        QCOMPARE(uri.host(), QStringLiteral("127.0.0.1"));
        QVERIFY(uri.port() > 0);
        QVERIFY(callback(uri, "/?code=fixture-code&state=" + state(fixture.authorization))
                    .startsWith("HTTP/1.1 200"));
        QTRY_VERIFY_WITH_TIMEOUT(authenticated, 1000);
        QVERIFY(!error);
        QVERIFY(auth.is_authenticated());
        QCOMPARE(fixture.endpoint.requests().size(), 1);
        const auto& request = fixture.endpoint.requests().front();
        QCOMPARE(request.path, QStringLiteral("/token"));
        QCOMPARE(request.form.queryItemValue("code"), QStringLiteral("fixture-code"));
        QCOMPARE(request.form.queryItemValue("redirect_uri"), uri.toString());
        QCOMPARE(LoopbackAuthorization::code_challenge(
                     request.form.queryItemValue("code_verifier").toStdString()),
                 query_value(fixture.authorization, "code_challenge").toStdString());
        GoogleDriveCredentials expected{"SYNTHETIC-NEW-ACCESS", "SYNTHETIC-NEW-REFRESH", client_id,
                                        "configured-secret"};
        CredentialsStore store(fixture.credentials());
        QCOMPARE(store.load(), std::optional{expected});
        QCOMPARE(std::filesystem::status(fixture.credentials()).permissions(),
                 std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
        bool refresh_finished = false;
        auth.refresh([&](auto token, auto failure) {
            refreshed = std::move(token);
            error = std::move(failure);
            refresh_finished = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(refresh_finished, 1000);
        QVERIFY(!error);
        expected.access_token = "SYNTHETIC-REFRESHED-ACCESS";
        QCOMPARE(refreshed, std::optional{expected.access_token});
        QCOMPARE(store.load(), std::optional{expected});
        QCOMPARE(fixture.endpoint.requests().size(), 2);
        QCOMPARE(fixture.endpoint.requests().back().path, QStringLiteral("/token"));
    }

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
