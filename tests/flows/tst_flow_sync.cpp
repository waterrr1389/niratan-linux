// SPDX-License-Identifier: GPL-3.0-or-later
// Features/Sync/GoogleDriveHandler.swift: ensureBookFolder, getStatisticsFileName.
// Also follows findRootFolder, performRequest and DriveFileList.
// GoogleDriveAuth.swift and GoogleDriveLoopbackAuthorization.swift: sign-in and refresh.
// SyncView.swift, BookshelfViewModel.syncBook and SyncManager.syncBookOnce:
// two app directories have separate settings, credentials, Books and book records,
// but share a fake Drive and token endpoint. The production settings view model
// writes settings and uses the real converter and sidecar storage. Reading state
// is written through storage rather than opening the process-singleton reader.
// Reader-open import-only audio is covered by import_only_audio_or_skip_before_ordinary_sync.

#include "epub_fixture.h"
#include "fakes/fake_drive_server.h"
#include "fakes/oauth_fixture.h"
#include "fakes/ttu_fixture.h"
#include "fakes/ttu_converter_fixture.h"
#include "models/sasayaki/sasayaki.h"
#include "services/settings/global_settings.h"
#include "services/statistics/statistics_storage.h"
#include "services/sync/drive_client.h"
#include "services/sync/drive_files.h"
#include "services/sync/oauth_client.h"
#include "services/sync/oauth_credentials.h"
#include "services/sync/ttu_naming.h"
#include "services/sync/ttu_convert.h"
#include "services/sync/ttu_sync.h"
#include "viewmodels/sync/sync_settings_view_model.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

#define VERIFY_FLOW_CALL(call)                                                                     \
    do {                                                                                           \
        call;                                                                                      \
        if (QTest::currentTestFailed()) {                                                          \
            return;                                                                                \
        }                                                                                          \
    } while (false)

namespace {

using namespace niratan;
using sync::TtuResult;
using viewmodels::SyncSettingsViewModel;

constexpr auto title = "同期の本";
const books::Bookmark first_bookmark{1, 0.5, 15, 711692802.0};
const books::Bookmark second_bookmark{1, 0.8, 18, 711692806.0};
const sasayaki::SasayakiPlaybackData playback{42.5, 0.0, 1.0F, std::nullopt};
const statistics::Statistics first_day{title, "2023-07-22", 15,   600.0,        7200,
                                       7200,  7200,         7200, 1690000000123};
const statistics::Statistics second_day{title, "2023-07-23", 3,    600.0,        7200,
                                        7200,  7200,         7200, 1690070400123};

struct Device {
    QTemporaryDir directory;
    std::filesystem::path root{directory.path().toStdString()};
    books::BookStorage storage{root};
    std::filesystem::path settings = root / "settings.json";
    std::filesystem::path credentials = root / "sync/credentials.json";
    std::unique_ptr<SyncSettingsViewModel> model;
    std::string folder;
    std::filesystem::path book_root;

    void open(const std::filesystem::path& epub, oauth_fixture::AuthFixture& fixture) {
        QVERIFY(directory.isValid());
        const auto imported = storage.import_book(epub.string());
        QVERIFY2(imported.book, imported.error.c_str());
        folder = imported.book->folder;
        const auto path = storage.book_directory(folder);
        QVERIFY(path);
        book_root = *path;
        settings::GlobalSettings store(settings);
        QCOMPARE(store.save_statistics_enabled(true), settings::StoreError::none);
        QCOMPARE(store.save_enable_sasayaki(true), settings::StoreError::none);
        model = std::make_unique<SyncSettingsViewModel>(SyncSettingsViewModel::Config{
            root.string(), fixture.config(), fixture.endpoint.base_url(), QTimeZone::utc()});
        model->setClientId(QString::fromLatin1(oauth_fixture::client_id));
        model->setEnabled(true);
        model->setSyncMode("Manual");
        model->setUploadBooks(false);
        model->setUploadBooks(true);
        model->setSyncStatistics(true);
        model->setSyncAudioBook(true);
    }

    void sign_in(oauth_fixture::AuthFixture& fixture) const {
        const auto calls = fixture.browser_calls;
        fixture.endpoint.enqueue({200, oauth_fixture::exchange_json});
        model->signIn();
        QCOMPARE(model->connectionStatus(), QStringLiteral("Connecting…"));
        QTRY_COMPARE(fixture.browser_calls, calls + 1);
        const auto auth = fixture.authorization;
        QVERIFY(oauth_fixture::callback(oauth_fixture::redirect(auth),
                                        "/?code=fixture&state=" + oauth_fixture::state(auth))
                    .startsWith("HTTP/1.1 200"));
        QTRY_COMPARE(model->connectionStatus(), QStringLiteral("Connected"));
        QVERIFY(model->authenticated());
    }

    void sync_book(TtuResult::Kind kind, int count) const {
        QSignalSpy finished(model.get(), &SyncSettingsViewModel::syncFinished);
        model->syncBook(QString::fromStdString(folder));
        QVERIFY(model->syncing());
        QTRY_VERIFY_WITH_TIMEOUT(finished.size() == 1 || !model->syncing(), 5000);
        QVERIFY2(finished.size() == 1, qPrintable(model->syncError()));
        const auto result = finished.takeFirst();
        QCOMPARE(result.at(0).toInt(), static_cast<int>(kind));
        QCOMPARE(result.at(1).toString(), QString::fromUtf8(title));
        QCOMPARE(result.at(2).toInt(), count);
        QVERIFY(!model->syncing());
    }
};

QJsonObject write_metadata(const FakeDriveServer::Request& request) {
    return request.metadata.isEmpty() ? QJsonDocument::fromJson(request.body).object()
                                      : request.metadata;
}

const FakeDriveServer::Request* named_write(const FakeDriveServer& server, qsizetype start,
                                            QStringView name) {
    const auto& requests = server.requests();
    for (auto i = start; i < requests.size(); ++i) {
        if (write_metadata(requests.at(i)).value("name").toString() == name) {
            return &requests.at(i);
        }
    }
    return nullptr;
}

void verify_days(const Device& device, bool two_days) {
    const auto rows = statistics::load_statistics(device.book_root, 0, QTimeZone::utc()).daily;
    QCOMPARE(rows.size(), two_days ? 2U : 1U);
    QCOMPARE(rows.at(0).date_key, first_day.date_key);
    QCOMPARE(rows.at(0).characters_read, 15);
    if (two_days) {
        QCOMPARE(rows.at(1).date_key, second_day.date_key);
        QCOMPARE(rows.at(1).characters_read, 3);
    }
}

}  // namespace

class SyncFlowTest : public QObject {
    Q_OBJECT

   private slots:
    void syncs_reading_state_between_app_directories() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        auto& drive = fixture.endpoint;
        drive.enable_tree();
        const auto epub = fixture.directory.filePath("round-trip.epub").toStdString();
        QVERIFY(test::write_epub(epub, title, "あいうえおかきくけこ", "さしすせそたちつてと"));
        Device a;
        Device b;
        VERIFY_FLOW_CALL(a.open(epub, fixture));
        VERIFY_FLOW_CALL(b.open(epub, fixture));

        VERIFY_FLOW_CALL(a.sign_in(fixture));
        const auto initial_settings = oauth_fixture::read(a.settings);
        const auto keys = QJsonDocument::fromJson(initial_settings).object();
        QCOMPARE(keys.value("googleClientId"), QJsonValue(oauth_fixture::client_id));
        QCOMPARE(keys.value("syncMode"), QJsonValue("Manual"));
        for (const auto* key :
             {"enableSync", "syncUploadBooks", "statisticsEnableSync", "sasayakiEnableSync"}) {
            QCOMPARE(keys.value(QLatin1StringView(key)), QJsonValue(true));
        }
        const auto initial_credentials = oauth_fixture::read(a.credentials);
        const auto credentials = sync::CredentialsStore(a.credentials).load();
        QVERIFY(credentials);
        QCOMPARE(credentials->client_id, oauth_fixture::client_id);
        QCOMPARE(credentials->access_token, "SYNTHETIC-NEW-ACCESS");
        QCOMPARE(credentials->refresh_token, "SYNTHETIC-NEW-REFRESH");

        QVERIFY(a.storage.save_bookmark(a.folder, first_bookmark));
        QVERIFY(statistics::import_daily_statistics(
            a.book_root, {first_day}, statistics::StatisticsSyncMode::merge, 0, QTimeZone::utc()));
        QVERIFY(a.storage.save_sasayaki_playback(a.folder, playback));
        auto start = drive.requests().size();
        VERIFY_FLOW_CALL(a.sync_book(TtuResult::Kind::exported, 15));
        const auto* root = named_write(drive, start, u"ttu-reader-data");
        QVERIFY(root);
        QCOMPARE(write_metadata(*root).value("parents").toArray(), QJsonArray{"root"});
        const auto* book_folder = named_write(drive, start, u"同期の本");
        QVERIFY(book_folder);
        const auto book_parents = write_metadata(*book_folder).value("parents").toArray();
        QCOMPARE(book_parents, QJsonArray{"new-1"});
        const auto* progress = named_write(drive, start, u"progress_1_6_1690000002000_0.75.json");
        QVERIFY(progress);
        QCOMPARE(
            QJsonDocument::fromJson(progress->media),
            QJsonDocument::fromJson(
                R"({"dataId":0,"exploredCharCount":15,"progress":0.75,"lastBookmarkModified":1690000002000})"));
        const auto a_rows = statistics::load_statistics(a.book_root, 0, QTimeZone::utc()).daily;
        const auto stats_name = QString::fromStdString(sync::ttu_statistics_file_name(a_rows));
        const auto* stats = named_write(drive, start, stats_name);
        QStringList uploaded_names;
        for (auto i = start; i < drive.requests().size(); ++i) {
            uploaded_names.append(write_metadata(drive.requests().at(i)).value("name").toString());
        }
        QVERIFY2(stats, qPrintable(stats_name + " not found among " + uploaded_names.join(", ")));
        const auto exported_days = QJsonDocument::fromJson(stats->media).array();
        QCOMPARE(exported_days.size(), 1);
        QCOMPARE(exported_days.at(0).toObject().value("dateKey"), QJsonValue("2023-07-22"));
        QCOMPARE(exported_days.at(0).toObject().value("charactersRead"), QJsonValue(15));
        QCOMPARE(exported_days.at(0).toObject(),
                 (QJsonObject{{"title", "同期の本"},
                              {"dateKey", "2023-07-22"},
                              {"charactersRead", 15},
                              {"readingTime", 600.0},
                              {"minReadingSpeed", 90},
                              {"altMinReadingSpeed", 90},
                              {"lastReadingSpeed", 90},
                              {"maxReadingSpeed", 90},
                              {"lastStatisticModified", qint64{1690000000123}}}));
        const FakeDriveServer::Request* audio = nullptr;
        const FakeDriveServer::Request* zip = nullptr;
        for (auto i = start; i < drive.requests().size(); ++i) {
            const auto& request = drive.requests().at(i);
            const auto name = request.metadata.value("name").toString();
            if (name.startsWith("audioBook_1_6_") && name.endsWith("_42.5.json")) {
                audio = &request;
            } else if (name.startsWith("bookdata_1_6_20_") && name.endsWith(".zip")) {
                zip = &request;
            }
        }
        QVERIFY(audio);
        const auto exported_audio = QJsonDocument::fromJson(audio->media).object();
        QCOMPARE(exported_audio.value("playbackPosition"), QJsonValue(42.5));
        QCOMPARE(exported_audio.value("title"), QJsonValue(QString::fromUtf8(title)));
        QVERIFY(zip);
        QCOMPARE(write_metadata(*zip).value("parents").toArray(), QJsonArray{"new-2"});
        QCOMPARE(zip->query.queryItemValue("uploadType"), QStringLiteral("multipart"));
        QCOMPARE(zip->media_type, "application/zip");
        QVERIFY(zip->media.startsWith("PK"));
        QCOMPARE(oauth_fixture::read(a.settings), initial_settings);

        VERIFY_FLOW_CALL(b.sign_in(fixture));
        start = drive.requests().size();
        VERIFY_FLOW_CALL(b.sync_book(TtuResult::Kind::imported, 15));
        QCOMPARE(b.storage.load_bookmark(b.folder), std::optional{first_bookmark});
        VERIFY_FLOW_CALL(verify_days(b, false));
        QCOMPARE(b.storage.load_sasayaki_playback(b.folder), std::optional{playback});
        int media_reads = 0;
        for (auto i = start; i < drive.requests().size(); ++i) {
            const auto& request = drive.requests().at(i);
            if (request.query.queryItemValue("alt") == "media") {
                QCOMPARE(request.method, "GET");
                ++media_reads;
            }
        }
        QCOMPARE(media_reads, 3);

        QVERIFY(b.storage.save_bookmark(b.folder, second_bookmark));
        QVERIFY(statistics::import_daily_statistics(
            b.book_root, {second_day}, statistics::StatisticsSyncMode::merge, 0, QTimeZone::utc()));
        start = drive.requests().size();
        VERIFY_FLOW_CALL(b.sync_book(TtuResult::Kind::exported, 18));
        progress = named_write(drive, start, u"progress_1_6_1690000006000_0.9.json");
        QVERIFY(progress);
        QCOMPARE(
            QJsonDocument::fromJson(progress->media),
            QJsonDocument::fromJson(
                R"({"dataId":0,"exploredCharCount":18,"progress":0.9,"lastBookmarkModified":1690000006000})"));
        const auto b_rows = statistics::load_statistics(b.book_root, 0, QTimeZone::utc()).daily;
        const auto both_days = QString::fromStdString(sync::ttu_statistics_file_name(b_rows));
        stats = named_write(drive, start, both_days);
        QVERIFY(stats);
        QCOMPARE(QJsonDocument::fromJson(stats->media).array().size(), 2);
        VERIFY_FLOW_CALL(a.sync_book(TtuResult::Kind::imported, 18));
        QCOMPARE(a.storage.load_bookmark(a.folder), std::optional{second_bookmark});
        VERIFY_FLOW_CALL(verify_days(a, true));

        start = drive.requests().size();
        VERIFY_FLOW_CALL(a.sync_book(TtuResult::Kind::synced, 0));
        QCOMPARE(drive.requests().size() - start, 1);
        QCOMPARE(drive.requests().back().method, "GET");
        a.model->clearCache();
        start = drive.requests().size();
        VERIFY_FLOW_CALL(a.sync_book(TtuResult::Kind::synced, 0));
        QCOMPARE(drive.requests().size() - start, 3);
        QCOMPARE(oauth_fixture::read(a.credentials), initial_credentials);
        QCOMPARE(oauth_fixture::read(a.settings), initial_settings);

        a.model->signOut();
        QVERIFY(!a.model->authenticated());
        QCOMPARE(a.model->connectionStatus(), QStringLiteral("Not connected"));
        QVERIFY(!std::filesystem::exists(a.credentials));
        start = drive.requests().size();
        a.model->syncBook(QString::fromStdString(a.folder));
        QCOMPARE(a.model->syncError(), QStringLiteral("Not authenticated\nPlease sign in"));
        QCOMPARE(drive.requests().size(), start);
    }

    void imports_drive_book_through_the_converter() {
        using namespace niratan::sync;
        ttu_fixture::Library library;
        QVERIFY(library.root.isValid());
        const auto source = library.root.filePath("fixture.zip");
        QVERIFY(ttu_converter_fixture::make_ttu(source.toStdString()));
        QFile archive(source);
        QVERIFY(archive.open(QIODevice::ReadOnly));
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        server.put_file("zip", "bookdata_1_6_fixture.zip", "book-id", archive.readAll());
        auto transport = ttu_fixture::transport_for(server);
        TtuDrive drive(*transport);
        TtuConverter converter(library.storage);
        TtuSync sync(
            drive, library.storage,
            {.export_zip = {},
             .import_zip = [&](const auto& zip,
                               const auto& dest) { return converter.import_zip(zip, dest); }},
            0, QTimeZone::utc());
        ttu_fixture::Result<std::filesystem::path> result;
        sync.import_google_drive_book(DriveSyncFiles({{"zip", "bookdata_1_6_fixture.zip", {}, {}}}),
                                      false, false, {}, result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(!result.error);
        QCOMPARE(result.value, std::optional{library.storage.books_directory() / "A & B_ 猫"});
        QVERIFY(library.storage.load_book_info("A & B_ 猫"));
    }

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
