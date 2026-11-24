// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveHandler.swift: performRequest, performDownloadRequest, findRootFolder,
// GoogleDriveError, session, uploadCoverImage and the Codable DTOs.

#include "fakes/fake_drive_server.h"
#include "services/sync/drive_client.h"
#include "services/sync/drive_files.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QtTest>

#include <algorithm>

using namespace niratan::sync;

static_assert(SyncDirection::import_from_ttu != SyncDirection::export_to_ttu);
static_assert(SyncDirection::import_from_ttu != SyncDirection::synced);
static_assert(SyncDirection::export_to_ttu != SyncDirection::synced);

namespace {

constexpr auto files_path = "/drive/v3/files";

DriveToken token() {
    return {"token-a", {}};
}

DriveTransport::Callback receive(std::optional<DriveResponse>& result) {
    return [&result](DriveResponse response) { result = std::move(response); };
}

void get_files(DriveTransport& transport, std::optional<DriveResponse>& result) {
    transport.request(DriveMethod::get, files_path, {}, {}, {}, receive(result));
}

}  // namespace

class DriveClientTest : public QObject {
    Q_OBJECT

   private slots:
    void sends_bearer_and_decoded_query_to_injected_host() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({200, R"({"files":[]})"});
        int token_calls = 0;
        std::optional<DriveResponse> result;
        DriveTransport transport(server.base_url(),
                                 [&] {
                                     ++token_calls;
                                     return token();
                                 },
                                 {});
        QUrlQuery query;
        const auto filter = QStringLiteral(
            "trashed=false and 'root' in parents and mimeType='application/vnd.google-apps.folder' "
            "and name = 'ttu-reader-data'");
        query.addQueryItem(QStringLiteral("q"), filter);
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id, name)"));
        transport.request(DriveMethod::get, files_path, query, {}, {}, receive(result));
        QTRY_VERIFY(result.has_value());
        QCOMPARE(server.requests().size(), 1);
        const auto& request = server.requests().front();
        QCOMPARE(request.method, "GET");
        QCOMPARE(request.path, QString::fromLatin1(files_path));
        QCOMPARE(request.headers.value("host"), server.base_url().authority().toLatin1());
        QCOMPARE(request.headers.value("authorization"), "Bearer token-a");
        QCOMPARE(request.query.queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded), filter);
        QCOMPARE(request.query.queryItemValue(QStringLiteral("fields"), QUrl::FullyDecoded),
                 QStringLiteral("files(id, name)"));
        QCOMPARE(token_calls, 1);
        QVERIFY(result->ok());
    }

    void refreshes_401_once_data() {
        QTest::addColumn<int>("replay_status");
        QTest::addColumn<bool>("refresh_fails");
        QTest::newRow("new-token-succeeds") << 200 << false;
        QTest::newRow("second-401") << 401 << false;
        QTest::newRow("refresh-error") << 0 << true;
    }

    void refreshes_401_once() {
        QFETCH(int, replay_status);
        QFETCH(bool, refresh_fails);
        FakeDriveServer server;
        QVERIFY(server.start());
        const QByteArray files = R"({"files":[]})";
        server.enqueue({401, {}});
        if (replay_status != 0) {
            server.enqueue({replay_status, replay_status == 200 ? files : QByteArray{}});
        }
        int token_calls = 0;
        int refresh_calls = 0;
        std::optional<DriveResponse> result;
        DriveTransport transport(
            server.base_url(),
            [&] {
                ++token_calls;
                return token();
            },
            [&](DriveTransport::RefreshCallback cb) {
                ++refresh_calls;
                QTimer::singleShot(0, &server, [cb = std::move(cb), refresh_fails] {
                    cb(refresh_fails ? DriveToken{{}, "Please sign in"}
                                     : DriveToken{"new-token", {}});
                });
            });
        get_files(transport, result);
        QTRY_VERIFY(result.has_value());
        QCOMPARE(refresh_calls, 1);
        QCOMPARE(token_calls, 1);
        QCOMPARE(server.requests().size(), refresh_fails ? 1 : 2);
        QCOMPARE(server.requests().front().headers.value("authorization"), "Bearer token-a");
        if (!refresh_fails) {
            QCOMPARE(server.requests().back().headers.value("authorization"), "Bearer new-token");
        }
        if (replay_status == 200) {
            QVERIFY(result->ok());
            QCOMPARE(*result->body, files);
        } else {
            QVERIFY(!result->ok());
            QVERIFY(result->error.has_value());
            if (refresh_fails) {
                QCOMPARE(result->error->kind, GoogleDriveErrorKind::unavailable);
                QCOMPARE(result->error->description(), "Please sign in");
            } else {
                QCOMPARE(result->error->kind, GoogleDriveErrorKind::api_error);
                QCOMPARE(result->error->status_code, std::optional{401});
                QCOMPARE(result->error->description(), "Request failed with status 401");
            }
        }
    }

    void http_errors_data() {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<QByteArray>("message");
        QTest::newRow("message") << 500 << QByteArray(R"({"error":{"message":"Something broke"}})")
                                 << QByteArray("Something broke");
        QTest::newRow("not-found")
            << 404 << QByteArray() << QByteArray("Request failed with status 404");
        QTest::newRow("forbidden")
            << 403 << QByteArray() << QByteArray("Request failed with status 403");
        QTest::newRow("missing-message")
            << 400 << QByteArray(R"({"error":{}})") << QByteArray("Request failed with status 400");
        QTest::newRow("not-json") << 400 << QByteArray("Bad request")
                                  << QByteArray("Request failed with status 400");
        QTest::newRow("non-string-message") << 400 << QByteArray(R"({"error":{"message":42}})")
                                            << QByteArray("Request failed with status 400");
        QTest::newRow("error-not-object") << 400 << QByteArray(R"({"error":"bad"})")
                                          << QByteArray("Request failed with status 400");
    }

    void http_errors() {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(QByteArray, message);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({status, body});
        std::optional<DriveResponse> result;
        DriveTransport transport(server.base_url(), token, {});
        get_files(transport, result);
        QTRY_VERIFY(result.has_value());
        QVERIFY(!result->ok());
        QVERIFY(result->error.has_value());
        QCOMPARE(result->error->kind, GoogleDriveErrorKind::api_error);
        QCOMPARE(result->error->status_code, std::optional{status});
        QCOMPARE(result->error->is_stale_cache_error(), status == 404);
        QCOMPARE(result->error->description(), message.toStdString());
    }

    void non_api_error_descriptions() {
        const GoogleDriveError invalid;
        const GoogleDriveError unavailable{GoogleDriveErrorKind::unavailable, "offline", {}};
        QCOMPARE(invalid.description(), "Invalid response from Google Drive");
        QCOMPARE(unavailable.description(), "offline");
        QVERIFY(!invalid.is_stale_cache_error());
        QVERIFY(!unavailable.is_stale_cache_error());
    }

    void decodes_drive_files() {
        DriveFileList list;
        std::string error;
        QVERIFY2(
            parse_drive_file_list(
                R"({"files":[{"id":"f1","name":"bookdata_1_6.zip","parents":["p1"],"thumbnailLink":"https://x/y","extra":true}],"nextPageToken":"tok"})",
                list, error),
            error.c_str());
        QCOMPARE(list.files.size(), 1U);
        QCOMPARE(list.files.front().id, "f1");
        QCOMPARE(list.files.front().name, "bookdata_1_6.zip");
        QCOMPARE(list.files.front().parents, std::optional{std::vector<std::string>{"p1"}});
        QCOMPARE(list.files.front().thumbnail_link, std::optional{std::string("https://x/y")});
        QCOMPARE(list.next_page_token, std::optional{std::string("tok")});
        QVERIFY2(parse_drive_file_list(R"({"files":[{"id":"f2","name":"cover_1_6.jpeg"}]})", list,
                                       error),
                 error.c_str());
        QCOMPARE(list.files.size(), 1U);
        QCOMPARE(list.files.front().id, "f2");
        QCOMPARE(list.files.front().name, "cover_1_6.jpeg");
        QVERIFY(!list.files.front().parents);
        QVERIFY(!list.files.front().thumbnail_link);
        QVERIFY(!list.next_page_token);
        QVERIFY(!parse_drive_file_list(R"({"files":[{"id":"f3"}]})", list, error));
    }

    void picks_first_match_per_prefix() {
        const DriveSyncFiles files({
            {"1", "bookdata_old.zip", {}, {}},
            {"2", "bookdata_new.zip", {}, {}},
            {"3", "cover_1_6.jpeg", {}, {}},
            {"4", "progress_1_6.json", {}, {}},
            {"5", "statistics_1_6.json", {}, {}},
            {"6", "audioBook_1_6.json", {}, {}},
        });
        QVERIFY(files.book_data && files.cover && files.progress && files.statistics &&
                files.audio_book);
        QCOMPARE(files.book_data->id, "1");
        QCOMPARE(files.cover->id, "3");
        QCOMPARE(files.progress->id, "4");
        QCOMPARE(files.statistics->id, "5");
        QCOMPARE(files.audio_book->id, "6");
        const DriveSyncFiles empty({});
        QVERIFY(!empty.book_data && !empty.cover && !empty.progress && !empty.statistics &&
                !empty.audio_book);
        const DriveSyncFiles no_underscore({{"7", "cover", {}, {}}, {"8", "statistics", {}, {}}});
        QVERIFY(!no_underscore.cover && !no_underscore.statistics);
    }

    void progress_json_round_trip() {
        const QByteArray json =
            R"({"dataId":7,"exploredCharCount":1200,"progress":0.375,"lastBookmarkModified":1690000000123})";
        TtuProgress progress;
        std::string error;
        QVERIFY2(parse_ttu_progress(json, progress, error), error.c_str());
        QCOMPARE(progress.data_id, 7);
        QCOMPARE(progress.explored_char_count, 1200);
        QCOMPARE(progress.progress, 0.375);
        QCOMPARE(progress.last_bookmark_modified, 1690000000123LL);
        const auto encoded = encode_ttu_progress(progress);
        QCOMPARE(QJsonDocument::fromJson(encoded), QJsonDocument::fromJson(json));
        QVERIFY(encoded.contains(R"("lastBookmarkModified":1690000000123)"));
        QVERIFY(!parse_ttu_progress(
            R"({"exploredCharCount":1200,"progress":0.375,"lastBookmarkModified":1690000000123})",
            progress, error));
    }

    void audio_book_json_round_trip() {
        const QByteArray json =
            R"({"title":"吾輩は猫である","playbackPosition":12.5,"lastAudioBookModified":1690000000123})";
        TtuAudioBook audio_book;
        std::string error;
        QVERIFY2(parse_ttu_audio_book(json, audio_book, error), error.c_str());
        QCOMPARE(audio_book.title, "吾輩は猫である");
        QCOMPARE(audio_book.playback_position, 12.5);
        QCOMPARE(audio_book.last_audio_book_modified, 1690000000123LL);
        const auto encoded = encode_ttu_audio_book(audio_book);
        QCOMPARE(QJsonDocument::fromJson(encoded), QJsonDocument::fromJson(json));
        QVERIFY(encoded.contains(R"("lastAudioBookModified":1690000000123)"));
        QVERIFY(!parse_ttu_audio_book(
            R"({"playbackPosition":12.5,"lastAudioBookModified":1690000000123})", audio_book,
            error));
    }

    void download_progress_data() {
        QTest::addColumn<int>("status");
        QTest::newRow("successful") << 200;
        QTest::newRow("not-found") << 404;
    }

    void download_progress() {
        QFETCH(int, status);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({status, status == 200 ? QByteArray("12345") : QByteArray{},
                        "application/octet-stream"});
        std::optional<DriveResponse> result;
        std::vector<double> progress;
        DriveTransport transport(server.base_url(), token, {});
        transport.download(
            "file-id", [&](double value) { progress.push_back(value); }, receive(result));
        QTRY_VERIFY(result.has_value());
        QCOMPARE(server.requests().size(), 1);
        const auto& request = server.requests().front();
        QCOMPARE(request.method, "GET");
        QCOMPARE(request.path, QStringLiteral("/drive/v3/files/file-id"));
        QCOMPARE(request.query.queryItemValue(QStringLiteral("alt")), QStringLiteral("media"));
        QCOMPARE(request.headers.value("authorization"), "Bearer token-a");
        if (status == 200) {
            QVERIFY(result->ok());
            QCOMPARE(*result->body, "12345");
            QVERIFY(!progress.empty());
            QVERIFY(std::ranges::all_of(progress,
                                        [](double value) { return value > 0 && value <= 1; }));
            QCOMPARE(progress.back(), 1.0);
        } else {
            QVERIFY(!result->ok());
            QVERIFY(result->error.has_value());
            QCOMPARE(result->error->kind, GoogleDriveErrorKind::api_error);
            QCOMPARE(result->error->status_code, std::optional{404});
            QVERIFY(progress.empty());
        }
    }

    void download_refreshes_401_once() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({401, {}});
        server.enqueue({200, QByteArray("12345"), "application/octet-stream"});
        int token_calls = 0;
        int refresh_calls = 0;
        std::optional<DriveResponse> result;
        std::vector<double> progress;
        DriveTransport transport(
            server.base_url(),
            [&] {
                ++token_calls;
                return DriveToken{"token-a", {}};
            },
            [&](DriveTransport::RefreshCallback cb) {
                ++refresh_calls;
                cb({"new-token", {}});
            });
        transport.download(
            "file-id", [&](double value) { progress.push_back(value); }, receive(result));
        QTRY_VERIFY(result.has_value());
        QVERIFY(result->ok());
        QCOMPARE(*result->body, QByteArray("12345"));
        QCOMPARE(token_calls, 1);
        QCOMPARE(refresh_calls, 1);
        QCOMPARE(server.requests().size(), 2);
        QCOMPARE(server.requests().back().headers.value("authorization"), "Bearer new-token");
        QVERIFY(!progress.empty());
        QCOMPARE(progress.back(), 1.0);
    }

    void offline_then_online() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({200, {}});
        std::optional<DriveResponse> result;
        DriveTransport transport(server.base_url(), token, {});
        transport.set_connectivity([] { return false; });
        get_files(transport, result);
        QTRY_VERIFY(result.has_value());
        QVERIFY(!result->ok());
        QVERIFY(result->error.has_value());
        QCOMPARE(result->error->kind, GoogleDriveErrorKind::unavailable);
        QCOMPARE(result->error->description(), "No Internet connection.");
        QVERIFY(server.requests().empty());
        std::optional<DriveResponse> download_result;
        std::vector<double> offline_progress;
        transport.download(
            "file-id", [&](double value) { offline_progress.push_back(value); },
            receive(download_result));
        QTRY_VERIFY(download_result.has_value());
        QVERIFY(!download_result->ok());
        QVERIFY(download_result->error.has_value());
        QCOMPARE(download_result->error->kind, GoogleDriveErrorKind::unavailable);
        QCOMPARE(download_result->error->description(), "No Internet connection.");
        QVERIFY(offline_progress.empty());
        QVERIFY(server.requests().empty());
        result.reset();
        transport.set_connectivity([] { return true; });
        get_files(transport, result);
        QTRY_VERIFY(result.has_value());
        QVERIFY(result->ok());
        QCOMPARE(server.requests().size(), 1);
    }

    void token_error_sends_nothing() {
        FakeDriveServer server;
        QVERIFY(server.start());
        std::optional<DriveResponse> result;
        DriveTransport transport(server.base_url(), [] { return DriveToken{{}, "Please sign in"}; },
                                 {});
        get_files(transport, result);
        QTRY_VERIFY(result.has_value());
        QVERIFY(!result->ok());
        QVERIFY(result->error.has_value());
        QCOMPARE(result->error->kind, GoogleDriveErrorKind::unavailable);
        QCOMPARE(result->error->description(), "Please sign in");
        QVERIFY(server.requests().empty());
    }

    void timeout_is_ten_seconds() {
        DriveTransport transport({}, token, {});
        QCOMPARE(transport.request_timeout().count(), 10000);
    }

    void multipart_envelope() {
        const QByteArray metadata = R"({"name":"cover_1_6.png","parents":["folder-1"]})";
        const QByteArray data = QByteArray::fromHex("89504e47");
        const auto multipart = build_multipart_related(metadata, "image/png", data, "BOUND");
        QCOMPARE(multipart.content_type, "multipart/related; boundary=BOUND");
        const QByteArray expected =
            "--BOUND\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n" + metadata +
            "\r\n--BOUND\r\nContent-Type: image/png\r\n\r\n" + data + "\r\n--BOUND--\r\n";
        QCOMPARE(multipart.body, expected);
        const auto automatic = build_multipart_related(metadata, "image/png", data);
        QVERIFY(QRegularExpression(QStringLiteral("^multipart/related; boundary=[0-9A-F]{32}$"))
                    .match(QString::fromStdString(automatic.content_type))
                    .hasMatch());
    }

    void network_error_without_status() {
        FakeDriveServer server;
        QVERIFY(server.start());
        const auto base_url = server.base_url();
        server.close();
        int refresh_calls = 0;
        std::optional<DriveResponse> result;
        DriveTransport transport(base_url, token, [&](DriveTransport::RefreshCallback cb) {
            ++refresh_calls;
            cb({"new-token", {}});
        });
        get_files(transport, result);
        QTRY_VERIFY(result.has_value());
        QVERIFY(!result->ok());
        QVERIFY(result->error.has_value());
        QCOMPARE(result->error->kind, GoogleDriveErrorKind::unavailable);
        QVERIFY(!result->error->status_code);
        QVERIFY(!result->error->description().empty());
        QCOMPARE(refresh_calls, 0);
    }

    void returns_success_body_unchanged_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("binary") << QByteArray::fromHex("0001ff7b7d");
        QTest::newRow("empty") << QByteArray{};
    }

    void returns_success_body_unchanged() {
        QFETCH(QByteArray, bytes);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({200, bytes, "application/octet-stream"});
        std::optional<DriveResponse> result;
        DriveTransport transport(server.base_url(), token, {});
        get_files(transport, result);
        QTRY_VERIFY(result.has_value());
        QVERIFY(result->ok());
        QCOMPARE(*result->body, bytes);
    }
};

QTEST_GUILESS_MAIN(DriveClientTest)
#include "tst_drive_client.moc"
