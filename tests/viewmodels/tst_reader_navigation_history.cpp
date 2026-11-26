// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's back and forward history, following NativeMac/NativeReaderView.swift
// (NativeReaderModel.currentPosition, recordPosition, jumpToChapter,
// jumpToCharacter, jumpToHighlight, jumpToLink, navigateBackwards,
// navigateForwards, restorePosition, handleManualNavigation, backTarget,
// forwardTarget, characterProgress, establishProgrammaticDestination,
// syncProgressAfterProgrammaticJump and the WebKit coordinator's link policy
// and progressChanged message), Models/Book.swift
// (BookInfo.resolveCharacterPosition) and Models/Profile.swift
// (ContentLanguageProfile.displayCount(forRawCharacters:)).
//
// The book "History" has a ten-character first chapter, a ten-character second
// chapter and a 1200-character third chapter, so global character offsets map
// onto all three and a history label above 999 shows whether grouping leaked
// in. The first two chapters also name anchors, so a link can target a
// fragment without changing its spine. Nothing here loads the web view: the
// model reports a restore and this test answers it as the page does.

#include "viewmodels/reader/reader_view_model.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QtTest>

#include <zip.h>

#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "data_root.h"
#include "models/profiles/reader_settings.h"
#include "services/books/book_storage.h"
#include "services/profiles/profile_repository.h"
#include "services/settings/settings_store.h"
#include "services/storage/app_paths.h"

using niratan::books::BookStorage;
using niratan::profiles::ReaderProfileSettings;
using niratan::viewmodels::ReaderViewModel;

class ReaderNavigationHistoryTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void cleanup();

    void histories_start_empty_and_chapter_jumps_record_the_origin();
    void same_chapter_jumps_record_even_the_same_spine();
    void character_jumps_record_the_resolved_origin();
    void links_match_spine_resources_and_decode_the_fragment();
    void same_spine_links_jump_in_the_open_document();
    void cross_spine_links_load_the_target_at_zero();
    void back_and_forward_restore_the_popped_position();
    void programmatic_restores_do_not_credit_reading();
    void manual_navigation_clears_forward_without_adding_back();
    void continuous_scrolling_clears_forward_without_adding_back();
    void continuous_report_at_the_same_position_clears_forward();
    void synced_bookmarks_keep_forward();
    void history_targets_use_the_profile_display_count();

   private:
    bool open_book(bool clear_bookmark = true);
    bool open_empty_book();
    bool write_reader(const ReaderProfileSettings& settings);

    niratan::test::TempDataRoot data_root_;
    ReaderViewModel* reader_ = nullptr;
};

namespace {

const QString kFolder = QStringLiteral("History");
const QString kEmptyFolder = QStringLiteral("Empty");
constexpr int kFirstCharacters = 10;
constexpr int kThirdCharacters = 1200;

bool write_epub(const std::filesystem::path& path) {
    int error = 0;
    zip_t* archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (archive == nullptr) {
        return false;
    }
    const std::string opf = R"XML(<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
    <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>History</dc:title></metadata>
    <manifest>
        <item id="first" href="first.xhtml" media-type="application/xhtml+xml"/>
        <item id="second" href="second.xhtml" media-type="application/xhtml+xml"/>
        <item id="third" href="third.xhtml" media-type="application/xhtml+xml"/>
        <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    </manifest>
    <spine>
        <itemref idref="first"/>
        <itemref idref="second"/>
        <itemref idref="third"/>
    </spine>
</package>
)XML";
    const std::string nav =
        R"XML(<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><body>
<nav epub:type="toc"><ol>
<li><a href="first.xhtml">First</a></li>
<li><a href="second.xhtml#spot">Second</a></li>
<li><a href="third.xhtml">Third</a></li>
</ol></nav>
</body></html>
)XML";
    std::string third;
    for (int index = 0; index < kThirdCharacters; ++index) {
        third += "あ";
    }
    const std::string first =
        "<html><body><p id=\"top\">あいうえお</p><p>かきくけこ</p></body></html>";
    const std::string second =
        "<html><body><p id=\"spot\">さしすせそ</p><p>たちつてと</p></body></html>";
    const std::string third_document = "<html><body><p>" + third + "</p></body></html>";
    const std::vector<std::pair<const char*, std::string_view>> entries{
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml",
         R"XML(<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
    <rootfiles>
        <rootfile full-path="book.opf" media-type="application/oebps-package+xml"/>
    </rootfiles>
</container>
)XML"},
        {"book.opf", opf},
        {"nav.xhtml", nav},
        {"first.xhtml", first},
        {"second.xhtml", second},
        {"third.xhtml", third_document},
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

bool make_history_book() {
    const BookStorage storage(niratan::storage::data_root());
    niratan::books::BookMetadata book;
    book.id = "History";
    book.folder = "History";
    book.title = "History";
    book.epub = "book.epub";
    const auto directory = storage.book_directory(book.folder);
    return directory && QDir().mkpath(QString::fromStdString(directory->string())) &&
        write_epub(*directory / "book.epub") && storage.save_metadata(book);
}

bool write_empty_epub(const std::filesystem::path& path) {
    int error = 0;
    zip_t* archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (archive == nullptr) {
        return false;
    }
    const std::string opf = R"XML(<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
    <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Empty</dc:title></metadata>
    <manifest>
        <item id="blank" href="blank.xhtml" media-type="application/xhtml+xml"/>
    </manifest>
    <spine><itemref idref="blank"/></spine>
</package>
)XML";
    const std::vector<std::pair<const char*, std::string_view>> entries{
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml",
         R"XML(<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
    <rootfiles>
        <rootfile full-path="book.opf" media-type="application/oebps-package+xml"/>
    </rootfiles>
</container>
)XML"},
        {"book.opf", opf},
        {"blank.xhtml", "<html><body><p> </p></body></html>"},
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

bool make_empty_book() {
    const BookStorage storage(niratan::storage::data_root());
    niratan::books::BookMetadata book;
    book.id = "Empty";
    book.folder = "Empty";
    book.title = "Empty";
    book.epub = "book.epub";
    const auto directory = storage.book_directory(book.folder);
    return directory && QDir().mkpath(QString::fromStdString(directory->string())) &&
        write_empty_epub(*directory / "book.epub") && storage.save_metadata(book);
}

bool write_global_settings(bool statistics_enabled, const QString& autostart) {
    const auto root = niratan::storage::data_root();
    std::error_code error;
    std::filesystem::create_directories(root, error);
    const QJsonObject settings{
        {"enableStatistics", statistics_enabled},
        {"statisticsAutostartMode", autostart},
        {"statisticsResetTime", 0},
        {"statisticsResetTimeMigratedToMinutes", true},
    };
    QFile file(QString::fromStdString((root / "settings.json").string()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(QJsonDocument(settings).toJson()) > 0;
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

QString chapter_document(ReaderViewModel& reader, const QString& path) {
    return QString::fromStdString(reader.document_for(path.toStdString()));
}

double initial_restore_progress(const QString& document) {
    const QString marker = QStringLiteral("return window.hoshiReader.restoreProgress(");
    const qsizetype start = document.indexOf(marker);
    if (start < 0) {
        return qQNaN();
    }
    const qsizetype argument_start = start + marker.size();
    const qsizetype end = document.indexOf(QLatin1Char(')'), argument_start);
    if (end < 0) {
        return qQNaN();
    }
    bool ok = false;
    const double progress = document.mid(argument_start, end - argument_start).toDouble(&ok);
    return ok ? progress : qQNaN();
}

std::optional<niratan::books::Bookmark> stored_bookmark() {
    return BookStorage(niratan::storage::data_root()).load_bookmark(kFolder.toStdString());
}

bool remove_bookmark() {
    const auto directory =
        BookStorage(niratan::storage::data_root()).book_directory(kFolder.toStdString());
    if (!directory) {
        return true;
    }
    const QString path = QString::fromStdString((*directory / "bookmark.json").string());
    return !QFile::exists(path) || QFile::remove(path);
}

void wait_for_clock_tick() {
    const qint64 start = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() == start) {
        QTest::qWait(1);
    }
}

}  // namespace

void ReaderNavigationHistoryTest::initTestCase() {
    QVERIFY(data_root_.isValid());
    QVERIFY(write_global_settings(false, QStringLiteral("Off")));
    QVERIFY(make_history_book());
    QVERIFY(make_empty_book());
    reader_ = ReaderViewModel::instance();
    QVERIFY(reader_ != nullptr);
}

bool ReaderNavigationHistoryTest::write_reader(const ReaderProfileSettings& settings) {
    auto store = niratan::settings::SettingsStore::create();
    return store && store->save_active_reader(settings) == niratan::settings::StoreError::none;
}

bool ReaderNavigationHistoryTest::open_book(bool clear_bookmark) {
    if (reader_->has_book()) {
        reader_->closeBook();
    }
    if (clear_bookmark && !remove_bookmark()) {
        return false;
    }
    if (!reader_->openBook(kFolder) || revision(*reader_).isEmpty()) {
        return false;
    }
    complete_restore(*reader_);
    return !reader_->restoring();
}

bool ReaderNavigationHistoryTest::open_empty_book() {
    if (reader_->has_book()) {
        reader_->closeBook();
    }
    const auto directory =
        BookStorage(niratan::storage::data_root()).book_directory(kEmptyFolder.toStdString());
    if (directory) {
        const QString path = QString::fromStdString((*directory / "bookmark.json").string());
        if (QFile::exists(path) && !QFile::remove(path)) {
            return false;
        }
    }
    if (!reader_->openBook(kEmptyFolder) || revision(*reader_).isEmpty()) {
        return false;
    }
    complete_restore(*reader_);
    return !reader_->restoring();
}

void ReaderNavigationHistoryTest::cleanup() {
    if (reader_ != nullptr && reader_->has_book()) {
        reader_->closeBook();
    }
    QFile::remove(
        QString::fromStdString((niratan::storage::data_root() / "settings.json").string()));
    remove_bookmark();
    const auto empty_directory =
        BookStorage(niratan::storage::data_root()).book_directory(kEmptyFolder.toStdString());
    if (empty_directory) {
        const QString path = QString::fromStdString((*empty_directory / "bookmark.json").string());
        if (QFile::exists(path)) {
            QFile::remove(path);
        }
    }
}

// B1 and B2: a new session has no history, empty navigation is a no-op, a
// valid contents entry records the current chapter and progress even before
// it loads, and an invalid chapter leaves both stacks alone.
void ReaderNavigationHistoryTest::histories_start_empty_and_chapter_jumps_record_the_origin() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());

    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), -1);
    QCOMPARE(reader_->back_target_text(), QString());
    QCOMPARE(reader_->forward_target_text(), QString());

    const QString url = reader_->chapter_url();
    reader_->navigateBackwards();
    reader_->navigateForwards();
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), -1);
    QCOMPARE(reader_->chapter_url(), url);
    QVERIFY(!reader_->restoring());

    reader_->updateChapterProgress(0.5);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->character_offset(), 5);

    QSignalSpy history(reader_, &ReaderViewModel::historyChanged);
    reader_->goToContentsEntry(1, QStringLiteral("spot"));
    QCOMPARE(history.size(), 1);
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->back_target(), 5);
    QCOMPARE(reader_->back_target_text(), QStringLiteral("5"));
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(reader_->restoring());
    QVERIFY(reader_->chapter_url() != url);
    const QString document = chapter_document(*reader_, QStringLiteral("second.xhtml"));
    QVERIFY(
        document.contains(QStringLiteral("return window.hoshiReader.jumpToFragment(\"spot\")")));
    QVERIFY(!document.contains(QStringLiteral("return window.hoshiReader.restoreProgress(")));

    complete_restore(*reader_);
    auto bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 1);
    QCOMPARE(bookmark->progress, 0.0);
    QCOMPARE(bookmark->character_count, kFirstCharacters);

    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), kFirstCharacters);

    reader_->goToContentsEntry(9, QStringLiteral("missing"));
    reader_->goToContentsEntry(-1, QStringLiteral("missing"));
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), kFirstCharacters);
    QVERIFY(!reader_->restoring());

    reader_->goToContentsEntry(1, QStringLiteral("spot"));
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->back_target(), 5);
    QCOMPARE(reader_->forward_target(), -1);
    complete_restore(*reader_);
    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.5);

    reader_->closeBook();
    QVERIFY(open_book(false));
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), -1);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.5);
}

// B2 and B7: a contents entry that names the current spine still records the
// origin, and restoring it keeps the same chapter without a fragment.
void ReaderNavigationHistoryTest::same_chapter_jumps_record_even_the_same_spine() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.4);

    reader_->goToContentsEntry(0, QStringLiteral("top"));
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->back_target(), 4);
    QCOMPARE(reader_->back_target_text(), QStringLiteral("4"));
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(reader_->restoring());
    const QString top_document = chapter_document(*reader_, QStringLiteral("first.xhtml"));
    QVERIFY(
        top_document.contains(QStringLiteral("return window.hoshiReader.jumpToFragment(\"top\")")));
    QVERIFY(!top_document.contains(QStringLiteral("return window.hoshiReader.restoreProgress(")));
    complete_restore(*reader_);

    reader_->navigateBackwards();
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.4);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), 0);
    QCOMPARE(reader_->forward_target_text(), QStringLiteral("0"));
    QVERIFY(reader_->restoring());
    const QString same_chapter_document = chapter_document(*reader_, QStringLiteral("first.xhtml"));
    QVERIFY(!same_chapter_document.contains(
        QStringLiteral("return window.hoshiReader.jumpToFragment(")));
    QVERIFY(std::abs(initial_restore_progress(same_chapter_document) - 0.4) < 1e-9);
    complete_restore(*reader_);

    const auto bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 0);
    QCOMPARE(bookmark->progress, 0.4);
    QCOMPARE(bookmark->character_count, 4);
}

// B3: character jumps resolve first, record the origin, and clear forward
// history only when they replace the position. Out-of-range offsets clamp to
// the first or last readable character.
void ReaderNavigationHistoryTest::character_jumps_record_the_resolved_origin() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.2);
    QCOMPARE(reader_->character_offset(), 2);

    reader_->jumpToCharacter(3);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->character_offset(), 3);
    QCOMPARE(reader_->back_target(), 2);
    QCOMPARE(reader_->back_target_text(), QStringLiteral("2"));
    QCOMPARE(reader_->forward_target(), -1);
    complete_restore(*reader_);

    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->character_offset(), 2);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), 3);

    reader_->jumpToCharacter(25);
    QCOMPARE(reader_->chapter_index(), 2);
    QCOMPARE(reader_->character_offset(), 25);
    QCOMPARE(reader_->back_target(), 2);
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(reader_->restoring());
    complete_restore(*reader_);

    reader_->jumpToCharacter(999);
    QCOMPARE(reader_->chapter_index(), 2);
    QCOMPARE(reader_->character_offset(), 999);
    QCOMPARE(reader_->chapter_progress(), 979.0 / 1200.0);
    QCOMPARE(reader_->back_target(), 25);
    complete_restore(*reader_);

    reader_->jumpToCharacter(9999);
    QCOMPARE(reader_->chapter_index(), 2);
    QCOMPARE(reader_->character_offset(), 1219);
    QCOMPARE(reader_->chapter_progress(), 1199.0 / 1200.0);
    QCOMPARE(reader_->back_target(), 999);
    QCOMPARE(reader_->forward_target(), -1);
    complete_restore(*reader_);

    reader_->jumpToCharacter(-5);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->character_offset(), 0);
    QCOMPARE(reader_->back_target(), 1219);
    QCOMPARE(reader_->forward_target(), -1);
    complete_restore(*reader_);

    QVERIFY(open_empty_book());
    const QString empty_url = reader_->chapter_url();
    QSignalSpy history(reader_, &ReaderViewModel::historyChanged);
    reader_->jumpToCharacter(0);
    QCOMPARE(reader_->chapter_url(), empty_url);
    QCOMPARE(history.size(), 0);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(!reader_->restoring());
}

// B4: an activated chapter link is handled internally when its normalized
// resource matches a spine item. The fragment is ignored for the match and
// decoded for the destination; other URLs keep their normal policy and leave
// history alone.
void ReaderNavigationHistoryTest::links_match_spine_resources_and_decode_the_fragment() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.3);

    QSignalSpy links(reader_, &ReaderViewModel::internalLinkJumpRequested);
    QSignalSpy history(reader_, &ReaderViewModel::historyChanged);
    QVERIFY(!reader_->jumpToLink(QUrl(QStringLiteral("https://example.com/first.xhtml#top"))));
    QVERIFY(!reader_->jumpToLink(QUrl(QStringLiteral("reader://book/missing.xhtml#top"))));
    QCOMPARE(links.size(), 0);
    QCOMPARE(history.size(), 0);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.3);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(!reader_->restoring());

    QVERIFY(
        reader_->jumpToLink(QUrl(QStringLiteral("reader://book/sub/../%66irst.xhtml#spot%20one"))));
    QCOMPARE(links.size(), 1);
    QCOMPARE(links.at(0).at(0).toString(), QStringLiteral("spot one"));
    QCOMPARE(history.size(), 1);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.3);
    QCOMPARE(reader_->back_target(), 3);
    QCOMPARE(reader_->back_target_text(), QStringLiteral("3"));
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(!reader_->restoring());
}

// B5: a same-spine link records the origin and clears forward before asking
// the page to jump in the open document. The reported progress becomes the
// programmatic destination without another history entry.
void ReaderNavigationHistoryTest::same_spine_links_jump_in_the_open_document() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.5);
    reader_->goToContentsEntry(1, QStringLiteral("spot"));
    complete_restore(*reader_);
    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), kFirstCharacters);

    QSignalSpy links(reader_, &ReaderViewModel::internalLinkJumpRequested);
    QVERIFY(reader_->jumpToLink(QUrl(QStringLiteral("reader://book/first.xhtml#top"))));
    QCOMPARE(links.size(), 1);
    QCOMPARE(links.at(0).at(0).toString(), QStringLiteral("top"));
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->back_target(), 5);
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(!reader_->restoring());

    reader_->syncProgressAfterProgrammaticJump(0.8);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.8);
    QCOMPARE(reader_->character_offset(), 8);
    QCOMPARE(reader_->back_target(), 5);
    QCOMPARE(reader_->forward_target(), -1);
    auto bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 0);
    QCOMPARE(bookmark->progress, 0.8);
    QCOMPARE(bookmark->character_count, 8);

    QVERIFY(reader_->jumpToLink(QUrl(QStringLiteral("reader://book/first.xhtml"))));
    QCOMPARE(links.size(), 2);
    QVERIFY(links.at(1).at(0).toString().isNull());
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.8);
    QCOMPARE(reader_->back_target(), 8);
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(!reader_->restoring());
    reader_->syncProgressAfterProgrammaticJump(0.0);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->character_offset(), 0);
    QCOMPARE(reader_->back_target(), 8);
    QCOMPARE(reader_->forward_target(), -1);
    bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 0);
    QCOMPARE(bookmark->progress, 0.0);
    QCOMPARE(bookmark->character_count, 0);

    QVERIFY(reader_->jumpToLink(QUrl(QStringLiteral("reader://book/first.xhtml#"))));
    QCOMPARE(links.size(), 3);
    QVERIFY(!links.at(2).at(0).toString().isNull());
    QVERIFY(links.at(2).at(0).toString().isEmpty());
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->back_target(), 0);
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(!reader_->restoring());
    reader_->syncProgressAfterProgrammaticJump(0.6);
    QCOMPARE(reader_->chapter_progress(), 0.6);
    QCOMPARE(reader_->character_offset(), 6);
    QCOMPARE(reader_->back_target(), 0);
    QCOMPARE(reader_->forward_target(), -1);
    bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 0);
    QCOMPARE(bookmark->progress, 0.6);
    QCOMPARE(bookmark->character_count, 6);
}

// B6: a link into another spine records the origin, clears forward and loads
// the target at zero, keeping the decoded fragment for the restore.
void ReaderNavigationHistoryTest::cross_spine_links_load_the_target_at_zero() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.5);
    reader_->goToContentsEntry(2, QString());
    complete_restore(*reader_);
    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->forward_target(), 20);

    QSignalSpy links(reader_, &ReaderViewModel::internalLinkJumpRequested);
    QVERIFY(reader_->jumpToLink(QUrl(QStringLiteral("reader://book/second.xhtml#spot%20one"))));
    QCOMPARE(links.size(), 0);
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->back_target(), 5);
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(reader_->restoring());
    const QString document = chapter_document(*reader_, QStringLiteral("second.xhtml"));
    QVERIFY(document.contains(
        QStringLiteral("return window.hoshiReader.jumpToFragment(\"spot one\")")));
    QVERIFY(!document.contains(QStringLiteral("return window.hoshiReader.restoreProgress(")));
    auto bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 1);
    QCOMPARE(bookmark->progress, 0.0);
    QCOMPARE(bookmark->character_count, kFirstCharacters);

    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->back_target(), 5);
    QCOMPARE(reader_->forward_target(), -1);

    QVERIFY(reader_->jumpToLink(QUrl(QStringLiteral("reader://book/third.xhtml"))));
    QCOMPARE(links.size(), 0);
    QCOMPARE(reader_->chapter_index(), 2);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->back_target(), 10);
    QCOMPARE(reader_->forward_target(), -1);
    QVERIFY(reader_->restoring());
    const QString third_document = chapter_document(*reader_, QStringLiteral("third.xhtml"));
    QVERIFY(!third_document.contains(QStringLiteral("return window.hoshiReader.jumpToFragment(")));
    QVERIFY(
        third_document.contains(QStringLiteral("return window.hoshiReader.restoreProgress(0)")));
    bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 2);
    QCOMPARE(bookmark->progress, 0.0);
    QCOMPARE(bookmark->character_count, 20);

    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 2);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->back_target(), 10);
    QCOMPARE(reader_->forward_target(), -1);
}

// B7: back and forward pop their own stack in LIFO order, push the exact
// current position onto the other one, and restore the popped chapter without
// a fragment. The restore path is programmatic, so it is invisible to reading
// statistics.
void ReaderNavigationHistoryTest::back_and_forward_restore_the_popped_position() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.4);
    reader_->goToContentsEntry(1, QStringLiteral("spot"));
    complete_restore(*reader_);
    reader_->updateChapterProgress(0.5);
    QCOMPARE(reader_->character_offset(), kFirstCharacters + 5);
    reader_->goToContentsEntry(0, QString());
    complete_restore(*reader_);
    QCOMPARE(reader_->back_target(), kFirstCharacters + 5);
    QCOMPARE(reader_->back_target_text(), QStringLiteral("15"));

    reader_->navigateBackwards();
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->back_target(), 4);
    QCOMPARE(reader_->back_target_text(), QStringLiteral("4"));
    QCOMPARE(reader_->forward_target(), 0);
    QVERIFY(reader_->restoring());
    const QString cross_spine_document = chapter_document(*reader_, QStringLiteral("second.xhtml"));
    QVERIFY(!cross_spine_document.contains(
        QStringLiteral("return window.hoshiReader.jumpToFragment(")));
    QVERIFY(std::abs(initial_restore_progress(cross_spine_document) - 0.5) < 1e-9);
    complete_restore(*reader_);
    auto bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 1);
    QCOMPARE(bookmark->progress, 0.5);
    QCOMPARE(bookmark->character_count, kFirstCharacters + 5);

    reader_->updateChapterProgress(0.6);
    reader_->navigateForwards();
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->back_target(), 16);
    QCOMPARE(reader_->forward_target(), -1);
    complete_restore(*reader_);

    reader_->updateChapterProgress(0.4);
    reader_->goToContentsEntry(0, QStringLiteral("top"));
    complete_restore(*reader_);
    QCOMPARE(reader_->back_target(), 4);
    reader_->navigateBackwards();
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.4);
    QCOMPARE(reader_->back_target(), 16);
    QCOMPARE(reader_->forward_target(), 0);
    QCOMPARE(reader_->forward_target_text(), QStringLiteral("0"));
    QVERIFY(reader_->restoring());
    complete_restore(*reader_);
    bookmark = stored_bookmark();
    QVERIFY(bookmark.has_value());
    QCOMPARE(bookmark->chapter_index, 0);
    QCOMPARE(bookmark->progress, 0.4);
    QCOMPARE(bookmark->character_count, 4);
}

// B7: restoring through history establishes a programmatic destination, so
// the characters crossed by the jump are not credited as read.
void ReaderNavigationHistoryTest::programmatic_restores_do_not_credit_reading() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(write_global_settings(true, QStringLiteral("On")));
    QVERIFY(open_book());
    reader_->setReaderActive(true);
    QVERIFY(reader_->tracking());

    reader_->goToContentsEntry(1, QString());
    complete_restore(*reader_);
    wait_for_clock_tick();
    reader_->updateChapterProgress(0.5);
    QCOMPARE(reader_->statistics_today_characters(), 5);

    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.0);
    QCOMPARE(reader_->statistics_today_characters(), 5);
    wait_for_clock_tick();

    reader_->navigateForwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 1);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->statistics_today_characters(), 5);
    wait_for_clock_tick();
    reader_->updateChapterProgress(0.6);
    QCOMPARE(reader_->statistics_today_characters(), 6);
}

// B8: a manual page turn clears forward history and never adds a back entry.
void ReaderNavigationHistoryTest::manual_navigation_clears_forward_without_adding_back() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.2);
    reader_->goToContentsEntry(1, QString());
    complete_restore(*reader_);
    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), kFirstCharacters);

    reader_->handlePageTurnResult(true, QVariant(0.3));
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.3);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), -1);
}

// B8: continuous scrolling is a manual navigation too, so the progress message
// clears forward history without adding a back entry.
void ReaderNavigationHistoryTest::continuous_scrolling_clears_forward_without_adding_back() {
    auto settings = niratan::profiles::default_reader_settings();
    settings.continuous_mode = true;
    QVERIFY(write_reader(settings));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.2);
    reader_->goToContentsEntry(1, QString());
    complete_restore(*reader_);
    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), kFirstCharacters);

    send(*reader_, QStringLiteral("progressChanged"),
         {{"token", revision(*reader_)}, {"progress", 0.4}});
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.4);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), -1);
}

// B8: every valid numeric continuous report is a manual navigation, even when
// the position did not change, so it clears forward before any threshold.
void ReaderNavigationHistoryTest::continuous_report_at_the_same_position_clears_forward() {
    auto settings = niratan::profiles::default_reader_settings();
    settings.continuous_mode = true;
    QVERIFY(write_reader(settings));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.2);
    reader_->goToContentsEntry(1, QString());
    complete_restore(*reader_);
    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.2);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), kFirstCharacters);

    send(*reader_, QStringLiteral("progressChanged"),
         {{"token", revision(*reader_)}, {"progress", 0.2}});
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.2);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), -1);
}

// B8: a synced bookmark is programmatic and leaves the forward stack alone.
void ReaderNavigationHistoryTest::synced_bookmarks_keep_forward() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.2);
    reader_->goToContentsEntry(1, QString());
    complete_restore(*reader_);
    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), kFirstCharacters);

    reader_->syncBookmarkToProgress(0, 0.5);
    QCOMPARE(reader_->chapter_index(), 0);
    QCOMPARE(reader_->chapter_progress(), 0.5);
    QCOMPARE(reader_->character_offset(), 5);
    QCOMPARE(reader_->back_target(), -1);
    QCOMPARE(reader_->forward_target(), kFirstCharacters);
}

// B9: a target is the chapter prefix plus the truncated chapter size times
// progress. Its label is the active profile's ungrouped display count; zero is
// a real target and changing the profile refreshes the text.
void ReaderNavigationHistoryTest::history_targets_use_the_profile_display_count() {
    QVERIFY(write_reader(niratan::profiles::default_reader_settings()));
    QVERIFY(open_book());
    reader_->updateChapterProgress(0.2);
    reader_->goToContentsEntry(1, QString());
    complete_restore(*reader_);
    reader_->navigateBackwards();
    complete_restore(*reader_);
    QCOMPARE(reader_->forward_target(), 10);
    QCOMPARE(reader_->forward_target_text(), QStringLiteral("10"));

    reader_->goToContentsEntry(2, QString());
    complete_restore(*reader_);
    QCOMPARE(reader_->chapter_index(), 2);
    reader_->updateChapterProgress(0.829);
    QCOMPARE(reader_->chapter_progress(), 0.829);
    QCOMPARE(reader_->character_offset(), 1014);
    reader_->goToContentsEntry(0, QString());
    complete_restore(*reader_);
    QCOMPARE(reader_->back_target(), 1014);
    QCOMPARE(reader_->back_target_text(), QStringLiteral("1014"));
    QCOMPARE(reader_->forward_target(), -1);

    auto repository = niratan::profiles::ProfileRepository::at_data_root();
    QVERIFY(repository.has_value());
    const auto created = repository->create_profile(
        "English", niratan::profiles::ContentLanguage::english, std::nullopt);
    QVERIFY(created.profile.has_value());
    QVERIFY(!repository->set_active_profile(created.profile->id).has_value());
    QSignalSpy history(reader_, &ReaderViewModel::historyChanged);
    QTRY_VERIFY(reader_->english_content());
    if (reader_->restoring()) {
        complete_restore(*reader_);
    }
    QTRY_COMPARE(reader_->back_target_text(), QStringLiteral("203"));
    QTRY_VERIFY(history.size() > 0);
    QCOMPARE(reader_->back_target(), 1014);
    QCOMPARE(reader_->forward_target(), -1);
}

QTEST_GUILESS_MAIN(ReaderNavigationHistoryTest)
#include "tst_reader_navigation_history.moc"
