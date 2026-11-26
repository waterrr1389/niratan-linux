// SPDX-License-Identifier: GPL-3.0-or-later
// GoogleDriveHandler.swift and SyncManager.swift: TTU Drive I/O and sync rules.
// script/test_sasayaki_sync_contract.swift: import-only audio before synced return.

#include "fakes/ttu_fixture.h"
#include "services/statistics/statistics_storage.h"
#include "services/sync/ttu_naming.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include <algorithm>

using namespace niratan;
using namespace niratan::sync;
using namespace ttu_fixture;

namespace {

constexpr auto files_path = "/drive/v3/files";
constexpr auto upload_path = "/upload/drive/v3/files";
constexpr auto root_filter =
    "trashed=false and 'root' in parents and mimeType='application/vnd.google-apps.folder'"
    " and name = 'ttu-reader-data'";

DriveFile remote(std::string name) {
    return {"p", std::move(name), {}, {}};
}

const FakeDriveServer::Request* request_at(const FakeDriveServer& server, const QString& path,
                                           const QByteArray& method) {
    const auto& requests = server.requests();
    const auto it = std::ranges::find_if(requests, [&](const auto& request) {
        return request.path == path && request.method == method;
    });
    return it == requests.end() ? nullptr : &*it;
}

std::vector<const FakeDriveServer::Request*> uploads(const FakeDriveServer& server) {
    std::vector<const FakeDriveServer::Request*> out;
    for (const auto& request : server.requests()) {
        if (request.path.startsWith(upload_path)) {
            out.push_back(&request);
        }
    }
    return out;
}

statistics::Statistics row(std::string date, std::int64_t stamp, int characters) {
    return {"My Book", std::move(date), characters, 600.0, 7200, 7200, 7200, 7200, stamp};
}

}  // namespace

class TtuSyncTest : public QObject {
    Q_OBJECT

   private slots:
    void finds_and_caches_first_root() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue(
            {200,
             R"({"files":[{"id":"first","name":"ttu-reader-data"},{"id":"second","name":"ttu-reader-data"}]})"});
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        Result<std::string> root;
        drive.find_root_folder(root.receive());
        QTRY_VERIFY(root.done);
        QCOMPARE(root.value, std::optional{std::string("first")});
        QVERIFY(!root.error);
        QCOMPARE(server.requests().size(), 1);
        const auto& request = server.requests().front();
        QCOMPARE(request.method, "GET");
        QCOMPARE(request.path, QString::fromLatin1(files_path));
        QCOMPARE(query(request, "q"), QString::fromLatin1(root_filter));
        QCOMPARE(query(request, "fields"), QStringLiteral("files(id, name)"));
        root = {};
        drive.find_root_folder(root.receive());
        QTRY_VERIFY(root.done);
        QCOMPARE(root.value, std::optional{std::string("first")});
        QCOMPARE(server.requests().size(), 1);
    }

    void creates_root_or_rejects_missing_id_data() {
        QTest::addColumn<bool>("valid_id");
        QTest::newRow("created") << true;
        QTest::newRow("missing-id") << false;
    }

    void creates_root_or_rejects_missing_id() {
        QFETCH(bool, valid_id);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({200, R"({"files":[]})"});
        server.enqueue({200, valid_id ? QByteArray(R"({"id":"created"})") : QByteArray("{}")});
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        Result<std::string> root;
        drive.find_root_folder(root.receive());
        QTRY_VERIFY(root.done);
        QCOMPARE(server.requests().size(), 2);
        const auto& request = server.requests().back();
        QCOMPARE(request.method, "POST");
        QCOMPARE(request.path, QString::fromLatin1(files_path));
        QCOMPARE(query(request, "fields"), QStringLiteral("id"));
        QCOMPARE(
            QJsonDocument::fromJson(request.body),
            QJsonDocument::fromJson(
                R"({"name":"ttu-reader-data","mimeType":"application/vnd.google-apps.folder","parents":["root"]})"));
        if (valid_id) {
            QCOMPARE(root.value, std::optional{std::string("created")});
            root = {};
            drive.find_root_folder(root.receive());
            QTRY_VERIFY(root.done);
            QCOMPARE(root.value, std::optional{std::string("created")});
            QCOMPARE(server.requests().size(), 2);
        } else {
            QVERIFY(!root.value);
            QVERIFY(root.error);
            QCOMPARE(root.error->kind, GoogleDriveErrorKind::invalid_response);
        }
    }

    void lists_book_pages_in_order() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree(1);
        seed_folders(server);
        server.put_file("book-2", "Book Two", "root-id", {}, folder_mime);
        server.put_file("other", "Unrelated", "root", {}, folder_mime);
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        Result<std::vector<DriveFile>> books;
        drive.list_books("root-id", books.receive());
        QTRY_VERIFY(books.done);
        QVERIFY(books.value);
        QCOMPARE(books.value->size(), 2U);
        QCOMPARE(books.value->at(0).id, "book-id");
        QCOMPARE(books.value->at(1).id, "book-2");
        QCOMPARE(server.requests().size(), 2);
        QCOMPARE(query(server.requests().front(), "q"),
                 QStringLiteral("trashed=false and 'root-id' in parents and "
                                "mimeType='application/vnd.google-apps.folder'"));
        QCOMPARE(query(server.requests().front(), "fields"),
                 QStringLiteral("nextPageToken, files(id, name)"));
        QCOMPARE(query(server.requests().back(), "pageToken"), QStringLiteral("1"));
    }

    void groups_file_pages_in_chunks_of_fifty() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue(
            {200,
             R"({"files":[{"id":"ignore","name":"cover_1_6.png"},{"id":"p","name":"progress_first","parents":["0","1"]}],"nextPageToken":"next"})"});
        server.enqueue(
            {200,
             R"({"files":[{"id":"p2","name":"progress_second","parents":["0"]},{"id":"a","name":"audioBook_fixture","parents":["1"]}]})"});
        server.enqueue(
            {200, R"({"files":[{"id":"s","name":"statistics_first","parents":["50"]}]})"});
        server.enqueue({200, R"({"files":[]})"});
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        Result<TtuDrive::FolderFiles> result;
        drive.list_sync_files(std::vector<std::string>{}, result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(result.value);
        QVERIFY(result.value->empty());
        QVERIFY(server.requests().empty());
        std::vector<std::string> ids;
        QStringList parents;
        for (int i = 0; i < 51; ++i) {
            ids.push_back(std::to_string(i));
            if (i < 50) {
                parents.append(QStringLiteral("'%1' in parents").arg(i));
            }
        }
        result = {};
        drive.list_sync_files(ids, result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(result.value);
        QCOMPARE(result.value->size(), 3U);
        QVERIFY(result.value->contains("0"));
        QVERIFY(result.value->contains("1"));
        QVERIFY(result.value->contains("50"));
        QVERIFY(result.value->at("0").progress);
        QCOMPARE(result.value->at("0").progress->id, "p");
        QVERIFY(!result.value->at("0").cover);
        QVERIFY(!result.value->at("1").progress);
        QVERIFY(result.value->at("1").audio_book);
        QVERIFY(result.value->at("50").statistics);
        QCOMPARE(server.requests().size(), 3);
        QCOMPARE(query(server.requests().at(0), "q"),
                 "trashed=false and (" + parents.join(" or ") +
                     ") and mimeType != 'application/vnd.google-apps.folder'");
        QCOMPARE(query(server.requests().at(0), "fields"),
                 QStringLiteral("nextPageToken, files(id, name, parents, thumbnailLink)"));
        QCOMPARE(query(server.requests().at(1), "pageToken"), QStringLiteral("next"));
        QCOMPARE(query(server.requests().at(2), "q"),
                 QStringLiteral("trashed=false and ('50' in parents) and mimeType != "
                                "'application/vnd.google-apps.folder'"));
        Result<DriveSyncFiles> absent;
        drive.list_sync_files("absent", absent.receive());
        QTRY_VERIFY(absent.done);
        QVERIFY(absent.value);
        QVERIFY(!absent.value->book_data && !absent.value->progress && !absent.value->statistics &&
                !absent.value->cover && !absent.value->audio_book);
    }

    void downloads_and_trashes_a_file() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        server.put_file("zip", "bookdata_1_6.zip", "book-id", "PK", "application/zip");
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        Result<QByteArray> result;
        std::vector<double> progress;
        drive.download_file(
            "zip", [&](double value) { progress.push_back(value); }, result.receive());
        QTRY_VERIFY(result.done);
        QCOMPARE(result.value, std::optional{QByteArray("PK")});
        QVERIFY(!progress.empty());
        QCOMPARE(progress.back(), 1.0);
        QCOMPARE(query(server.requests().front(), "alt"), QStringLiteral("media"));
        QCOMPARE(server.requests().front().path, QStringLiteral("/drive/v3/files/zip"));
        Status trash;
        drive.trash_file("zip", trash.receive());
        QTRY_VERIFY(trash.done);
        QVERIFY(!trash.error);
        const auto& request = server.requests().back();
        QCOMPARE(request.method, "PATCH");
        QCOMPARE(request.path, QStringLiteral("/drive/v3/files/zip"));
        QCOMPARE(query(request, "fields"), QStringLiteral("id"));
        QCOMPARE(QJsonDocument::fromJson(request.body),
                 QJsonDocument::fromJson(R"({"trashed":true})"));
        result = {};
        drive.download_file("zip", {}, result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(result.error);
        QCOMPARE(result.error->status_code, std::optional{404});
    }

    void decodes_media_and_rejects_malformed_bodies_data() {
        QTest::addColumn<bool>("malformed");
        QTest::newRow("valid") << false;
        QTest::newRow("malformed") << true;
    }

    void decodes_media_and_rejects_malformed_bodies() {
        QFETCH(bool, malformed);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        server.put_file(
            "p", "progress_fixture", "book-id",
            malformed
                ? QByteArray("{}")
                : QByteArray(
                      R"({"dataId":7,"exploredCharCount":1200,"progress":0.375,"lastBookmarkModified":1690000000123})"));
        server.put_file("s", "statistics_fixture", "book-id",
                        malformed ? QByteArray("[{},42]") : QByteArray(stats_json));
        server.put_file(
            "a", "audioBook_fixture", "book-id",
            malformed
                ? QByteArray("{}")
                : QByteArray(
                      R"({"title":"吾輩は猫である","playbackPosition":12.5,"lastAudioBookModified":1690000000123})"));
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        Result<TtuProgress> progress;
        Result<std::vector<statistics::Statistics>> stats;
        Result<TtuAudioBook> audio;
        drive.get_progress_file("p", progress.receive());
        drive.get_stats_file("s", stats.receive());
        drive.get_audio_book_file("a", audio.receive());
        QTRY_VERIFY(progress.done && stats.done && audio.done);
        QCOMPARE(server.requests().size(), 3);
        for (const auto& request : server.requests()) {
            QCOMPARE(request.method, "GET");
            QCOMPARE(query(request, "alt"), QStringLiteral("media"));
        }
        if (malformed) {
            QVERIFY(!progress.value && !stats.value && !audio.value);
            for (const auto& error : {progress.error, stats.error, audio.error}) {
                QVERIFY(error);
                QCOMPARE(error->kind, GoogleDriveErrorKind::invalid_response);
            }
        } else {
            QVERIFY(progress.value && stats.value && audio.value);
            QCOMPARE(progress.value->data_id, 7);
            QCOMPARE(progress.value->explored_char_count, 1200);
            QCOMPARE(progress.value->progress, 0.375);
            QCOMPARE(progress.value->last_bookmark_modified, 1690000000123LL);
            QCOMPARE(*stats.value, (std::vector{row("2023-07-22", 1690000000123, 1200)}));
            QCOMPARE(audio.value->title, "吾輩は猫である");
            QCOMPARE(audio.value->playback_position, 12.5);
            QCOMPARE(audio.value->last_audio_book_modified, 1690000000123LL);
        }
    }

    void writes_progress_statistics_and_audio_multipart_data() {
        QTest::addColumn<bool>("update");
        QTest::newRow("post") << false;
        QTest::newRow("patch") << true;
    }

    void writes_progress_statistics_and_audio_multipart() {
        QFETCH(bool, update);
        FakeDriveServer server;
        QVERIFY(server.start());
        for (int i = 0; i < 3; ++i) {
            server.enqueue({200, "{}"});
        }
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        const auto id = update ? std::optional{std::string("existing")} : std::nullopt;
        const auto rows = std::vector{row("2023-07-22", 1690000000123, 1200)};
        const auto check = [&](qsizetype index, const QString& name, const QByteArray& payload) {
            QVERIFY(server.requests().size() > index);
            const auto& request = server.requests().at(index);
            QCOMPARE(request.method, update ? "PATCH" : "POST");
            QCOMPARE(request.path, QString::fromLatin1(upload_path) + (update ? "/existing" : ""));
            QCOMPARE(query(request, "uploadType"), QStringLiteral("multipart"));
            auto expected = QJsonObject{{"name", name}};
            if (!update) {
                expected.insert("parents", QJsonArray{"book-id"});
            }
            QCOMPARE(request.metadata, expected);
            QCOMPARE(request.media_type, "application/json");
            QCOMPARE(QJsonDocument::fromJson(request.media), QJsonDocument::fromJson(payload));
        };
        Status result;
        drive.update_progress_file("book-id", id, {7, 1200, 1.0, 1690000000123}, result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(!result.error);
        check(
            0, "progress_1_6_1690000000123_1.0.json",
            R"({"dataId":7,"exploredCharCount":1200,"progress":1.0,"lastBookmarkModified":1690000000123})");
        result = {};
        drive.update_stats_file("book-id", id, rows, result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(!result.error);
        check(1, QString::fromStdString(ttu_statistics_file_name(rows)), stats_json);
        result = {};
        drive.update_audio_book_file("book-id", id, {"My Book", 42.5, 1690000001000},
                                     result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(!result.error);
        check(2, "audioBook_1_6_1690000001000_42.5.json", audio_json);
    }

    void finds_existing_book_folder_without_cover_upload() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({200, R"({"files":[{"id":"book-id","name":"吾輩%2F猫~ttu-dend~"}]})"});
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        int reads = 0;
        const auto cover = TtuDrive::CoverProvider{[&] {
            ++reads;
            return QByteArray("cover");
        }};
        Result<std::string> folder;
        for (int i = 0; i < 2; ++i) {
            folder = {};
            drive.ensure_book_folder("吾輩/猫.", "root-id", cover, folder.receive());
            QTRY_VERIFY(folder.done);
            QCOMPARE(folder.value, std::optional{std::string("book-id")});
        }
        QCOMPARE(reads, 0);
        QCOMPARE(server.requests().size(), 1);
        QCOMPARE(
            query(server.requests().front(), "q"),
            QStringLiteral(
                "trashed=false and 'root-id' in parents and "
                "mimeType='application/vnd.google-apps.folder' and name=\"吾輩%2F猫~ttu-dend~\""));
        QCOMPARE(query(server.requests().front(), "fields"), QStringLiteral("files(id, name)"));
    }

    void creates_book_folder_without_cover() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({200, R"({"files":[]})"});
        server.enqueue({200, R"({"id":"new-book"})"});
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        Result<std::string> folder;
        drive.ensure_book_folder("吾輩/猫.", "root-id", {}, folder.receive());
        QTRY_VERIFY(folder.done);
        QCOMPARE(folder.value, std::optional{std::string("new-book")});
        QCOMPARE(server.requests().size(), 2);
        QCOMPARE(server.requests().at(1).method, "POST");
        QCOMPARE(server.requests().at(1).path, QString::fromLatin1(files_path));
        QVERIFY(uploads(server).empty());
    }

    void creates_book_folder_with_cover_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<QString>("extension");
        QTest::addColumn<bool>("upload_fails");
        QTest::newRow("png") << QByteArray::fromHex("89504e47") << QString("png") << false;
        QTest::newRow("gif") << QByteArray("GIF8") << QString("gif") << false;
        QTest::newRow("bmp") << QByteArray("BM12") << QString("bmp") << false;
        QTest::newRow("webp") << QByteArray("RIFF") << QString("webp") << false;
        QTest::newRow("short") << QByteArray("BM") << QString("jpeg") << false;
        QTest::newRow("unrecognized") << QByteArray("other") << QString("jpeg") << true;
    }

    void creates_book_folder_with_cover() {
        QFETCH(QByteArray, bytes);
        QFETCH(QString, extension);
        QFETCH(bool, upload_fails);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enqueue({200, R"({"files":[]})"});
        server.enqueue({200, R"({"id":"new-book"})"});
        server.enqueue({upload_fails ? 500 : 200, "{}"});
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        Result<std::string> folder;
        drive.ensure_book_folder("吾輩/猫.", "root-id", {[&] { return bytes; }}, folder.receive());
        QTRY_VERIFY(folder.done);
        QCOMPARE(folder.value, std::optional{std::string("new-book")});
        QVERIFY(!folder.error);
        QCOMPARE(server.requests().size(), 3);
        const auto& create = server.requests().at(1);
        QCOMPARE(create.method, "POST");
        QCOMPARE(create.path, QString::fromLatin1(files_path));
        QVERIFY(create.query.isEmpty());
        QCOMPARE(
            QJsonDocument::fromJson(create.body),
            QJsonDocument::fromJson(
                R"({"name":"吾輩%2F猫~ttu-dend~","mimeType":"application/vnd.google-apps.folder","parents":["root-id"]})"));
        const auto& cover = server.requests().back();
        QCOMPARE(cover.method, "POST");
        QCOMPARE(cover.path, QString::fromLatin1(upload_path));
        QCOMPARE(query(cover, "uploadType"), QStringLiteral("multipart"));
        QCOMPARE(
            cover.metadata,
            (QJsonObject{{"name", "cover_1_6." + extension}, {"parents", QJsonArray{"new-book"}}}));
        QCOMPARE(cover.media_type, "image/" + extension.toLatin1());
        QCOMPARE(cover.media, bytes);
    }

    void direction_and_filename_timestamps() {
        const auto file = remote("progress_1_6_1690000000000_0.25.json");
        QCOMPARE(parse_progress_timestamp(file.name), std::optional{remote_ms});
        QCOMPARE(parse_progress_timestamp("progress__1_6_+1690000000000_0.json"),
                 std::optional{remote_ms});
        QVERIFY(!parse_progress_timestamp("progress_1_6_1690000000000"));
        QVERIFY(!parse_progress_timestamp("progress_1_6_1690000000000bad_0.json"));
        QVERIFY(!parse_progress_timestamp("other_1_6_1690000000000_0.json"));
        books::Bookmark local;
        QCOMPARE(determine_direction(std::nullopt, std::nullopt), SyncDirection::synced);
        QCOMPARE(determine_direction(local, file), SyncDirection::import_from_ttu);
        local.last_modified = remote_ms / 1000.0 - unix_reference;
        QCOMPARE(determine_direction(local, std::nullopt), SyncDirection::export_to_ttu);
        QCOMPARE(determine_direction(local, file), SyncDirection::synced);
        *local.last_modified += 0.0005;
        QCOMPARE(determine_direction(local, file), SyncDirection::export_to_ttu);
        *local.last_modified -= 0.001;
        QCOMPARE(determine_direction(local, file), SyncDirection::import_from_ttu);
    }

    void audio_timestamp_and_mtime() {
        const auto file = remote("audioBook_1_6_1690000001000_42.5.json");
        const std::int64_t timestamp = remote_ms + 1000;
        QCOMPARE(parse_audio_book_timestamp(file.name), std::optional{timestamp});
        QCOMPARE(parse_audio_book_timestamp("audioBook_1_6_1690000001000"),
                 std::optional{timestamp});
        QVERIFY(!parse_audio_book_timestamp("audiobook_1_6_1690000001000_42.5.json"));
        QVERIFY(!should_import_audio_book(std::nullopt, std::nullopt));
        QVERIFY(!should_import_audio_book(remote("audioBook_wrong"), std::nullopt));
        QVERIFY(should_import_audio_book(file, std::nullopt));
        QVERIFY(should_import_audio_book(file, timestamp - 1));
        QVERIFY(!should_import_audio_book(file, timestamp));
        QVERIFY(!should_import_audio_book(file, timestamp + 1));
    }

    void merge_statistics_uses_strictly_newer_date_keys() {
        using enum statistics::StatisticsSyncMode;
        const auto local = std::vector{row("a", 10, 1), row("b", 10, 2), row("c", 10, 3)};
        const auto incoming =
            std::vector{row("d", 10, 4), row("b", 10, 10), row("c", 11, 11), row("a", 9, 9)};
        QCOMPARE(merge_statistics(local, incoming, replace), incoming);
        auto merged = merge_statistics(local, incoming, merge);
        std::ranges::sort(merged, {}, &statistics::Statistics::date_key);
        QCOMPARE(merged, (std::vector{local[0], local[1], incoming[2], incoming[0]}));
    }

    void retries_404_with_both_caches_cleared_data() {
        QTest::addColumn<bool>("fails_again");
        QTest::newRow("recovers") << false;
        QTest::newRow("fails-twice") << true;
    }

    void retries_404_with_both_caches_cleared() {
        QFETCH(bool, fails_again);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        Library library;
        QVERIFY(library.start());
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        Result<std::string> root;
        drive.find_root_folder(root.receive());
        QTRY_VERIFY(root.done);
        QVERIFY(root.value);
        Result<std::string> folder;
        drive.ensure_book_folder(library.book.title, *root.value, {}, folder.receive());
        QTRY_VERIFY(folder.done);
        QCOMPARE(folder.value, std::optional{std::string("book-id")});
        server.put_file("root-id", "old-root", "root", {}, folder_mime);
        server.put_file("new-root", "ttu-reader-data", "root", {}, folder_mime);
        server.put_file("new-book", "My Book", "new-root", {}, folder_mime);
        server.enqueue({404, {}});
        if (fails_again) {
            server.enqueue({200, R"({"files":[{"id":"new-root","name":"ttu-reader-data"}]})"});
            server.enqueue({200, R"({"files":[{"id":"new-book","name":"My Book"}]})"});
            server.enqueue({404, {}});
        }
        TtuSync sync(drive, library.storage, {}, 0, QTimeZone::utc());
        Result<TtuResult> result;
        sync.sync_book(library.book, {}, {}, result.receive());
        QTRY_VERIFY(result.done);
        QCOMPARE(server.requests().size(), 6);
        QVERIFY(query(server.requests().at(2), "q").contains("'book-id' in parents"));
        QCOMPARE(query(server.requests().at(3), "q"), QString::fromLatin1(root_filter));
        QVERIFY(query(server.requests().at(4), "q").contains("'new-root' in parents"));
        QVERIFY(query(server.requests().at(5), "q").contains("'new-book' in parents"));
        if (fails_again) {
            QVERIFY(!result.value);
            QVERIFY(result.error);
            QCOMPARE(result.error->status_code, std::optional{404});
        } else {
            QCOMPARE(result.value,
                     (std::optional{TtuResult{TtuResult::Kind::synced, "Display Title", 0}}));
            QVERIFY(!result.error);
        }
    }

    void sync_book_data_before_automatic_direction_data() {
        QTest::addColumn<QString>("mode");
        for (const auto* mode : {"upload", "no-output", "no-converter", "explicit-import",
                                 "import-only", "already-present"}) {
            QTest::newRow(mode) << QString::fromLatin1(mode);
        }
    }

    void sync_book_data_before_automatic_direction() {
        QFETCH(QString, mode);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        seed_media(server);
        if (mode == "already-present") {
            server.put_file("zip", "bookdata_1_6_fixture.zip", "book-id");
        }
        Library library;
        QVERIFY(library.start());
        QVERIFY(library.bookmark(10, remote_ms - 1000));
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        int conversions = 0;
        std::filesystem::path temporary_zip;
        BookDataConverter converter;
        if (mode != "no-converter") {
            converter.export_zip = [&](const auto& folder,
                                       const auto& output) -> std::optional<std::filesystem::path> {
                ++conversions;
                if (folder != library.path() || mode == "no-output") {
                    return std::nullopt;
                }
                temporary_zip = output / "bookdata_1_6_1_1690000000000.zip";
                return write(temporary_zip, "PK") ? std::optional{temporary_zip} : std::nullopt;
            };
        }
        TtuSync sync(drive, library.storage, std::move(converter), 0, QTimeZone::utc());
        Result<TtuResult> result;
        const auto direction = mode == "explicit-import"
            ? std::optional{SyncDirection::import_from_ttu}
            : std::nullopt;
        sync.sync_book(library.book, direction,
                       {.sync_book_data = true, .import_only = mode == "import-only"},
                       result.receive());
        QTRY_VERIFY(result.done);
        QCOMPARE(result.value,
                 (std::optional{TtuResult{TtuResult::Kind::imported, "Display Title", 250}}));
        QCOMPARE(conversions, mode == "upload" || mode == "no-output" ? 1 : 0);
        const auto writes = uploads(server);
        QCOMPARE(writes.size(), mode == "upload" ? 1U : 0U);
        if (mode == "upload") {
            QCOMPARE(writes.front()->method, "POST");
            QCOMPARE(writes.front()->path, QString::fromLatin1(upload_path));
            QCOMPARE(query(*writes.front(), "uploadType"), QStringLiteral("multipart"));
            QCOMPARE(writes.front()->metadata,
                     (QJsonObject{{"name", "bookdata_1_6_1_1690000000000.zip"},
                                  {"parents", QJsonArray{"book-id"}}}));
            QCOMPARE(writes.front()->media_type, "application/zip");
            QCOMPARE(writes.front()->media, "PK");
            const auto* read_progress = request_at(server, "/drive/v3/files/p", "GET");
            QVERIFY(read_progress);
            QVERIFY(writes.front() < read_progress);
            QVERIFY(!std::filesystem::exists(temporary_zip));
        }
    }

    void import_only_audio_or_skip_before_ordinary_sync_data() {
        QTest::addColumn<QString>("mode");
        QTest::newRow("audio-disabled") << QStringLiteral("audio-disabled");
        for (const auto* mode :
             {"audio-newer", "audio-older", "missing-audio", "export-skipped", "normal-synced"}) {
            QTest::newRow(mode) << QString::fromLatin1(mode);
        }
    }

    void import_only_audio_or_skip_before_ordinary_sync() {
        QFETCH(QString, mode);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        server.put_file("p", "progress_1_6_1690000000000_0.25.json", "book-id", progress_json);
        if (mode != "missing-audio") {
            seed_media(server);
        }
        Library library;
        QVERIFY(library.start());
        QVERIFY(library.bookmark(250, remote_ms + (mode == "export-skipped" ? 2000 : 0)));
        const sasayaki::SasayakiPlaybackData local{2.0, 1.25, 1.5F, QByteArray("local-bookmark")};
        QVERIFY(library.storage.save_sasayaki_playback(library.book.folder, local));
        QFile sidecar(QString::fromStdString((library.path() / "sasayaki_playback.json").string()));
        QVERIFY(sidecar.open(QIODevice::ReadOnly));
        QVERIFY(sidecar.setFileTime(
            QDateTime::fromMSecsSinceEpoch(remote_ms + (mode == "audio-older" ? 1001 : 999),
                                           QTimeZone::utc()),
            QFileDevice::FileModificationTime));
        sidecar.close();
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        TtuSync sync(drive, library.storage, {}, 0, QTimeZone::utc());
        Result<TtuResult> result;
        sync.sync_book(
            library.book, {},
            {.sync_audio_book = mode != "audio-disabled", .import_only = mode != "normal-synced"},
            result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(result.value);
        QVERIFY(!result.error);
        auto expected = local;
        if (mode == "audio-newer") {
            QCOMPARE(*result.value, (TtuResult{TtuResult::Kind::imported, "Display Title", 250}));
            QVERIFY(request_at(server, "/drive/v3/files/a", "GET"));
            expected.last_position = 42.5;
        } else if (mode == "export-skipped") {
            QCOMPARE(result.value->kind, TtuResult::Kind::skipped);
        } else {
            QCOMPARE(*result.value, (TtuResult{TtuResult::Kind::synced, "Display Title", 0}));
        }
        if (mode == "audio-older" || mode == "audio-disabled") {
            QVERIFY(!request_at(server, "/drive/v3/files/a", "GET"));
        }
        QCOMPARE(library.storage.load_sasayaki_playback(library.book.folder),
                 std::optional{expected});
        QVERIFY(!request_at(server, "/drive/v3/files/p", "GET"));
        QVERIFY(uploads(server).empty());
    }

    void import_progress_without_book_info_or_with_no_matching_chapter_data() {
        QTest::addColumn<QString>("mode");
        for (const auto* mode : {"missing-progress", "missing-info", "missing-chapter"}) {
            QTest::newRow(mode) << QString::fromLatin1(mode);
        }
    }

    void import_progress_without_book_info_or_with_no_matching_chapter() {
        QFETCH(QString, mode);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        Library library;
        if (mode == "missing-chapter") {
            library.info.chapter_info.clear();
        }
        QVERIFY(library.start());
        if (mode == "missing-info") {
            QVERIFY(std::filesystem::remove(library.path() / "bookinfo.json"));
        }
        if (mode != "missing-progress") {
            seed_media(server);
            server.put_file("s", "statistics_empty.json", "book-id", "[]");
        }
        const auto existing = row("2023-07-21", remote_ms - 1000, 321);
        QVERIFY(statistics::import_daily_statistics(library.path(), {existing},
                                                    statistics::StatisticsSyncMode::merge, 0,
                                                    QTimeZone::utc()));
        const auto before = statistics::load_statistics(library.path(), 0, QTimeZone::utc()).daily;
        QVERIFY(!before.empty());
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        TtuSync sync(drive, library.storage, {}, 0, QTimeZone::utc());
        Result<TtuResult> result;
        sync.sync_book(library.book, SyncDirection::import_from_ttu,
                       {.sync_stats = true, .stats_mode = statistics::StatisticsSyncMode::replace},
                       result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(result.value);
        QVERIFY(!result.error);
        const auto bookmark = library.storage.load_bookmark(library.book.folder);
        if (mode == "missing-progress") {
            QCOMPARE(result.value->kind, TtuResult::Kind::skipped);
        } else {
            QCOMPARE(*result.value, (TtuResult{TtuResult::Kind::imported, "Display Title", 250}));
        }
        if (mode == "missing-chapter") {
            QCOMPARE(
                bookmark,
                (std::optional{books::Bookmark{0, 0.0, 250, remote_ms / 1000.0 - unix_reference}}));
        } else {
            QVERIFY(!bookmark);
        }
        QCOMPARE(statistics::load_statistics(library.path(), 0, QTimeZone::utc()).daily, before);
    }

    void import_statistics_mode_applies_data() {
        QTest::addColumn<bool>("replace");
        QTest::newRow("merge") << false;
        QTest::newRow("replace") << true;
    }

    void import_statistics_mode_applies() {
        QFETCH(bool, replace);
        Library library;
        QVERIFY(library.start());
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        seed_media(server);
        constexpr auto merge = statistics::StatisticsSyncMode::merge;
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        TtuSync sync(drive, library.storage, {}, 0, QTimeZone::utc());
        QVERIFY(statistics::import_daily_statistics(library.path(),
                                                    {row("2023-07-22", remote_ms + 1000, 999)},
                                                    merge, 0, QTimeZone::utc()));
        Result<TtuResult> result;
        sync.sync_book(library.book, SyncDirection::import_from_ttu,
                       {.sync_stats = true,
                        .stats_mode = replace ? statistics::StatisticsSyncMode::replace : merge},
                       result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(result.value && !result.error);
        QCOMPARE(result.value->kind, TtuResult::Kind::imported);
        const auto rows = statistics::load_statistics(library.path(), 0, QTimeZone::utc()).daily;
        const auto day = std::ranges::find(rows, "2023-07-22", &statistics::Statistics::date_key);
        QVERIFY(day != rows.end());
        QCOMPARE(day->characters_read, replace ? 1200 : 999);
    }

    void export_without_statistics_or_audio_sync() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        seed_media(server);
        Library library;
        QVERIFY(library.start());
        QVERIFY(library.bookmark(600, remote_ms + 2000));
        QVERIFY(statistics::import_daily_statistics(
            library.path(), {row("2023-07-21", remote_ms - 1000, 321)},
            statistics::StatisticsSyncMode::merge, 0, QTimeZone::utc()));
        QVERIFY(library.storage.save_sasayaki_playback(library.book.folder,
                                                       {12.5, 3.0, 1.25F, QByteArray("local")}));
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        TtuSync sync(drive, library.storage, {}, 0, QTimeZone::utc());
        Result<TtuResult> result;
        sync.sync_book(library.book, {}, {.sync_stats = false, .sync_audio_book = false},
                       result.receive());
        QTRY_VERIFY(result.done);
        QCOMPARE(result.value,
                 (std::optional{TtuResult{TtuResult::Kind::exported, "Display Title", 600}}));
        const auto* progress = request_at(server, "/upload/drive/v3/files/p", "PATCH");
        QVERIFY(progress);
        QCOMPARE(progress->metadata.value("name").toString(),
                 QStringLiteral("progress_1_6_1690000002000_0.6.json"));
        QVERIFY(!request_at(server, "/drive/v3/files/s", "GET"));
        QVERIFY(!request_at(server, "/drive/v3/files/a", "GET"));
        QCOMPARE(uploads(server).size(), 1U);
    }

    void exports_progress_stats_and_local_audio() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        seed_media(server);
        Library library;
        QVERIFY(library.start());
        const auto modified = remote_ms / 1000.0 - unix_reference + 10.12375;
        QVERIFY(library.storage.save_bookmark(library.book.folder, {1, 0.125, 500, modified}));
        QVERIFY(library.storage.save_sasayaki_playback(library.book.folder,
                                                       {12.5, 3.0, 1.25F, QByteArray("local")}));
        QVERIFY(statistics::import_daily_statistics(
            library.path(), {row("2023-07-21", remote_ms - 1000, 321)},
            statistics::StatisticsSyncMode::merge, 0, QTimeZone::utc()));
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        TtuSync sync(drive, library.storage, {}, 0, QTimeZone::utc());
        const auto before = QDateTime::currentMSecsSinceEpoch();
        Result<TtuResult> result;
        sync.sync_book(library.book, {}, {.sync_stats = true, .sync_audio_book = true},
                       result.receive());
        QTRY_VERIFY(result.done);
        QCOMPARE(result.value,
                 (std::optional{TtuResult{TtuResult::Kind::exported, "Display Title", 500}}));
        const auto expected_ms = remote_ms + 10123;
        const auto* progress = request_at(server, "/upload/drive/v3/files/p", "PATCH");
        QVERIFY(progress);
        QCOMPARE(progress->metadata.value("name").toString(),
                 QStringLiteral("progress_1_6_1690000010123_0.5.json"));
        QCOMPARE(
            QJsonDocument::fromJson(progress->media),
            QJsonDocument::fromJson(
                R"({"dataId":7,"exploredCharCount":500,"progress":0.5,"lastBookmarkModified":1690000010123})"));
        QCOMPARE(
            library.storage.load_bookmark(library.book.folder),
            (std::optional{books::Bookmark{1, 0.125, 500, expected_ms / 1000.0 - unix_reference}}));
        const auto* stats = request_at(server, "/upload/drive/v3/files/s", "PATCH");
        QVERIFY(stats);
        const auto rows = QJsonDocument::fromJson(stats->media).array();
        QCOMPARE(rows.size(), 2);
        QStringList dates;
        for (const auto item : rows) {
            dates.append(item.toObject().value("dateKey").toString());
        }
        QVERIFY(dates.contains("2023-07-21") && dates.contains("2023-07-22"));
        const auto* audio = request_at(server, "/upload/drive/v3/files/a", "PATCH");
        QVERIFY(audio);
        const auto audiobook = QJsonDocument::fromJson(audio->media).object();
        QCOMPARE(audiobook.size(), 3);
        QCOMPARE(audiobook.value("title").toString(), QStringLiteral("My Book"));
        QCOMPARE(audiobook.value("playbackPosition").toDouble(), 12.5);
        const auto stamp = audiobook.value("lastAudioBookModified").toInteger();
        QVERIFY(stamp >= before && stamp <= QDateTime::currentMSecsSinceEpoch());
        QCOMPARE(audio->metadata.value("name").toString(),
                 QStringLiteral("audioBook_1_6_%1_12.5.json").arg(stamp));
    }

    void export_statistics_mode_is_applied_data() {
        QTest::addColumn<bool>("replace");
        QTest::newRow("merge") << false;
        QTest::newRow("replace") << true;
    }

    // B7: Replace replaces the remote file with the local rows unchanged;
    // Merge keeps the newer row per date key and adds a local day the remote
    // does not have.
    void export_statistics_mode_is_applied() {
        QFETCH(bool, replace);
        constexpr auto merge = statistics::StatisticsSyncMode::merge;
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        seed_media(server);
        Library library;
        QVERIFY(library.start());
        QVERIFY(statistics::import_daily_statistics(
            library.path(),
            {row("2023-07-22", remote_ms - 1000, 999), row("2023-07-23", remote_ms - 1000, 111)},
            merge, 0, QTimeZone::utc()));
        QVERIFY(library.bookmark(600, remote_ms + 2000));
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        TtuSync sync(drive, library.storage, {}, 0, QTimeZone::utc());
        Result<TtuResult> result;
        sync.sync_book(library.book, SyncDirection::export_to_ttu,
                       {.sync_stats = true,
                        .stats_mode = replace ? statistics::StatisticsSyncMode::replace : merge},
                       result.receive());
        QTRY_VERIFY(result.done);
        QCOMPARE(result.value,
                 (std::optional{TtuResult{TtuResult::Kind::exported, "Display Title", 600}}));
        const auto* stats = request_at(server, "/upload/drive/v3/files/s", "PATCH");
        QVERIFY(stats);
        const auto rows = QJsonDocument::fromJson(stats->media).array();
        QMap<QString, int> characters;
        for (const auto item : rows) {
            const auto object = item.toObject();
            characters.insert(object.value("dateKey").toString(),
                              object.value("charactersRead").toInt());
        }
        if (replace) {
            QCOMPARE(characters, (QMap<QString, int>{{"2023-07-22", 999}, {"2023-07-23", 111}}));
        } else {
            QCOMPARE(characters, (QMap<QString, int>{{"2023-07-22", 1200}, {"2023-07-23", 111}}));
        }
    }

    void no_spurious_exports_when_sidecars_are_missing_data() {
        QTest::addColumn<QString>("mode");
        for (const auto* mode : {"no-bookmark", "no-date", "no-info", "zero-length", "valid"}) {
            QTest::newRow(mode) << QString::fromLatin1(mode);
        }
    }

    void no_spurious_exports_when_sidecars_are_missing() {
        QFETCH(QString, mode);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        Library library;
        if (mode == "zero-length") {
            library.info.character_count = 0;
        }
        QVERIFY(library.start());
        if (mode == "no-info") {
            QVERIFY(std::filesystem::remove(library.path() / "bookinfo.json"));
        }
        if (mode == "no-date") {
            QVERIFY(library.storage.save_bookmark(library.book.folder, {0, 0.125, 500, {}}));
        } else if (mode != "no-bookmark") {
            QVERIFY(library.bookmark(500, remote_ms));
        }
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        TtuSync sync(drive, library.storage, {}, 0, QTimeZone::utc());
        Result<TtuResult> result;
        sync.sync_book(library.book, SyncDirection::export_to_ttu,
                       {.sync_stats = true, .sync_audio_book = true}, result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(result.value);
        QVERIFY(!result.error);
        QCOMPARE(result.value->kind,
                 mode == "no-bookmark" ? TtuResult::Kind::skipped : TtuResult::Kind::exported);
        const auto writes = uploads(server);
        if (mode == "zero-length" || mode == "valid") {
            QCOMPARE(writes.size(), 1U);
            const auto json = QJsonDocument::fromJson(writes.front()->media).object();
            QCOMPARE(json.value("dataId").toInt(-1), 0);
            QCOMPARE(json.value("progress").toDouble(-1), mode == "zero-length" ? 0.0 : 0.5);
        } else {
            QVERIFY(writes.empty());
        }
    }

    void imports_remote_book_without_statistics_or_audio_sync() {
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        seed_media(server);
        server.put_file("zip", "bookdata_1_6_fixture.zip", "book-id", "PK", "application/zip");
        Library library;
        QVERIFY(library.root.isValid());
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        BookDataConverter converter;
        converter.import_zip = [&](const auto&,
                                   const auto&) -> std::optional<std::filesystem::path> {
            return library.start() ? std::optional{library.path()} : std::nullopt;
        };
        TtuSync sync(drive, library.storage, std::move(converter), 0, QTimeZone::utc());
        std::vector<DriveFile> files{{"zip", "bookdata_1_6_fixture.zip", {}, {}},
                                     {"p", "progress_fixture", {}, {}},
                                     {"s", "statistics_fixture", {}, {}},
                                     {"a", "audioBook_fixture", {}, {}}};
        Result<std::filesystem::path> result;
        sync.import_google_drive_book(DriveSyncFiles(std::move(files)), false, false, {},
                                      result.receive());
        QTRY_VERIFY(result.done);
        QVERIFY(!result.error);
        QCOMPARE(result.value, std::optional{library.path()});
        QCOMPARE(server.requests().size(), 2);
        QVERIFY(request_at(server, "/drive/v3/files/zip", "GET"));
        QVERIFY(request_at(server, "/drive/v3/files/p", "GET"));
        QVERIFY(!request_at(server, "/drive/v3/files/s", "GET"));
        QVERIFY(!request_at(server, "/drive/v3/files/a", "GET"));
        const auto bookmark = library.storage.load_bookmark(library.book.folder);
        QVERIFY(bookmark);
        QCOMPARE(bookmark->character_count, 250);
        QVERIFY(statistics::load_statistics(library.path(), 0, QTimeZone::utc()).daily.empty());
        QVERIFY(!library.storage.load_sasayaki_playback(library.book.folder));
    }

    void imports_remote_book_via_converter_data() {
        QTest::addColumn<QString>("mode");
        for (const auto* mode : {"success", "no-book-data", "no-converter", "no-output"}) {
            QTest::newRow(mode) << QString::fromLatin1(mode);
        }
    }

    void imports_remote_book_via_converter() {
        QFETCH(QString, mode);
        FakeDriveServer server;
        QVERIFY(server.start());
        server.enable_tree();
        seed_folders(server);
        seed_media(server);
        server.put_file("zip", "bookdata_1_6_fixture.zip", "book-id", "PK", "application/zip");
        Library library;
        QVERIFY(library.root.isValid());
        auto transport = transport_for(server);
        TtuDrive drive(*transport);
        QByteArray zip_bytes;
        std::filesystem::path zip_path;
        std::filesystem::path destination;
        BookDataConverter converter;
        if (mode != "no-converter") {
            converter.import_zip =
                [&](const auto& zip, const auto& book_dir) -> std::optional<std::filesystem::path> {
                zip_path = zip;
                destination = book_dir;
                QFile file(QString::fromStdString(zip.string()));
                if (file.open(QIODevice::ReadOnly)) {
                    zip_bytes = file.readAll();
                }
                if (mode == "no-output" || !library.start()) {
                    return std::nullopt;
                }
                return library.path();
            };
        }
        TtuSync sync(drive, library.storage, std::move(converter), 0, QTimeZone::utc());
        auto files = std::vector<DriveFile>{remote("progress_fixture"),
                                            {"s", "statistics_fixture", {}, {}},
                                            {"a", "audioBook_fixture", {}, {}}};
        if (mode != "no-book-data") {
            files.push_back({"zip", "bookdata_1_6_fixture.zip", {}, {}});
        }
        Result<std::filesystem::path> result;
        std::vector<double> progress;
        sync.import_google_drive_book(
            DriveSyncFiles(std::move(files)), true, true,
            [&](double value) { progress.push_back(value); }, result.receive());
        QTRY_VERIFY(result.done);
        if (mode == "no-book-data") {
            QVERIFY(!result.value);
            QVERIFY(result.error);
            QCOMPARE(result.error->kind, GoogleDriveErrorKind::invalid_response);
            QVERIFY(server.requests().empty());
        } else if (mode == "no-converter") {
            QVERIFY(!result.value);
            QVERIFY(result.error);
            QCOMPARE(result.error->kind, GoogleDriveErrorKind::unavailable);
            QCOMPARE(result.error->description(), "Book data conversion is not configured");
            QVERIFY(server.requests().empty());
        } else {
            QCOMPARE(zip_bytes, "PK");
            QCOMPARE(destination, library.storage.books_directory());
            QVERIFY(!std::filesystem::exists(zip_path));
            QVERIFY(!progress.empty());
            QCOMPARE(progress.back(), 1.0);
            if (mode == "no-output") {
                QVERIFY(!result.value);
                QVERIFY(result.error);
                QCOMPARE(result.error->kind, GoogleDriveErrorKind::unavailable);
                QCOMPARE(result.error->description(), "Could not convert the downloaded book data");
                return;
            }
            QVERIFY(!result.error);
            QCOMPARE(result.value, std::optional{library.path()});
            QCOMPARE(server.requests().size(), 4);
            const auto bookmark = library.storage.load_bookmark(library.book.folder);
            QVERIFY(bookmark);
            QCOMPARE(bookmark->chapter_index, 1);
            QCOMPARE(bookmark->character_count, 250);
            const auto audio = library.storage.load_sasayaki_playback(library.book.folder);
            QCOMPARE(audio, (std::optional{sasayaki::SasayakiPlaybackData{42.5, 0.0, 1.0F, {}}}));
            const auto rows =
                statistics::load_statistics(library.path(), 0, QTimeZone::utc()).daily;
            QCOMPARE(rows.size(), 1U);
            QCOMPARE(rows.front().characters_read, 1200);
        }
    }
};

QTEST_GUILESS_MAIN(TtuSyncTest)
#include "tst_ttu_sync.moc"
