// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's ッツ auto sync, following NativeMac/NativeReaderView.swift
// (configure's autoSyncEnabled, syncOnOpenIfNeeded, didSyncOnOpen,
// reloadBookmark, scheduleAutoExport, flushAutoSync and runAutoExport) and
// Features/Sync/SyncManager.swift (syncBook(importOnly:)). The sync view model
// owns the commands; the reader exposes the bookmark it wrote and the reload
// the page runs after an imported open sync.
//
// The book "book" has a ten-character first chapter and a 1000-character
// second one, so the fixture's 250-character remote progress resolves to
// chapter 1 at progress 0.24. Every test drives the real TtuSync stack against
// the fake Drive server, so "import-only" is observed as the absence of an
// upload, not as an option flag.

#include "viewmodels/reader/reader_view_model.h"
#include "viewmodels/sasayaki/sasayaki_view_model.h"
#include "viewmodels/sync/sync_settings_view_model.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTimeZone>
#include <QUrlQuery>
#include <QJsonArray>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "data_root.h"
#include "fakes/fake_drive_server.h"
#include "fakes/ttu_fixture.h"
#include "flows/epub_fixture.h"
#include "models/statistics/statistics.h"
#include "services/books/book_storage.h"
#include "services/settings/global_settings.h"
#include "services/statistics/statistics_storage.h"
#include "services/storage/app_paths.h"
#include "services/sync/oauth_credentials.h"

using niratan::statistics::StatisticsSyncMode;
using niratan::viewmodels::ReaderViewModel;
using niratan::viewmodels::SasayakiViewModel;
using niratan::viewmodels::SyncSettingsViewModel;

namespace {

const QString kFolder = QStringLiteral("book");
constexpr int kFirstCharacters = 10;
constexpr int kSecondCharacters = 1000;
constexpr int kBookCharacters = kFirstCharacters + kSecondCharacters;

std::filesystem::path book_root() {
    return niratan::storage::data_root() / "Books" / kFolder.toStdString();
}

bool write_settings(const QJsonObject& extra = {}) {
    QJsonObject settings{
        {"googleClientId", "123-fixture.apps.googleusercontent.com"},
        {"enableSync", true},
        {"enableAutoSync", true},
        {"syncProvider", "ttu"},
        {"syncUploadBooks", true},
        {"statisticsEnableSync", true},
        {"statisticsSyncMode", "Merge"},
        {"enableStatistics", true},
        {"statisticsAutostartMode", "On"},
        {"statisticsResetTime", 0},
        {"statisticsResetTimeMigratedToMinutes", true},
        {"enableSasayaki", true},
        {"sasayakiEnableSync", true},
    };
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        settings.insert(it.key(), it.value());
    }
    QFile file(QString::fromStdString((niratan::storage::data_root() / "settings.json").string()));
    const QByteArray data = QJsonDocument(settings).toJson();
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}

bool authorize() {
    return niratan::sync::CredentialsStore(niratan::storage::data_root() / "sync/credentials.json")
        .save({"access", "refresh", "123-fixture.apps.googleusercontent.com", {}});
}

bool make_book() {
    const niratan::books::BookStorage storage(niratan::storage::data_root());
    niratan::books::BookMetadata book;
    book.id = "book";
    book.folder = kFolder.toStdString();
    book.title = "My Book";
    book.renamed_title = "Display Title";
    book.epub = "book.epub";
    const auto directory = storage.book_directory(book.folder);
    if (!directory || !QDir().mkpath(QString::fromStdString(directory->string()))) {
        return false;
    }
    std::string first;
    std::string second;
    for (int index = 0; index < kFirstCharacters; ++index) {
        first += "あ";
    }
    for (int index = 0; index < kSecondCharacters; ++index) {
        second += "い";
    }
    if (!niratan::test::write_epub(*directory / "book.epub", "My Book", first, second)) {
        return false;
    }
    niratan::books::BookInfo info;
    info.character_count = kBookCharacters;
    info.chapter_info.emplace("first.xhtml",
                              niratan::books::ChapterInfo{0, 0, kFirstCharacters, {}});
    info.chapter_info.emplace(
        "second.xhtml", niratan::books::ChapterInfo{1, kFirstCharacters, kSecondCharacters, {}});
    return storage.save_metadata(book) && storage.save_book_info(book.folder, info);
}

std::unique_ptr<SyncSettingsViewModel> make_sync(FakeDriveServer& server) {
    SyncSettingsViewModel::Config config{niratan::storage::data_root().string(),
                                         {},
                                         server.base_url(),
                                         QTimeZone(QTimeZone::LocalTime)};
    return std::make_unique<SyncSettingsViewModel>(std::move(config));
}

std::vector<const FakeDriveServer::Request*> uploads(const FakeDriveServer& server) {
    std::vector<const FakeDriveServer::Request*> out;
    for (const auto& request : server.requests()) {
        if (request.path.startsWith(QStringLiteral("/upload/drive/v3/files"))) {
            out.push_back(&request);
        }
    }
    return out;
}

const FakeDriveServer::Request* request_at(const FakeDriveServer& server, const QString& path,
                                           const QByteArray& method) {
    const auto& requests = server.requests();
    const auto it = std::ranges::find_if(requests, [&](const auto& request) {
        return request.path == path && request.method == method;
    });
    return it == requests.end() ? nullptr : &*it;
}

niratan::statistics::Statistics today_row(int characters) {
    niratan::statistics::Statistics row;
    row.title = "My Book";
    row.date_key = niratan::statistics::date_key(QDateTime::currentDateTime(), 0);
    row.characters_read = characters;
    row.reading_time = 60.0;
    row.last_statistic_modified = QDateTime::currentMSecsSinceEpoch();
    return row;
}

bool import_today(int characters) {
    return niratan::statistics::import_daily_statistics(book_root(), {today_row(characters)},
                                                        StatisticsSyncMode::merge, 0);
}

QString revision(const ReaderViewModel& reader) {
    return QUrlQuery(QUrl(reader.chapter_url())).queryItemValue(QStringLiteral("v"));
}

void complete_restore(ReaderViewModel& reader) {
    reader.message(QStringLiteral("restoreCompleted"),
                   QString::fromUtf8(QJsonDocument(QJsonObject{{"token", revision(reader)}})
                                         .toJson(QJsonDocument::Compact)));
}

void remove_bookmark() {
    const QFile file(QString::fromStdString((book_root() / "bookmark.json").string()));
    if (file.exists()) {
        QFile::remove(file.fileName());
    }
}

bool write_bookmark(int chapter, double progress, int characters, std::int64_t modified_ms) {
    const niratan::books::BookStorage storage(niratan::storage::data_root());
    return storage.save_bookmark(
        kFolder.toStdString(),
        {chapter, progress, characters, modified_ms / 1000.0 - ttu_fixture::unix_reference});
}

void clear_statistics() {
    for (const auto* name : {"statistics.json", "statistics_sessions.json"}) {
        const QFile file(QString::fromStdString((book_root() / name).string()));
        if (file.exists()) {
            QFile::remove(file.fileName());
        }
    }
}

}  // namespace

class ReaderOpenSyncTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void init();
    void cleanup();

    void open_sync_imports_once_and_reloads();
    void skipped_open_sync_leaves_the_position();
    void close_flushes_an_edited_bookmark();
    void close_without_an_edit_sends_nothing_then_an_edit_does();
    void close_failure_sets_the_sync_error();
    void close_while_syncing_sends_nothing();

   private:
    niratan::test::TempDataRoot data_root_;
    ReaderViewModel* reader_ = nullptr;
};

void ReaderOpenSyncTest::initTestCase() {
    QVERIFY(data_root_.isValid());
    QVERIFY(make_book());
    reader_ = ReaderViewModel::instance(0);
    QVERIFY(reader_ != nullptr);
}

void ReaderOpenSyncTest::init() {
    if (reader_->has_book()) {
        reader_->closeBook();
    }
    remove_bookmark();
    clear_statistics();
}

void ReaderOpenSyncTest::cleanup() {
    if (reader_->has_book()) {
        reader_->closeBook();
    }
    remove_bookmark();
    clear_statistics();
}

// B2 and B3: opening a book runs one import-only sync; an imported result
// moves the reader to the synced bookmark on reload, shows the externally
// written statistics and reloads the audiobook position. Each open runs one
// sync; a settings change and a page turn do not run it.
void ReaderOpenSyncTest::open_sync_imports_once_and_reloads() {
    FakeDriveServer server;
    QVERIFY(server.start());
    server.enable_tree();
    ttu_fixture::seed_folders(server);
    ttu_fixture::seed_media(server);
    QVERIFY(write_settings());
    QVERIFY(authorize());
    auto sync = make_sync(server);

    QVERIFY(reader_->openBook(kFolder));
    QVERIFY(reader_->restoring());
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QVERIFY(!reader_->bookmark_edited());
    SasayakiViewModel player;
    QCOMPARE(player.position(), 0.0);

    // The sync writes today's row to disk; the open reader still holds the
    // statistics it loaded before.
    QVERIFY(import_today(321));
    QCOMPARE(reader_->statistics_today_characters(), 0);

    QSignalSpy finished(sync.get(), &SyncSettingsViewModel::readerSyncFinished);
    sync->syncReaderOpen(kFolder);
    QVERIFY(sync->syncing());
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(finished.front().at(0).toString(), kFolder);
    QCOMPARE(finished.front().at(1).toBool(), true);

    // The page reloads the position, statistics and audiobook on an import.
    reader_->reloadBookmark();
    complete_restore(*reader_);
    player.reloadPlayback();
    QCOMPARE(reader_->chapter_index(), 1);
    QVERIFY(std::abs(reader_->chapter_progress() - 0.24) < 1e-9);
    QCOMPARE(reader_->character_offset(), 250);
    QCOMPARE(reader_->statistics_today_characters(), 321);
    QCOMPARE(player.position(), 42.5);

    // Import-only: the open sync read the sidecars and uploaded nothing.
    QVERIFY(uploads(server).empty());
    QVERIFY(request_at(server, "/drive/v3/files/p", "GET"));
    QVERIFY(request_at(server, "/drive/v3/files/s", "GET"));
    QVERIFY(request_at(server, "/drive/v3/files/a", "GET"));

    // Each call is one open: reopening the same book syncs again. The first
    // import stamped the local bookmark with the remote time, so the remote
    // file moves to make the reopen an import whose progress read is visible.
    const auto progress_reads = [&server] {
        return static_cast<int>(
            std::ranges::count_if(server.requests(), [](const FakeDriveServer::Request& request) {
                return request.path == QStringLiteral("/drive/v3/files/p") &&
                    request.method == "GET";
            }));
    };
    const int reads = progress_reads();
    server.put_file("p", "progress_1_6_1690000010000_0.25.json", "book-id",
                    ttu_fixture::progress_json);
    QVERIFY(reader_->openBook(kFolder));
    complete_restore(*reader_);
    sync->syncReaderOpen(kFolder);
    QTRY_COMPARE(finished.size(), 2);
    QCOMPARE(progress_reads(), reads + 1);

    // A settings change and a page turn do not run the sync again.
    const auto before = server.requests().size();
    QVERIFY(sync->setProperty("syncMode", QStringLiteral("Manual")));
    reader_->updateChapterProgress(0.3);
    QTest::qWait(100);
    QCOMPARE(server.requests().size(), before);
    QCOMPARE(finished.size(), 2);
}

// B2 and B3: a skipped open sync (the local bookmark is newer, so import-only
// refuses the export direction) reports no import and leaves the position and
// statistics the reader opened with.
void ReaderOpenSyncTest::skipped_open_sync_leaves_the_position() {
    FakeDriveServer server;
    QVERIFY(server.start());
    server.enable_tree();
    ttu_fixture::seed_folders(server);
    ttu_fixture::seed_media(server);
    QVERIFY(write_settings());
    QVERIFY(authorize());
    auto sync = make_sync(server);
    QVERIFY(write_bookmark(1, 0.5, kFirstCharacters + 500, ttu_fixture::remote_ms + 2000));
    QVERIFY(import_today(123));

    QVERIFY(reader_->openBook(kFolder));
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->statistics_today_characters(), 123);

    QSignalSpy finished(sync.get(), &SyncSettingsViewModel::readerSyncFinished);
    sync->syncReaderOpen(kFolder);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(finished.front().at(1).toBool(), false);
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->character_offset(), kFirstCharacters + 500);
    QCOMPARE(reader_->statistics_today_characters(), 123);
    QVERIFY(uploads(server).empty());
}

// B4: closing after a read exports the bookmark, merged statistics and the
// audiobook position, and a second close without a new edit sends nothing.
void ReaderOpenSyncTest::close_flushes_an_edited_bookmark() {
    FakeDriveServer server;
    QVERIFY(server.start());
    server.enable_tree();
    ttu_fixture::seed_folders(server);
    ttu_fixture::seed_media(server);
    QVERIFY(write_settings());
    QVERIFY(authorize());
    auto sync = make_sync(server);
    QVERIFY(write_bookmark(1, 0.125, kFirstCharacters + 125, ttu_fixture::remote_ms + 2000));

    QVERIFY(reader_->openBook(kFolder));
    QVERIFY(!reader_->bookmark_edited());
    reader_->updateChapterProgress(0.5);
    const bool edited = reader_->bookmark_edited();
    QVERIFY(edited);
    reader_->closeBook();

    sync->syncReaderClose(kFolder, edited);
    QTRY_VERIFY(!uploads(server).empty());
    QTRY_VERIFY(request_at(server, "/upload/drive/v3/files/s", "PATCH") != nullptr);
    const auto* progress = request_at(server, "/upload/drive/v3/files/p", "PATCH");
    QVERIFY(progress);
    QCOMPARE(QJsonDocument::fromJson(progress->media).object().value("exploredCharCount").toInt(),
             kFirstCharacters + 500);
    const auto rows =
        QJsonDocument::fromJson(request_at(server, "/upload/drive/v3/files/s", "PATCH")->media)
            .array();
    QStringList dates;
    for (const auto item : rows) {
        dates.append(item.toObject().value("dateKey").toString());
    }
    QVERIFY(dates.contains(QStringLiteral("2023-07-22")));

    const auto count = uploads(server).size();
    sync->syncReaderClose(kFolder, false);
    QTest::qWait(100);
    QCOMPARE(uploads(server).size(), count);
}

// B4: a close before the session wrote a bookmark sends nothing, while the
// same close after a page turn sends the bookmark.
void ReaderOpenSyncTest::close_without_an_edit_sends_nothing_then_an_edit_does() {
    FakeDriveServer server;
    QVERIFY(server.start());
    server.enable_tree();
    ttu_fixture::seed_folders(server);
    ttu_fixture::seed_media(server);
    QVERIFY(write_settings());
    QVERIFY(authorize());
    auto sync = make_sync(server);
    QVERIFY(write_bookmark(1, 0.125, kFirstCharacters + 125, ttu_fixture::remote_ms + 2000));

    QVERIFY(reader_->openBook(kFolder));
    const bool untouched = reader_->bookmark_edited();
    QVERIFY(!untouched);
    reader_->closeBook();
    sync->syncReaderClose(kFolder, untouched);
    QTest::qWait(100);
    QVERIFY(uploads(server).empty());

    QVERIFY(reader_->openBook(kFolder));
    reader_->updateChapterProgress(0.5);
    const bool edited = reader_->bookmark_edited();
    QVERIFY(edited);
    reader_->closeBook();
    sync->syncReaderClose(kFolder, edited);
    QTRY_VERIFY(!uploads(server).empty());
}

// B4: an export failure sets the sync error and still lets the window close.
void ReaderOpenSyncTest::close_failure_sets_the_sync_error() {
    FakeDriveServer server;
    QVERIFY(server.start());
    server.enable_tree();
    ttu_fixture::seed_folders(server);
    ttu_fixture::seed_media(server);
    QVERIFY(write_settings());
    QVERIFY(authorize());
    auto sync = make_sync(server);
    QVERIFY(write_bookmark(1, 0.125, kFirstCharacters + 125, ttu_fixture::remote_ms + 2000));

    QVERIFY(reader_->openBook(kFolder));
    reader_->updateChapterProgress(0.5);
    const bool edited = reader_->bookmark_edited();
    QVERIFY(edited);
    reader_->closeBook();

    server.enqueue({500, {}});
    QSignalSpy failed(sync.get(), &SyncSettingsViewModel::syncFailed);
    sync->syncReaderClose(kFolder, edited);
    QTRY_VERIFY(!sync->syncError().isEmpty());
    QCOMPARE(failed.size(), 0);
    QVERIFY(uploads(server).empty());
}

// B4: a close during a running sync leaves the remote at its last completed
// write instead of starting a second transfer.
void ReaderOpenSyncTest::close_while_syncing_sends_nothing() {
    FakeDriveServer server;
    QVERIFY(server.start());
    server.enable_tree();
    ttu_fixture::seed_folders(server);
    ttu_fixture::seed_media(server);
    QVERIFY(write_settings());
    QVERIFY(authorize());
    auto sync = make_sync(server);

    QVERIFY(reader_->openBook(kFolder));
    QSignalSpy finished(sync.get(), &SyncSettingsViewModel::readerSyncFinished);
    sync->syncReaderOpen(kFolder);
    QVERIFY(sync->syncing());
    sync->syncReaderClose(kFolder, true);
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(uploads(server).empty());
}

QTEST_GUILESS_MAIN(ReaderOpenSyncTest)
#include "tst_reader_open_sync.moc"
