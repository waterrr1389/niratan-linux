// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader half of Sasayaki cue following, following
// NativeMac/NativeReaderView.swift (loadChapterForSasayaki,
// establishProgrammaticDestination, persistBookmark, resetTrackingBaseline,
// nextChapter, previousChapter, jumpToCharacter, jumpToChapter) and
// SasayakiPlayer.pendingCue's readerProgress.
//
// The book "Cue" has two spine chapters of five and three readable characters,
// so a cue offset maps onto a chapter count, a chapter index and the global
// bookmark character. Nothing here loads the web view: the model reports a
// restore and this test answers it, as the page does.

#include "viewmodels/reader/reader_view_model.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QTest>

#include <zip.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "data_root.h"
#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"

using niratan::books::BookStorage;
using niratan::viewmodels::ReaderViewModel;

class ReaderSasayakiNavigationTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void cleanup();
    void goToChapterAtWritesTheDestinationBookmark();
    void chapterCharacterCount();
    void readerChapterLoadsStartATransition();
    void goToChapterAtResetsTheStatisticsBaseline();
    void syncBookmarkToProgress();

   private:
    bool open_book();

    niratan::test::TempDataRoot data_root_;
    ReaderViewModel* reader_ = nullptr;
};

namespace {

bool write_epub(const std::filesystem::path& path) {
    int error = 0;
    zip_t* archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (archive == nullptr) {
        return false;
    }
    const std::string opf = R"XML(<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
    <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Cue</dc:title></metadata>
    <manifest>
        <item id="one" href="one.xhtml" media-type="application/xhtml+xml"/>
        <item id="two" href="two.xhtml" media-type="application/xhtml+xml"/>
    </manifest>
    <spine>
        <itemref idref="one"/>
        <itemref idref="two"/>
    </spine>
</package>
)XML";
    const std::pair<const char*, std::string_view> entries[]{
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml",
         R"XML(<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
    <rootfiles>
        <rootfile full-path="book.opf" media-type="application/oebps-package+xml"/>
    </rootfiles>
</container>
)XML"},
        {"book.opf", opf},
        {"one.xhtml", R"XML(<html><body><p>一二三四五</p></body></html>)XML"},
        {"two.xhtml", R"XML(<html><body><p>あいう</p></body></html>)XML"},
    };
    for (const auto& [name, bytes] : entries) {
        auto* source = zip_source_buffer(archive, bytes.data(), bytes.size(), 0);
        if (source == nullptr || zip_file_add(archive, name, source, 0) < 0) {
            if (source != nullptr) {
                zip_source_free(source);
            }
            zip_discard(archive);
            return false;
        }
    }
    if (zip_close(archive) < 0) {
        zip_discard(archive);
        return false;
    }
    return true;
}

bool make_cue_book() {
    const BookStorage storage(niratan::storage::data_root());
    niratan::books::BookMetadata book;
    book.id = "Cue";
    book.folder = "Cue";
    book.title = "Cue";
    book.epub = "book.epub";
    const auto directory = storage.book_directory(book.folder);
    return directory && QDir().mkpath(QString::fromStdString(directory->string())) &&
        write_epub(*directory / "book.epub") && storage.save_metadata(book);
}

QString revision(const ReaderViewModel& reader) {
    return QUrlQuery(QUrl(reader.chapter_url())).queryItemValue("v");
}

void send(ReaderViewModel& reader, const QString& name, const QJsonObject& data) {
    reader.message(name, QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
}

std::optional<niratan::books::Bookmark> stored_bookmark() {
    return BookStorage(niratan::storage::data_root()).load_bookmark("Cue");
}

bool write_statistics_settings() {
    const auto root = niratan::storage::data_root();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    const QJsonObject settings{
        {"enableStatistics", true},
        {"statisticsAutostartMode", "On"},
        {"statisticsResetTime", 0},
        {"statisticsResetTimeMigratedToMinutes", true},
    };
    QFile file(QString::fromStdString((root / "settings.json").string()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(QJsonDocument(settings).toJson()) > 0;
}

void wait_for_clock_tick() {
    const qint64 start = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() == start) {
        QTest::qWait(1);
    }
}

}  // namespace

void ReaderSasayakiNavigationTest::initTestCase() {
    QVERIFY(data_root_.isValid());
    QVERIFY(make_cue_book());
    reader_ = ReaderViewModel::instance();
}

bool ReaderSasayakiNavigationTest::open_book() {
    if (!reader_->openBook("Cue") || revision(*reader_).isEmpty()) {
        return false;
    }
    send(*reader_, "restoreCompleted", {{"token", revision(*reader_)}});
    return !reader_->restoring();
}

void ReaderSasayakiNavigationTest::cleanup() {
    if (reader_ != nullptr) {
        reader_->closeBook();
    }
}

// B5: NativeReaderView.loadChapterForSasayaki and
// establishProgrammaticDestination. The destination bookmark is written before
// the restore and the character count adds the chapter's cue offset to the
// chapters before it.
void ReaderSasayakiNavigationTest::goToChapterAtWritesTheDestinationBookmark() {
    QVERIFY(open_book());
    QVERIFY(!stored_bookmark().has_value());

    reader_->goToChapterAt(1, 0.4);
    QCOMPARE(reader_->chapter_index(), 1);
    auto bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 1);
    QCOMPARE(bookmark->progress, 0.4);
    QCOMPARE(bookmark->character_count, 6);

    reader_->goToChapterAt(0, 0.5);
    QCOMPARE(reader_->chapter_index(), 0);
    bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 0);
    QCOMPARE(bookmark->progress, 0.5);
    QCOMPARE(bookmark->character_count, 2);

    reader_->goToChapterAt(2, 0.5);
    QCOMPARE(reader_->chapter_index(), 0);
    bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 0);
    QCOMPARE(bookmark->progress, 0.5);
    QCOMPARE(bookmark->character_count, 2);
}

// B5: the readable characters of a spine chapter.
void ReaderSasayakiNavigationTest::chapterCharacterCount() {
    QVERIFY(open_book());
    QCOMPARE(reader_->chapterCharacterCount(0), 5);
    QCOMPARE(reader_->chapterCharacterCount(1), 3);
    QCOMPARE(reader_->chapterCharacterCount(2), 0);
    QCOMPARE(reader_->chapterCharacterCount(-1), 0);
}

// B8: nextChapter, previousChapter, jumpToCharacter and jumpToChapter start a
// transition; the Sasayaki chapter load does not repeat it.
void ReaderSasayakiNavigationTest::readerChapterLoadsStartATransition() {
    QVERIFY(open_book());
    QSignalSpy transitions(reader_, &ReaderViewModel::chapterTransitionStarted);
    QVERIFY(transitions.isEmpty());

    reader_->goToChapter(1);
    QCOMPARE(transitions.size(), 1);
    reader_->goToChapter(1);
    QCOMPARE(transitions.size(), 1);

    reader_->goToChapterAt(0, 0.25);
    QCOMPARE(transitions.size(), 1);

    reader_->goToContentsEntry(1, "");
    QCOMPARE(transitions.size(), 2);

    reader_->jumpToCharacter(6);
    QCOMPARE(transitions.size(), 3);

    send(*reader_, "restoreCompleted", {{"token", revision(*reader_)}});
    reader_->goToChapter(0);
    QCOMPARE(transitions.size(), 4);

    send(*reader_, "restoreCompleted", {{"token", revision(*reader_)}});
    reader_->handlePageTurnResult(true, QVariant(QStringLiteral("limit")));
    QCOMPARE(transitions.size(), 5);
    QCOMPARE(reader_->chapter_index(), 1);

    send(*reader_, "restoreCompleted", {{"token", revision(*reader_)}});
    reader_->handlePageTurnResult(false, QVariant(QStringLiteral("limit")));
    QCOMPARE(transitions.size(), 6);
    QCOMPARE(reader_->chapter_index(), 0);
}

// B5: NativeReaderView.establishProgrammaticDestination resets the
// statistics baseline at the destination, so the characters a cue jump
// crosses are not credited (resetTrackingBaseline).
void ReaderSasayakiNavigationTest::goToChapterAtResetsTheStatisticsBaseline() {
    QVERIFY(write_statistics_settings());
    QVERIFY(open_book());
    reader_->setReaderActive(true);
    QVERIFY(reader_->tracking());

    reader_->goToChapterAt(1, 0.4);
    send(*reader_, "restoreCompleted", {{"token", revision(*reader_)}});
    QVERIFY(!reader_->restoring());

    // The first report is the cue destination already: no characters.
    wait_for_clock_tick();
    reader_->updateChapterProgress(0.5);
    QCOMPARE(reader_->statistics_today_characters(), 0);
    // A report past it credits only the two characters read after it.
    wait_for_clock_tick();
    reader_->updateChapterProgress(1.0);
    QCOMPARE(reader_->statistics_today_characters(), 2);
}

// B5: NativeReaderView.syncBookmarkToSasayakiCue's reader side. In the open
// chapter the cue destination is stored without a reload; another chapter
// loads at the cue's progress with its bookmark written first.
void ReaderSasayakiNavigationTest::syncBookmarkToProgress() {
    const auto directory = BookStorage(niratan::storage::data_root()).book_directory("Cue");
    QVERIFY(directory.has_value());
    QFile::remove(QString::fromStdString((*directory / "bookmark.json").string()));
    QVERIFY(open_book());
    QVERIFY(!stored_bookmark().has_value());

    reader_->syncBookmarkToProgress(0, 0.5);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QVERIFY(!reader_->restoring());
    auto bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 0);
    QCOMPARE(bookmark->progress, 0.5);
    QCOMPARE(bookmark->character_count, 2);

    reader_->syncBookmarkToProgress(1, 0.4);
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.4);
    QVERIFY(reader_->restoring());
    bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 1);
    QCOMPARE(bookmark->progress, 0.4);
    QCOMPARE(bookmark->character_count, 6);

    reader_->syncBookmarkToProgress(2, 0.5);
    bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 1);
    QCOMPARE(bookmark->progress, 0.4);
    QCOMPARE(bookmark->character_count, 6);
}

QTEST_GUILESS_MAIN(ReaderSasayakiNavigationTest)
#include "tst_reader_sasayaki_navigation.moc"
