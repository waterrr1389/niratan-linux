// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/books/book_storage.h"

#include <QTemporaryDir>
#include <QtTest>

#include <zip.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

using niratan::books::BookInfo;
using niratan::books::BookMetadata;
using niratan::books::BookShelf;
using niratan::books::BookStorage;
using niratan::books::Bookmark;
using niratan::books::sanitize_file_name;

// Core/BookStorage.swift and BookshelfViewModel.processImport define the
// directory layout and the import guarantees; the fixture is a minimal EPUB.
namespace {

class ZipWriter {
   public:
    explicit ZipWriter(const std::filesystem::path& path) {
        int error = 0;
        zip_ = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    }
    ~ZipWriter() {
        if (zip_ != nullptr) {
            zip_close(zip_);
        }
    }
    ZipWriter(const ZipWriter&) = delete;
    ZipWriter& operator=(const ZipWriter&) = delete;

    bool valid() const { return zip_ != nullptr; }

    void add(const std::string& name, std::string data) {
        if (zip_ == nullptr) {
            return;
        }
        storage_.push_back(std::move(data));
        std::string& stored = storage_.back();
        zip_source_t* source = zip_source_buffer(zip_, stored.data(), stored.size(), 0);
        if (source == nullptr) {
            return;
        }
        zip_file_add(zip_, name.c_str(), source, ZIP_FL_OVERWRITE);
    }

    void close() {
        if (zip_ != nullptr) {
            zip_close(zip_);
            zip_ = nullptr;
        }
    }

   private:
    zip_t* zip_ = nullptr;
    std::vector<std::string> storage_;
};

std::filesystem::path make_epub(const QTemporaryDir& directory, const std::string& name,
                                const std::string& title) {
    const std::filesystem::path path = std::filesystem::path(directory.path().toStdString()) / name;
    ZipWriter zip(path);
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml",
            "<?xml version=\"1.0\"?><container version=\"1.0\" "
            "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
            "<rootfiles><rootfile full-path=\"OEBPS/content.opf\" "
            "media-type=\"application/oebps-package+xml\"/></rootfiles></container>");
    zip.add("OEBPS/content.opf",
            "<?xml version=\"1.0\"?><package xmlns=\"http://www.idpf.org/2007/opf\" "
            "version=\"3.0\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "<dc:title>" +
                title +
                "</dc:title><dc:language>ja</dc:language></metadata><manifest>"
                "<item id=\"c\" href=\"text/ch.xhtml\" media-type=\"application/xhtml+xml\"/>"
                "<item id=\"cover\" href=\"images/cover.png\" media-type=\"image/png\" "
                "properties=\"cover-image\"/>"
                "</manifest><spine><itemref idref=\"c\"/></spine></package>");
    zip.add("OEBPS/text/ch.xhtml", "<html><body><p>本文です。</p></body></html>");
    zip.add("OEBPS/images/cover.png", "PNG");
    zip.close();
    return path;
}

std::filesystem::path app_dir(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString()) / "app";
}

}  // namespace

class BookStorageTest : public QObject {
    Q_OBJECT

   private slots:
    void file_names_replace_forbidden_characters();
    void importing_a_book_writes_its_files();
    void importing_the_same_title_reports_a_duplicate();
    void importing_a_broken_epub_reports_an_error();
    void metadata_bookmarks_and_info_round_trip();
    void book_order_and_shelves_round_trip();
    void unsafe_folders_are_rejected();
    void books_without_metadata_are_skipped();
    void book_directories_must_exist();
    void sasayaki_sidecars_round_trip();
    void missing_or_undecodable_sasayaki_sidecars_load_as_none();
};

void BookStorageTest::file_names_replace_forbidden_characters() {
    QCOMPARE(sanitize_file_name("a/b\\c:d*e?f\"g<h>i|j"), std::string("a_b_c_d_e_f_g_h_i_j"));
    QCOMPARE(sanitize_file_name("  spaced  "), std::string("spaced"));
    QCOMPARE(sanitize_file_name("line\nbreak"), std::string("line_break"));
    QCOMPARE(sanitize_file_name("日本語"), std::string("日本語"));
}

void BookStorageTest::importing_a_book_writes_its_files() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path source = make_epub(directory, "source.epub", "My Book");
    BookStorage storage(app_dir(directory));

    const auto result = storage.import_book(source);

    QVERIFY2(result.book.has_value(), result.error.c_str());
    QVERIFY(!result.already_exists);
    QCOMPARE(result.book->title, std::string("My Book"));
    QCOMPARE(result.book->folder, std::string("My Book"));
    QCOMPARE(result.book->book_language, std::optional<std::string>{std::string("ja")});
    QVERIFY(result.book->epub.has_value());
    QVERIFY(std::filesystem::exists(app_dir(directory) / "Books" / "My Book" /
                                    *result.book->epub));

    const auto cover = storage.cover_path(*result.book);
    QVERIFY(cover.has_value());
    QVERIFY(std::filesystem::exists(*cover));
    QCOMPARE(result.book->cover, std::optional<std::string>{std::string("Books/My Book/cover.png")});

    const auto info = storage.load_book_info("My Book");
    QVERIFY(info.has_value());
    QCOMPARE(info->character_count, 4);

    const auto loaded = storage.load_all_books();
    QCOMPARE(loaded.size(), 1u);
    QCOMPARE(loaded.front().id, result.book->id);
}

void BookStorageTest::importing_the_same_title_reports_a_duplicate() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path source = make_epub(directory, "source.epub", "Same");
    BookStorage storage(app_dir(directory));

    QVERIFY(storage.import_book(source).book.has_value());
    const auto second = storage.import_book(source);

    QVERIFY(!second.book.has_value());
    QVERIFY(second.already_exists);
    QCOMPARE(storage.load_all_books().size(), 1u);
}

void BookStorageTest::importing_a_broken_epub_reports_an_error() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path source =
        std::filesystem::path(directory.path().toStdString()) / "broken.epub";
    std::ofstream(source) << "not an epub";
    BookStorage storage(app_dir(directory));

    const auto result = storage.import_book(source);

    QVERIFY(!result.book.has_value());
    QVERIFY(!result.error.empty());
    QVERIFY(!std::filesystem::exists(app_dir(directory) / "Books" / "broken"));
}

void BookStorageTest::metadata_bookmarks_and_info_round_trip() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookStorage storage(app_dir(directory));

    BookMetadata book;
    book.id = "0F0F0F0F-1111-2222-3333-444444444444";
    book.title = "Title";
    book.folder = "Title";
    book.last_access = 700000000.0;
    std::filesystem::create_directories(app_dir(directory) / "Books" / "Title");
    QVERIFY(storage.save_metadata(book));
    QCOMPARE(storage.load_metadata("Title"), std::optional<BookMetadata>{book});

    Bookmark bookmark;
    bookmark.chapter_index = 2;
    bookmark.progress = 0.5;
    bookmark.character_count = 100;
    QVERIFY(storage.save_bookmark("Title", bookmark));
    QCOMPARE(storage.load_bookmark("Title"), std::optional<Bookmark>{bookmark});

    BookInfo info;
    info.character_count = 100;
    QVERIFY(storage.save_book_info("Title", info));
    QCOMPARE(storage.load_book_info("Title"), std::optional<BookInfo>{info});
}

void BookStorageTest::book_order_and_shelves_round_trip() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookStorage storage(app_dir(directory));

    const std::vector<std::string> order{"a", "b"};
    QVERIFY(storage.save_book_order(order));
    QCOMPARE(storage.load_book_order(), order);

    const std::vector<BookShelf> shelves{BookShelf{"Shelf", {"a"}}};
    QVERIFY(storage.save_shelves(shelves));
    QCOMPARE(storage.load_shelves(), shelves);
}

void BookStorageTest::unsafe_folders_are_rejected() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookStorage storage(app_dir(directory));

    QVERIFY(!storage.book_directory("../escape").has_value());
    QVERIFY(!storage.book_directory("/absolute").has_value());
    QVERIFY(storage.book_directory("fine").has_value());
    QVERIFY(!storage.load_metadata("../escape").has_value());
    QVERIFY(!storage.save_bookmark("../escape", Bookmark{}));
}

void BookStorageTest::books_without_metadata_are_skipped() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookStorage storage(app_dir(directory));
    std::filesystem::create_directories(app_dir(directory) / "Books" / "Empty");
    std::filesystem::create_directories(app_dir(directory) / "Books" / "Damaged");
    std::ofstream(app_dir(directory) / "Books" / "Damaged" / "metadata.json") << "{}";

    QVERIFY(storage.load_all_books().empty());
}

void BookStorageTest::book_directories_must_exist() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookStorage storage(app_dir(directory));
    std::filesystem::create_directories(app_dir(directory) / "Books" / "Title");
    std::ofstream(app_dir(directory) / "Books" / "File") << "{}";

    QVERIFY(storage.has_book_directory("Title"));
    QVERIFY(!storage.has_book_directory("Missing"));
    QVERIFY(!storage.has_book_directory("File"));
    QVERIFY(!storage.has_book_directory("../escape"));
}

// BookStorage.loadSasayakiPlayback / loadSasayakiMatch and
// BookStorage.save(_:inside:as:) with FileNames.sasayakiPlayback and
// FileNames.sasayakiMatch.
void BookStorageTest::sasayaki_sidecars_round_trip() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookStorage storage(app_dir(directory));
    const auto folder = app_dir(directory) / "Books" / "Title";
    std::filesystem::create_directories(folder);

    niratan::sasayaki::SasayakiPlaybackData playback;
    playback.last_position = 12.5;
    playback.delay = -0.25;
    playback.rate = 1.5F;
    playback.audio_bookmark = QByteArray("/music/book.m4b");
    QVERIFY(storage.save_sasayaki_playback("Title", playback));
    QVERIFY(std::filesystem::exists(folder / "sasayaki_playback.json"));
    QCOMPARE(storage.load_sasayaki_playback("Title"),
             std::optional<niratan::sasayaki::SasayakiPlaybackData>{playback});

    niratan::sasayaki::SasayakiMatchData match;
    niratan::sasayaki::SasayakiMatch entry;
    entry.id = "1";
    entry.start_time = 1.0;
    entry.end_time = 2.0;
    entry.text = "text";
    entry.chapter_index = 3;
    entry.start = 10;
    entry.length = 4;
    match.matches.push_back(entry);
    match.unmatched = 2;
    QVERIFY(storage.save_sasayaki_match("Title", match));
    QVERIFY(std::filesystem::exists(folder / "sasayaki_match.json"));
    QCOMPARE(storage.load_sasayaki_match("Title"),
             std::optional<niratan::sasayaki::SasayakiMatchData>{match});

    QVERIFY(!storage.save_sasayaki_playback("../escape", playback));
    std::string reason;
    QVERIFY(!storage.save_sasayaki_match("../escape", match, &reason));
}

void BookStorageTest::missing_or_undecodable_sasayaki_sidecars_load_as_none() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BookStorage storage(app_dir(directory));
    const auto folder = app_dir(directory) / "Books" / "Title";
    std::filesystem::create_directories(folder);

    QVERIFY(!storage.load_sasayaki_playback("Title").has_value());
    QVERIFY(!storage.load_sasayaki_match("Title").has_value());

    std::ofstream(folder / "sasayaki_playback.json") << "{ damaged";
    std::ofstream(folder / "sasayaki_match.json") << R"({"matches": 3})";
    QVERIFY(!storage.load_sasayaki_playback("Title").has_value());
    QVERIFY(!storage.load_sasayaki_match("Title").has_value());
}

QTEST_APPLESS_MAIN(BookStorageTest)
#include "tst_book_storage.moc"
