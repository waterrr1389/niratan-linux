// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/book_session.h"

#include <QTemporaryDir>
#include <QtTest>

#include <zip.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using niratan::books::BookInfo;
using niratan::reader::BookSession;
using niratan::reader::container_mime_type;
using niratan::reader::ReaderScripts;
using niratan::reader::ShellInput;

// Util/BookProcessor.swift and Core/BookStorage.swift define the chapter
// accounting; the fixture is a minimal container with two chapters and a
// nested table of contents.
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

std::string chapter_one() {
    return "<html><body><p id=\"start\">第一章です。</p><p>続き</p></body></html>";
}

std::string chapter_two() {
    return "<html><body><p>Second <ruby>漢<rt>かん</rt></ruby>字</p></body></html>";
}

std::filesystem::path make_book(const QTemporaryDir& directory, bool nested = false) {
    const std::filesystem::path path =
        std::filesystem::path(directory.path().toStdString()) / "book.epub";
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
            "<dc:title>Book</dc:title></metadata><manifest>"
            "<item id=\"c1\" href=\"text/one.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "<item id=\"c2\" href=\"text/two.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" "
            "properties=\"nav\"/>"
            "<item id=\"style\" href=\"styles/book.css\" media-type=\"text/css\"/>"
            "<item id=\"pic\" href=\"images/pic.png\" media-type=\"image/png\"/>"
            "</manifest><spine><itemref idref=\"c1\"/><itemref idref=\"c2\"/></spine>"
            "</package>");
    zip.add("OEBPS/nav.xhtml",
            "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
            "xmlns:epub=\"http://www.idpf.org/2007/ops\"><body><nav epub:type=\"toc\"><ol>"
            "<li><a href=\"text/one.xhtml\">Chapter One</a>" +
            std::string(nested ? "<ol><li><a href=\"text/one.xhtml#middle\">Part Two</a></li></ol>" : "") +
            "</li>"
            "<li><a href=\"text/two.xhtml#mark\">Chapter Two</a></li>"
            "</ol></nav></body></html>");
    zip.add("OEBPS/text/one.xhtml", nested ? "<html><body><p>abc</p><p id=\"middle\">defg</p></body></html>"
                                         : chapter_one());
    zip.add("OEBPS/text/two.xhtml", chapter_two());
    zip.add("OEBPS/styles/book.css", "p { color: #000; }");
    zip.add("OEBPS/images/pic.png", "PNG");
    zip.close();
    return path;
}

ReaderScripts scripts() {
    ReaderScripts result;
    result.selection = "/* selection */";
    result.reader = "/* reader */";
    result.scrollreader = "/* scrollreader */";
    result.highlights = "/* highlights */";
    result.paragraph = "/* paragraph */";
    result.webchannel = "/* webchannel */";
    return result;
}

}  // namespace

class BookSessionTest : public QObject {
    Q_OBJECT

   private slots:
    void spine_and_character_counts_are_built();
    void chapter_titles_come_from_the_table_of_contents();
    void fragment_offsets_are_recorded();
    void chapter_positions_map_to_character_offsets();
    void chapter_remaining_stops_at_the_toc_boundary();
    void logical_chapter_progress_follows_toc_fragments();
    void resources_are_served_with_mime_types();
    void chapter_documents_are_injected();
    void an_unreadable_chapter_is_skipped();
    void a_missing_book_fails();
};

void BookSessionTest::spine_and_character_counts_are_built() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    std::string error;
    const auto session = BookSession::open(make_book(directory), &error);
    QVERIFY2(session.has_value(), error.c_str());

    QCOMPARE(session->spine_count(), 2);
    QCOMPARE(session->spine_path(0), std::string("OEBPS/text/one.xhtml"));
    QCOMPARE(session->spine_path(1), std::string("OEBPS/text/two.xhtml"));

    const BookInfo& info = session->info();
    QCOMPARE(info.character_count, 7 + 8);
    QCOMPARE(info.chapter_info.at("OEBPS/text/one.xhtml").current_total, 0);
    QCOMPARE(info.chapter_info.at("OEBPS/text/one.xhtml").chapter_count, 7);
    QCOMPARE(info.chapter_info.at("OEBPS/text/two.xhtml").current_total, 7);
    QCOMPARE(info.chapter_info.at("OEBPS/text/two.xhtml").chapter_count, 8);
    QCOMPARE(info.chapter_info.at("OEBPS/text/two.xhtml").spine_index, 1);
}

void BookSessionTest::chapter_titles_come_from_the_table_of_contents() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto session = BookSession::open(make_book(directory));
    QVERIFY(session.has_value());

    QCOMPARE(session->chapter_title(0), std::string("Chapter One"));
    QCOMPARE(session->chapter_title(1), std::string("Chapter Two"));
    QCOMPARE(session->chapter_title(5), std::string(""));
}

void BookSessionTest::fragment_offsets_are_recorded() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto session = BookSession::open(make_book(directory));
    QVERIFY(session.has_value());

    const auto offsets = session->info().chapter_info.at("OEBPS/text/two.xhtml").fragment_offsets;
    QVERIFY(offsets.has_value());
    QVERIFY(offsets->contains("mark"));
}

void BookSessionTest::chapter_positions_map_to_character_offsets() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto session = BookSession::open(make_book(directory));
    QVERIFY(session.has_value());

    QCOMPARE(session->character_offset(0, 0.0), 0);
    QCOMPARE(session->character_offset(0, 0.5), 3);
    QCOMPARE(session->character_offset(1, 0.5), 11);
    QCOMPARE(session->character_offset(1, 2.0), 15);

    const auto position = session->position_for_character(11);
    QVERIFY(position.has_value());
    QCOMPARE(position->first, 1);
    QCOMPARE(position->second, 0.5);
}

// NativeReaderView.displayedChapterCharactersRemaining and
// ReaderChapterIndex.ChapterRange.remaining: the table-of-contents ranges are
// [0, 7) and [7, 15) for the fixture, and the end of a chapter reports zero
// instead of the next chapter's count.
void BookSessionTest::chapter_remaining_stops_at_the_toc_boundary() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto session = BookSession::open(make_book(directory));
    QVERIFY(session.has_value());

    QCOMPARE(session->chapter_characters_remaining(0, 0.0), 7);
    QCOMPARE(session->chapter_characters_remaining(0, 0.5), 4);
    QCOMPARE(session->chapter_characters_remaining(0, 1.0), 0);
    QCOMPARE(session->chapter_characters_remaining(1, 0.0), 8);
    QCOMPARE(session->chapter_characters_remaining(1, 0.5), 4);
    QCOMPARE(session->chapter_characters_remaining(1, 2.0), 0);
}

// ReaderChapterIndex.chapterStart and NativeReaderView.progressString use TOC
// fragments, even when two logical chapters share the same spine item.
void BookSessionTest::logical_chapter_progress_follows_toc_fragments() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto session = BookSession::open(make_book(directory, true));
    QVERIFY(session.has_value());
    const auto& contents = session->contents();
    QCOMPARE(contents.size(), 3U);
    QCOMPARE(session->contents_character_start(contents[0]), std::optional{0});
    QCOMPARE(session->contents_character_start(contents[1]), std::optional{3});
    QCOMPARE(session->logical_chapter_progress(0, 0.4), 2.0 / 3.0);
    QCOMPARE(session->logical_chapter_progress(0, 0.5), 0.0);
    QCOMPARE(session->logical_chapter_progress(0, 0.75), 0.5);
    QCOMPARE(session->logical_chapter_progress(0, 1.0), 1.0);
}

void BookSessionTest::resources_are_served_with_mime_types() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto session = BookSession::open(make_book(directory));
    QVERIFY(session.has_value());

    const auto image = session->resource("OEBPS/images/pic.png");
    QVERIFY(image.has_value());
    QCOMPARE(image->bytes, std::string("PNG"));
    QCOMPARE(image->mime_type, std::string("image/png"));

    const auto style = session->resource("OEBPS/styles/book.css");
    QVERIFY(style.has_value());
    QCOMPARE(style->mime_type, std::string("text/css"));

    QVERIFY(!session->resource("OEBPS/missing.png").has_value());
    QVERIFY(!session->resource("../../etc/passwd").has_value());

    QCOMPARE(container_mime_type("a.xhtml"), std::string("application/xhtml+xml"));
    QCOMPARE(container_mime_type("a.svg"), std::string("image/svg+xml"));
    QCOMPARE(container_mime_type("a.otf"), std::string("font/otf"));
    QCOMPARE(container_mime_type("a.unknown-extension"), std::string("application/octet-stream"));
}

void BookSessionTest::chapter_documents_are_injected() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto session = BookSession::open(make_book(directory));
    QVERIFY(session.has_value());

    ShellInput input;
    input.appearance.selected_font = "Noto Serif JP";
    input.page = niratan::reader::PageSize{800, 600};
    input.colors = niratan::reader::Colors{"#111111", "#FAFAFA", "#222222", "#EEEEEE"};
    input.content_language = "ja";

    const std::string document = session->chapter_document(0, input, scripts());
    QVERIFY(document.find("第一章") != std::string::npos);
    QVERIFY(document.find("writing-mode: vertical-rl") != std::string::npos);
    QVERIFY(document.find("/* reader */") != std::string::npos);
    QVERIFY(session->chapter_document(9, input, scripts()).empty());
}

void BookSessionTest::an_unreadable_chapter_is_skipped() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path path =
        std::filesystem::path(directory.path().toStdString()) / "broken.epub";
    ZipWriter zip(path);
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml",
            "<?xml version=\"1.0\"?><container version=\"1.0\" "
            "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
            "<rootfiles><rootfile full-path=\"content.opf\" "
            "media-type=\"application/oebps-package+xml\"/></rootfiles></container>");
    zip.add("content.opf",
            "<?xml version=\"1.0\"?><package xmlns=\"http://www.idpf.org/2007/opf\" "
            "version=\"3.0\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "<dc:title>Broken</dc:title></metadata><manifest>"
            "<item id=\"a\" href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "<item id=\"b\" href=\"b.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "</manifest><spine><itemref idref=\"a\"/><itemref idref=\"b\"/></spine>"
            "</package>");
    zip.add("a.xhtml", "<html><body>Alpha</body></html>");
    zip.close();

    const auto session = BookSession::open(path);
    QVERIFY(session.has_value());
    QCOMPARE(session->spine_count(), 2);
    QCOMPARE(session->info().character_count, 5);
    QVERIFY(!session->info().chapter_info.contains("b.xhtml"));
}

void BookSessionTest::a_missing_book_fails() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    std::string error;
    const auto session = BookSession::open(
        std::filesystem::path(directory.path().toStdString()) / "missing.epub", &error);
    QVERIFY(!session.has_value());
    QVERIFY(!error.empty());
}

QTEST_APPLESS_MAIN(BookSessionTest)
#include "tst_book_session.moc"
