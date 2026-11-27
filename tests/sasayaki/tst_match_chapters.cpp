// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Sasayaki/SasayakiMatcher.swift (SasayakiMatcher.match(rootURL:):
// guideTocPaths and the spine loop) and EPUBKit's spine, manifest and guide
// parsers are the specification. The EPUBs are built with libzip like
// tests/novels/tst_epub_book.cpp, and every fixture name follows the task
// file's description so a failure names the skip it exercises.

#include <QTemporaryDir>
#include <QTest>

#include <zip.h>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "services/novels/epub_book.h"
#include "services/sasayaki/match_chapters.h"

using niratan::novels::EpubBook;
using niratan::sasayaki::match_chapters;
using niratan::sasayaki::MatchChapter;

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

    void close() {
        if (zip_ != nullptr) {
            zip_close(zip_);
            zip_ = nullptr;
        }
    }

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

   private:
    zip_t* zip_ = nullptr;
    std::vector<std::string> storage_;
};

std::string container_xml(const std::string& rootfile) {
    return "<?xml version=\"1.0\"?>"
           "<container version=\"1.0\" "
           "xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
           "<rootfiles><rootfile full-path=\"" +
        rootfile + "\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>";
}

std::string package_head() {
    return "<?xml version=\"1.0\"?><package xmlns=\"http://www.idpf.org/2007/opf\" "
           "version=\"3.0\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
           "<dc:title>Match Book</dc:title></metadata><manifest>";
}

std::string main_package() {
    return package_head() +
        "<item id=\"ch0\" href=\"text/ch0.xhtml\" "
        "media-type=\"application/xhtml+xml\"/>"
        "<item id=\"linear-no\" href=\"text/linear-no.xhtml\" "
        "media-type=\"application/xhtml+xml\"/>"
        "<item id=\"nav\" href=\"nav.xhtml\" "
        "media-type=\"application/xhtml+xml\" properties=\"scripted nav\"/>"
        "<item id=\"guide\" href=\"text/guide.xhtml\" "
        "media-type=\"application/xhtml+xml\"/>"
        "<item id=\"missing\" href=\"text/missing.xhtml\" "
        "media-type=\"application/xhtml+xml\"/>"
        "<item id=\"ch5\" href=\"text/ch5.xhtml\" "
        "media-type=\"application/xhtml+xml\"/>"
        "</manifest><spine>"
        "<itemref idref=\"ch0\"/>"
        "<itemref idref=\"linear-no\" linear=\"no\"/>"
        "<itemref idref=\"nav\"/>"
        "<itemref idref=\"guide\"/>"
        "<itemref idref=\"missing\"/>"
        "<itemref idref=\"ch5\"/>"
        "</spine><guide>"
        "<reference type=\"toc\" href=\"text/guide.xhtml#start\"/>"
        "<reference type=\"text\" href=\"text/ch5.xhtml\"/>"
        "</guide></package>";
}

std::string nav_xhtml() {
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
           "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
           "xmlns:epub=\"http://www.idpf.org/2007/ops\"><body>"
           "<nav epub:type=\"toc\"><ol></ol></nav></body></html>";
}

std::filesystem::path make_file(const QTemporaryDir& directory, const char* name) {
    return std::filesystem::path(directory.path().toStdString()) / name;
}

std::optional<EpubBook> open_book(const QTemporaryDir& directory) {
    return EpubBook::open(make_file(directory, "book.epub"));
}

void write_main_book(ZipWriter& zip) {
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml", container_xml("OEBPS/book.opf"));
    zip.add("OEBPS/book.opf", main_package());
    zip.add("OEBPS/nav.xhtml", nav_xhtml());
    zip.add("OEBPS/text/ch0.xhtml", "<html><body><p>あいう</p><script>えお</script></body></html>");
    zip.add("OEBPS/text/linear-no.xhtml", "<html><body><p>りく</p></body></html>");
    zip.add("OEBPS/text/guide.xhtml", "<html><body><p>ぐ</p></body></html>");
    zip.add("OEBPS/text/ch5.xhtml", "<html><body><p>さしす</p></body></html>");
}

}  // namespace

class MatchChaptersTest : public QObject {
    Q_OBJECT

   private slots:
    void main_book_selects_matching_chapters();
    void guide_toc_types_are_case_insensitive_but_hrefs_are_raw();
    void navigation_property_skips_a_chapter();
    void empty_spine_yields_nothing();
};

// B4, B5, B6, B7: linear, the nav substring, the guide fragment, a missing
// entry and preserved spine indices in one book.
void MatchChaptersTest::main_book_selects_matching_chapters() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    write_main_book(zip);
    zip.close();

    const std::optional<EpubBook> book = open_book(directory);
    QVERIFY(book.has_value());
    const std::vector<MatchChapter> chapters = match_chapters(*book);
    QCOMPARE(static_cast<int>(chapters.size()), 2);
    if (chapters.size() != 2) {
        return;
    }
    QCOMPARE(chapters[0].index, 0);
    QCOMPARE(QString::fromStdString(chapters[0].text), QStringLiteral("あいう"));
    QCOMPARE(chapters[1].index, 5);
    QCOMPARE(QString::fromStdString(chapters[1].text), QStringLiteral("さしす"));
}

// B5: type is lowercased, the href is cut at '#', a leading '#' yields the
// text after it (Swift drops the empty part), and "./" is not stripped.
void MatchChaptersTest::guide_toc_types_are_case_insensitive_but_hrefs_are_raw() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml", container_xml("OEBPS/book.opf"));
    zip.add("OEBPS/book.opf",
            package_head() +
                "<item id=\"g1\" href=\"text/g1.xhtml\" "
                "media-type=\"application/xhtml+xml\"/>"
                "<item id=\"g2\" href=\"text/g2.xhtml\" "
                "media-type=\"application/xhtml+xml\"/>"
                "<item id=\"g3\" href=\"start\" "
                "media-type=\"application/xhtml+xml\"/>"
                "</manifest><spine><itemref idref=\"g1\"/><itemref idref=\"g2\"/>"
                "<itemref idref=\"g3\"/></spine>"
                "<guide>"
                "<reference type=\"TOC\" href=\"text/g1.xhtml#a\"/>"
                "<reference type=\"toc\" href=\"./text/g2.xhtml#b\"/>"
                "<reference type=\"toc\" href=\"#start\"/>"
                "</guide></package>");
    zip.add("OEBPS/text/g1.xhtml", "<html><body><p>いち</p></body></html>");
    zip.add("OEBPS/text/g2.xhtml", "<html><body><p>に</p></body></html>");
    zip.add("OEBPS/start", "<html><body><p>さん</p></body></html>");
    zip.close();

    const std::optional<EpubBook> book = open_book(directory);
    QVERIFY(book.has_value());
    const std::vector<MatchChapter> chapters = match_chapters(*book);
    QCOMPARE(static_cast<int>(chapters.size()), 1);
    if (chapters.empty()) {
        return;
    }
    QCOMPARE(chapters[0].index, 1);
    QCOMPARE(QString::fromStdString(chapters[0].text), QStringLiteral("に"));
}

// B4: properties are a substring test, so "navigation" also skips.
void MatchChaptersTest::navigation_property_skips_a_chapter() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml", container_xml("OEBPS/book.opf"));
    zip.add("OEBPS/book.opf",
            package_head() +
                "<item id=\"n\" href=\"text/n.xhtml\" "
                "media-type=\"application/xhtml+xml\" properties=\"navigation\"/>"
                "</manifest><spine><itemref idref=\"n\"/></spine></package>");
    zip.add("OEBPS/text/n.xhtml", "<html><body><p>な</p></body></html>");
    zip.close();

    const std::optional<EpubBook> book = open_book(directory);
    QVERIFY(book.has_value());
    QVERIFY(match_chapters(*book).empty());
}

// B7: an empty spine selects nothing.
void MatchChaptersTest::empty_spine_yields_nothing() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml", container_xml("OEBPS/book.opf"));
    zip.add("OEBPS/book.opf",
            package_head() +
                "<item id=\"unused\" href=\"text/unused.xhtml\" "
                "media-type=\"application/xhtml+xml\"/></manifest><spine></spine></package>");
    zip.close();

    const std::optional<EpubBook> book = open_book(directory);
    QVERIFY(book.has_value());
    QVERIFY(match_chapters(*book).empty());
}

QTEST_APPLESS_MAIN(MatchChaptersTest)
#include "tst_match_chapters.moc"
