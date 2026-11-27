// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader window's ッツ auto sync wiring, following
// NativeMac/NativeReaderView.swift (syncOnOpenIfNeeded, reloadBookmark,
// scheduleAutoExport, flushAutoSync and the lifecycle onDisappear) and
// Features/Sync/SyncManager.swift (syncBook(importOnly:)). The sync view model
// owns both commands; the reader exposes the bookmark it wrote, the reload an
// imported open sync needs and the close preparation; ReaderWindow and
// ReaderPage join the two.
//
// No credentials exist in this data root, so every sync command fails with the
// "Not authenticated" message. That failure is what proves a call happened:
// opening a book sets it, closing a window whose session wrote a bookmark sets
// it again, and a close without such a write leaves it empty. The imported
// reload is driven by the sync model's own signal, as the page connects it:
// the reader then restores the bookmark and today's statistics the sync wrote,
// and the page reloads the audiobook position.
//
// The book "book" has a ten-character first chapter and a 1000-character second
// one, so a bookmark at chapter 1 progress 0.24 is the position the fixture's
// remote progress resolves to.

#include "models/sasayaki/sasayaki.h"
#include "models/statistics/statistics.h"
#include "services/books/book_storage.h"
#include "services/statistics/statistics_storage.h"
#include "services/storage/app_paths.h"
#include "viewmodels/reader/reader_view_model.h"
#include "viewmodels/sasayaki/sasayaki_view_model.h"
#include "viewmodels/sync/sync_settings_view_model.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrlQuery>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

#include <algorithm>
#include <clocale>
#include <filesystem>
#include <memory>
#include <string>

#include "data_root.h"
#include "epub_fixture.h"

Q_IMPORT_QML_PLUGIN(NiratanPlugin)
Q_IMPORT_QML_PLUGIN(NiratanStylePlugin)

using niratan::statistics::StatisticsSyncMode;
using niratan::viewmodels::ReaderViewModel;
using niratan::viewmodels::SasayakiViewModel;
using niratan::viewmodels::SyncSettingsViewModel;

namespace {

const QString kFolder = QStringLiteral("book");
constexpr int kFirstCharacters = 10;
constexpr int kSecondCharacters = 1000;

std::filesystem::path book_root() {
    return niratan::storage::data_root() / "Books" / kFolder.toStdString();
}

bool write_settings() {
    const QJsonObject settings{
        {"enableSync", true},           {"enableAutoSync", true},
        {"statisticsEnableSync", true}, {"sasayakiEnableSync", true},
        {"enableStatistics", true},     {"statisticsAutostartMode", "On"},
        {"statisticsResetTime", 0},     {"statisticsResetTimeMigratedToMinutes", true},
    };
    const auto root = niratan::storage::data_root();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    QFile file(QString::fromStdString((root / "settings.json").string()));
    const QByteArray data = QJsonDocument(settings).toJson();
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(data) == data.size();
}

bool make_book() {
    const niratan::books::BookStorage storage(niratan::storage::data_root());
    niratan::books::BookMetadata book;
    book.id = "book";
    book.folder = kFolder.toStdString();
    book.title = "My Book";
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
    info.character_count = kFirstCharacters + kSecondCharacters;
    info.chapter_info.emplace("first.xhtml",
                              niratan::books::ChapterInfo{0, 0, kFirstCharacters, {}});
    info.chapter_info.emplace(
        "second.xhtml", niratan::books::ChapterInfo{1, kFirstCharacters, kSecondCharacters, {}});
    return storage.save_metadata(book) && storage.save_book_info(book.folder, info);
}

bool write_playback(double position) {
    return niratan::books::BookStorage(niratan::storage::data_root())
        .save_sasayaki_playback(kFolder.toStdString(), {position, 0.0, 1.0F, {}});
}

bool import_today(int characters, std::int64_t stamp) {
    niratan::statistics::Statistics row;
    row.title = "My Book";
    row.date_key = niratan::statistics::date_key(QDateTime::currentDateTime(), 0);
    row.characters_read = characters;
    row.reading_time = 60.0;
    row.last_statistic_modified = stamp;
    return niratan::statistics::import_daily_statistics(book_root(), {row},
                                                        StatisticsSyncMode::merge, 0);
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

}  // namespace

class ReaderAutoSyncTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();

    void open_close_and_imported_reload_wiring();

   private:
    niratan::test::TempDataRoot data_root_;
    ReaderViewModel* reader_ = nullptr;
    SyncSettingsViewModel* sync_ = nullptr;
    QQmlEngine engine_;
    std::unique_ptr<QObject> window_object_;
    QQuickWindow* window_ = nullptr;
};

void ReaderAutoSyncTest::initTestCase() {
    QVERIFY(data_root_.isValid());
    QVERIFY(write_settings());
    QVERIFY(make_book());
    QVERIFY(import_today(321, QDateTime::currentMSecsSinceEpoch()));
    QVERIFY(write_playback(42.5));
    reader_ = ReaderViewModel::instance(0);
    QVERIFY(reader_ != nullptr);
}

// B3 and B4: the open command runs on every book open, the window close
// flushes only a session that wrote a bookmark, and an imported sync result
// reloads the position, today's statistics and the audiobook position.
void ReaderAutoSyncTest::open_close_and_imported_reload_wiring() {
    QQmlComponent component(
        &engine_, QUrl(QStringLiteral("qrc:/qt/qml/Niratan/src/views/shell/ReaderWindow.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    window_object_.reset(component.create());
    QVERIFY2(window_object_, qPrintable(component.errorString()));
    window_ = qobject_cast<QQuickWindow*>(window_object_.get());
    QVERIFY(window_);
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_));

    // The page's Sasayaki model follows the reader's book changes.
    auto* player = window_->findChild<SasayakiViewModel*>();
    QVERIFY(player);

    sync_ = SyncSettingsViewModel::instance();
    QVERIFY(sync_);

    // (2) Opening the book calls syncReaderOpen; without credentials the sync
    // fails and reports it.
    sync_->clearSyncError();
    QVERIFY(reader_->openBook(kFolder));
    QVERIFY(reader_->restoring());
    complete_restore(*reader_);
    QTRY_VERIFY_WITH_TIMEOUT(!sync_->syncError().isEmpty(), 5000);

    // (3) A session that wrote a bookmark flushes on window close.
    sync_->clearSyncError();
    reader_->updateChapterProgress(0.5);
    QVERIFY(reader_->bookmark_edited());
    window_->close();
    QTRY_VERIFY_WITH_TIMEOUT(!sync_->syncError().isEmpty(), 5000);

    // (4) Returning to the window and closing without a new bookmark write
    // sends nothing.
    sync_->clearSyncError();
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_));
    window_->close();
    QTest::qWait(500);
    QVERIFY2(sync_->syncError().isEmpty(), qPrintable(sync_->syncError()));

    // (5) An imported open sync reloads the reader and the audiobook. The
    // sync model's own signal drives the page's relay into the reader; the
    // player holds the sidecar value it loaded on the last book change, so the
    // reload must pick up the value written after it.
    sync_->clearSyncError();
    window_->show();
    QVERIFY(QTest::qWaitForWindowExposed(window_));
    QVERIFY(write_playback(0.0));
    QVERIFY(reader_->openBook(kFolder));
    QVERIFY(write_playback(42.5));
    QVERIFY(niratan::books::BookStorage(niratan::storage::data_root())
                .save_bookmark(kFolder.toStdString(), {1, 0.24, kFirstCharacters + 240, {}}));
    QVERIFY(import_today(654, QDateTime::currentMSecsSinceEpoch() + 1000));
    QVERIFY(QMetaObject::invokeMethod(sync_, "readerSyncFinished", Q_ARG(QString, kFolder),
                                      Q_ARG(bool, true)));
    QCOMPARE(reader_->chapter_index(), 1);
    QVERIFY(std::abs(reader_->chapter_progress() - 0.24) < 1e-9);
    QCOMPARE(reader_->statistics_today_characters(), 654);
    QCOMPARE(player->position(), 42.5);
}

int main(int argc, char* argv[]) {
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("NiratanTest"));
    QGuiApplication::setOrganizationName(QStringLiteral("niratan"));
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle(QStringLiteral("NiratanStyle"));
    ReaderAutoSyncTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_reader_auto_sync.moc"
