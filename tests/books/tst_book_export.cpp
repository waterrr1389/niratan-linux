// SPDX-License-Identifier: GPL-3.0-or-later
//
// EPUB export: NovelExportFileName.CreateBaseName and
// NovelLibraryService.ExportEpubAsync from Niratan-win, reached through
// BookStorage.export_epub and the book's stored EPUB name
// (BookCell.bookExportURL). The private files stay untouched: export only
// copies bytes to a chosen destination.

#include "services/books/book_storage.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

using niratan::books::BookMetadata;
using niratan::books::BookStorage;
using niratan::books::export_base_name;
using niratan::books::ExportOutcome;
using niratan::books::ExportResult;

namespace {

std::filesystem::path build_dir(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString()) / "app";
}

std::optional<BookMetadata> stored_book(const BookStorage& storage, const std::string& epub,
                                        bool write_file) {
    BookMetadata book;
    book.id = "BOOK-ID";
    book.title = "Story";
    book.folder = "story";
    book.epub = epub;
    book.character_count = 100;
    if (!storage.save_metadata(book)) {
        return std::nullopt;
    }
    const auto folder = storage.book_directory(book.folder);
    if (!folder.has_value()) {
        return std::nullopt;
    }
    std::error_code error;
    std::filesystem::create_directories(*folder, error);
    if (error) {
        return std::nullopt;
    }
    if (write_file && !epub.empty()) {
        std::ofstream(*folder / epub) << "EPUB-BYTES";
    }
    return book;
}

}  // namespace

class BookExportTest : public QObject {
    Q_OBJECT

   private slots:
    void base_name_follows_the_niratan_win_rules();
    void export_copies_the_private_epub_without_touching_the_library();
    void export_refuses_invalid_destinations_data();
    void export_refuses_invalid_destinations();
    void export_refuses_the_source_itself();
};

// NovelExportFileName.CreateBaseName: trim, strip every trailing ".epub"
// case-insensitively, replace Windows-invalid and control characters, trim,
// drop trailing dots, and fall back to "book".
void BookExportTest::base_name_follows_the_niratan_win_rules() {
    QCOMPARE(export_base_name("  My Book  "), std::string("My Book"));
    QCOMPARE(export_base_name("Title.epub"), std::string("Title"));
    QCOMPARE(export_base_name("Title.EPUB.epub"), std::string("Title"));
    QCOMPARE(export_base_name("a\\b/c:d*e?f\"g<h>i|j"), std::string("a_b_c_d_e_f_g_h_i_j"));
    QCOMPARE(export_base_name("line\nbreak"), std::string("line_break"));
    QCOMPARE(export_base_name("Book... "), std::string("Book"));
    QCOMPARE(export_base_name(" . "), std::string("book"));
    QCOMPARE(export_base_name("   "), std::string("book"));
    QCOMPARE(export_base_name(".epub"), std::string("book"));
    QCOMPARE(export_base_name("\u3000My Book\u3000"), std::string("My Book"));
    QCOMPARE(export_base_name("\u3000.epub\u3000"), std::string("book"));
    QCOMPARE(export_base_name("日本語"), std::string("日本語"));
}

// ExportEpubAsync copies the stored EPUB and leaves metadata, order and
// shelves alone.
void BookExportTest::export_copies_the_private_epub_without_touching_the_library() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const BookStorage storage(build_dir(directory));
    const auto book = stored_book(storage, "story.epub", true);
    QVERIFY(book.has_value());
    QVERIFY(storage.save_book_order({book->id}));
    QVERIFY(storage.save_shelves({{"Shelf", {book->id}}}));
    const auto books_before = storage.load_all_books();
    const auto order_before = storage.load_book_order();
    const auto shelves_before = storage.load_shelves();

    const std::filesystem::path destination = build_dir(directory) / "exports" / "out.epub";
    std::filesystem::create_directories(destination.parent_path());
    const ExportResult result = storage.export_epub(*book, destination);

    QCOMPARE(int(result.outcome), int(ExportOutcome::exported));
    QVERIFY(result.error.empty());
    QVERIFY(std::filesystem::exists(destination));
    std::ifstream copied(destination);
    const std::string bytes{std::istreambuf_iterator<char>(copied),
                            std::istreambuf_iterator<char>()};
    QCOMPARE(bytes, std::string("EPUB-BYTES"));
    QCOMPARE(storage.load_all_books(), books_before);
    QCOMPARE(storage.load_book_order(), order_before);
    QCOMPARE(storage.load_shelves(), shelves_before);
    const auto private_path = storage.epub_path(*book);
    QVERIFY(private_path.has_value());
    QVERIFY(std::filesystem::exists(*private_path));
}

void BookExportTest::export_refuses_invalid_destinations_data() {
    QTest::addColumn<QString>("epub");
    QTest::addColumn<bool>("write_file");
    QTest::addColumn<QString>("destination");
    QTest::addColumn<int>("outcome");
    QTest::newRow("missing extension")
        << "story.epub" << true << "out.txt" << int(ExportOutcome::invalid_extension);
    QTest::newRow("empty destination")
        << "story.epub" << true << "" << int(ExportOutcome::invalid_extension);
    QTest::newRow("missing private epub")
        << "missing.epub" << false << "out.epub" << int(ExportOutcome::missing_source);
    QTest::newRow("no epub name") << "" << false << "out.epub"
                                  << int(ExportOutcome::missing_source);
    QTest::newRow("missing folder")
        << "story.epub" << true << "missing/out.epub" << int(ExportOutcome::missing_directory);
}

void BookExportTest::export_refuses_invalid_destinations() {
    QFETCH(QString, epub);
    QFETCH(bool, write_file);
    QFETCH(QString, destination);
    QFETCH(int, outcome);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const BookStorage storage(build_dir(directory));
    const auto book = stored_book(storage, epub.toStdString(), write_file);
    QVERIFY(book.has_value());
    const std::filesystem::path target = build_dir(directory) / destination.toStdString();

    const ExportResult result = storage.export_epub(*book, target);

    QCOMPARE(int(result.outcome), outcome);
    QVERIFY(!result.error.empty());
    // The empty destination names the app directory, which already exists.
    if (!destination.isEmpty()) {
        QVERIFY(!std::filesystem::exists(target));
    }
}

// The same file compared case-insensitively is still refused, and the private
// EPUB is not overwritten.
void BookExportTest::export_refuses_the_source_itself() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const BookStorage storage(build_dir(directory));
    const auto book = stored_book(storage, "story.epub", true);
    QVERIFY(book.has_value());
    const auto source = storage.epub_path(*book);
    QVERIFY(source.has_value());
    std::filesystem::path upper = *source;
    upper.replace_filename("STORY.EPUB");

    const ExportResult result = storage.export_epub(*book, upper);

    QCOMPARE(int(result.outcome), int(ExportOutcome::same_as_source));
    QVERIFY(!result.error.empty());
    QVERIFY(std::filesystem::exists(*source));
    QCOMPARE(std::filesystem::file_size(*source), std::uintmax_t(10));
}

QTEST_GUILESS_MAIN(BookExportTest)
#include "tst_book_export.moc"
