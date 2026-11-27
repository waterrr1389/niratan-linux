// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/novels/epub_book.h"

#include <QTemporaryDir>
#include <QTest>

#include <zip.h>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using niratan::novels::EpubBook;
using niratan::novels::EpubGuideReference;
using niratan::novels::EpubMetadata;
using niratan::novels::EpubSpineItem;
using niratan::novels::EpubTocEntry;

// Core/BookStorage.swift and EPUBKit define what the reader needs; the
// fixture is a minimal container built with the same libzip the parser uses.
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

    // libzip writes the archive on zip_close; open() must see the file.
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
        zip_source_t* source =
            zip_source_buffer(zip_, stored.data(), stored.size(), 0);
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
           rootfile +
           "\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>";
}

std::string nav_xhtml() {
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
           "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
           "xmlns:epub=\"http://www.idpf.org/2007/ops\"><body>"
           "<nav epub:type=\"toc\"><ol>"
           "<li><a href=\"text/chapter1.xhtml\">Chapter One</a></li>"
           "<li><a href=\"text/chapter2.xhtml\">One &amp; Two</a>"
           "<ol><li><a href=\"text/chapter2.xhtml#s1\">Section</a></li></ol></li>"
           "</ol></nav></body></html>";
}

std::string epub3_opf() {
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
           "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" "
           "unique-identifier=\"id\">"
           "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
           "<dc:title>Test Book</dc:title><dc:creator>Author Name</dc:creator>"
           "<dc:language>ja</dc:language>"
           "<meta name=\"cover\" content=\"cover-image\"/>"
           "</metadata><manifest>"
           "<item id=\"chapter2\" href=\"text/chapter2.xhtml\" "
           "media-type=\"application/xhtml+xml\"/>"
           "<item id=\"chapter1\" href=\"text/chapter1.xhtml\" "
           "media-type=\"application/xhtml+xml\"/>"
           "<item id=\"nav\" href=\"nav.xhtml\" "
           "media-type=\"application/xhtml+xml\" properties=\"nav\"/>"
           "<item id=\"cover-image\" href=\"images/cover.jpg\" "
           "media-type=\"image/jpeg\" properties=\"cover-image\"/>"
           "</manifest><spine>"
           "<itemref idref=\"chapter1\"/><itemref idref=\"chapter2\"/>"
           "</spine></package>";
}

void write_epub3(ZipWriter& zip) {
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml", container_xml("OEBPS/content.opf"));
    zip.add("OEBPS/content.opf", epub3_opf());
    zip.add("OEBPS/nav.xhtml", nav_xhtml());
    zip.add("OEBPS/text/chapter1.xhtml", "<html><body>Chapter One body</body></html>");
    zip.add("OEBPS/text/chapter2.xhtml", "<html><body>Chapter Two body</body></html>");
    zip.add("OEBPS/images/cover.jpg", "JPEG");
}

std::string epub2_opf() {
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
           "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"2.0\" "
           "unique-identifier=\"id\"><metadata "
           "xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
           "<dc:title>Old Book</dc:title></metadata><manifest>"
           "<item id=\"ncx\" href=\"toc.ncx\" "
           "media-type=\"application/x-dtbncx+xml\"/>"
           "<item id=\"ch\" href=\"ch.xhtml\" "
           "media-type=\"application/xhtml+xml\"/></manifest>"
           "<spine toc=\"ncx\"><itemref idref=\"ch\"/></spine></package>";
}

std::string ncx() {
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
           "<ncx xmlns=\"http://www.daisy.org/z3986/2005/ncx/\" version=\"2005-1\">"
           "<navMap><navPoint id=\"a\"><navLabel><text>Part</text></navLabel>"
           "<content src=\"ch.xhtml\"/>"
           "<navPoint id=\"b\"><navLabel><text>Sub</text></navLabel>"
           "<content src=\"ch.xhtml#x\"/></navPoint></navPoint></navMap></ncx>";
}

std::filesystem::path make_file(const QTemporaryDir& directory, const char* name) {
    return std::filesystem::path(directory.path().toStdString()) / name;
}

}  // namespace

class EpubBookTest : public QObject {
    Q_OBJECT

   private slots:
    void metadata_and_cover_are_read();
    void the_spine_follows_the_itemref_order();
    void spine_items_keep_linear_and_manifest_inputs();
    void guide_references_are_read_raw();
    void entries_can_be_read_by_href();
    void the_nav_document_supplies_leveled_toc_entries();
    void the_ncx_supplies_the_toc_without_a_nav_document();
    void a_missing_container_or_package_fails();
    void hrefs_are_percent_decoded();
};

void EpubBookTest::metadata_and_cover_are_read() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    write_epub3(zip);
    zip.close();

    std::string error;
    auto book = EpubBook::open(make_file(directory, "book.epub"), &error);
    QVERIFY2(book.has_value(), error.c_str());

    const std::optional<std::string> expected_author = std::string("Author Name");
    const std::optional<std::string> expected_cover = std::string("OEBPS/images/cover.jpg");
    QCOMPARE(book->metadata().title, std::string("Test Book"));
    QCOMPARE(book->metadata().author, expected_author);
    QCOMPARE(book->metadata().language, std::optional<std::string>{std::string("ja")});
    QCOMPARE(book->metadata().cover_href, expected_cover);
}

void EpubBookTest::the_spine_follows_the_itemref_order() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    write_epub3(zip);
    zip.close();

    auto book = EpubBook::open(make_file(directory, "book.epub"));
    QVERIFY(book.has_value());
    const std::vector<EpubSpineItem> expected{
        EpubSpineItem{"chapter1", "OEBPS/text/chapter1.xhtml", "application/xhtml+xml",
                      "text/chapter1.xhtml", "", true},
        EpubSpineItem{"chapter2", "OEBPS/text/chapter2.xhtml", "application/xhtml+xml",
                      "text/chapter2.xhtml", "", true},
    };
    QCOMPARE(book->spine(), expected);
    QVERIFY(book->guide().empty());
}

void EpubBookTest::spine_items_keep_linear_and_manifest_inputs() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml", container_xml("OEBPS/pkg/content.opf"));
    zip.add("OEBPS/pkg/content.opf",
            "<?xml version=\"1.0\"?><package xmlns=\"http://www.idpf.org/2007/opf\" "
            "version=\"3.0\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "<dc:title>Linear</dc:title></metadata><manifest>"
            "<item id=\"a\" href=\"text/a.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "<item id=\"b\" href=\"text/b.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "<item id=\"c\" href=\"text/c.xhtml\" media-type=\"application/xhtml+xml\" "
            "properties=\"scripted nav\"/>"
            "<item id=\"d\" href=\"text/d.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "<item id=\"e\" href=\"text/%61.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "</manifest><spine>"
            "<itemref idref=\"a\"/>"
            "<itemref idref=\"b\" linear=\"no\"/>"
            "<itemref idref=\"c\" linear=\"YES\"/>"
            "<itemref idref=\"d\" linear=\"yes\"/>"
            "<itemref idref=\"e\"/>"
            "</spine></package>");
    zip.add("OEBPS/pkg/text/a.xhtml", "<html><body>A</body></html>");
    zip.add("OEBPS/pkg/text/b.xhtml", "<html><body>B</body></html>");
    zip.add("OEBPS/pkg/text/c.xhtml", "<html><body>C</body></html>");
    zip.add("OEBPS/pkg/text/d.xhtml", "<html><body>D</body></html>");
    zip.close();

    auto book = EpubBook::open(make_file(directory, "book.epub"));
    QVERIFY(book.has_value());
    const std::vector<EpubSpineItem> expected{
        EpubSpineItem{"a", "OEBPS/pkg/text/a.xhtml", "application/xhtml+xml", "text/a.xhtml", "",
                      true},
        EpubSpineItem{"b", "OEBPS/pkg/text/b.xhtml", "application/xhtml+xml", "text/b.xhtml", "",
                      false},
        EpubSpineItem{"c", "OEBPS/pkg/text/c.xhtml", "application/xhtml+xml", "text/c.xhtml",
                      "scripted nav", false},
        EpubSpineItem{"d", "OEBPS/pkg/text/d.xhtml", "application/xhtml+xml", "text/d.xhtml", "",
                      true},
        EpubSpineItem{"e", "OEBPS/pkg/text/a.xhtml", "application/xhtml+xml", "text/%61.xhtml", "",
                      true},
    };
    QCOMPARE(book->spine(), expected);
}

void EpubBookTest::guide_references_are_read_raw() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml", container_xml("OEBPS/content.opf"));
    zip.add("OEBPS/content.opf",
            "<?xml version=\"1.0\"?><package xmlns=\"http://www.idpf.org/2007/opf\" "
            "version=\"3.0\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "<dc:title>Guide</dc:title></metadata><manifest>"
            "<item id=\"c\" href=\"text/c.xhtml\" media-type=\"application/xhtml+xml\"/>"
            "</manifest><spine><itemref idref=\"c\"/></spine><guide>"
            "<reference type=\"toc\" href=\"text/c3.xhtml#part\"/>"
            "<reference type=\"other\" href=\"x\"/>"
            "<reference href=\"no-type\"/>"
            "<reference type=\"notoc\"/>"
            "<reference type=\"toc\" href=\"\"/>"
            "</guide></package>");
    zip.add("OEBPS/text/c.xhtml", "<html><body>Guide body</body></html>");
    zip.close();

    auto book = EpubBook::open(make_file(directory, "book.epub"));
    QVERIFY(book.has_value());
    const std::vector<EpubGuideReference> expected{
        EpubGuideReference{"toc", "text/c3.xhtml#part"},
        EpubGuideReference{"other", "x"},
        EpubGuideReference{"toc", ""},
    };
    QCOMPARE(book->guide(), expected);
}

void EpubBookTest::entries_can_be_read_by_href() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    write_epub3(zip);
    zip.close();

    auto book = EpubBook::open(make_file(directory, "book.epub"));
    QVERIFY(book.has_value());

    const auto chapter = book->read("OEBPS/text/chapter1.xhtml");
    QVERIFY(chapter.has_value());
    QVERIFY(chapter->find("Chapter One body") != std::string::npos);

    const auto cover = book->read("OEBPS/images/cover.jpg");
    QVERIFY(cover.has_value());
    QCOMPARE(*cover, std::string("JPEG"));

    QVERIFY(!book->read("OEBPS/missing.xhtml").has_value());
}

void EpubBookTest::the_nav_document_supplies_leveled_toc_entries() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    write_epub3(zip);
    zip.close();

    auto book = EpubBook::open(make_file(directory, "book.epub"));
    QVERIFY(book.has_value());
    const std::vector<EpubTocEntry> expected{
        EpubTocEntry{0, "Chapter One", "OEBPS/text/chapter1.xhtml"},
        EpubTocEntry{0, "One & Two", "OEBPS/text/chapter2.xhtml"},
        EpubTocEntry{1, "Section", "OEBPS/text/chapter2.xhtml#s1"},
    };
    QCOMPARE(book->toc(), expected);
}

void EpubBookTest::the_ncx_supplies_the_toc_without_a_nav_document() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml", container_xml("OEBPS/content.opf"));
    zip.add("OEBPS/content.opf", epub2_opf());
    zip.add("OEBPS/toc.ncx", ncx());
    zip.add("OEBPS/ch.xhtml", "<html><body>Old body</body></html>");
    zip.close();

    auto book = EpubBook::open(make_file(directory, "book.epub"));
    QVERIFY(book.has_value());
    const std::vector<EpubTocEntry> expected{
        EpubTocEntry{0, "Part", "OEBPS/ch.xhtml"},
        EpubTocEntry{1, "Sub", "OEBPS/ch.xhtml#x"},
    };
    QCOMPARE(book->toc(), expected);
}

void EpubBookTest::a_missing_container_or_package_fails() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "bad.epub"));
    QVERIFY(zip.valid());
    zip.add("mimetype", "application/epub+zip");
    zip.close();

    std::string error;
    const auto book = EpubBook::open(make_file(directory, "bad.epub"), &error);
    QVERIFY(!book.has_value());
    QVERIFY(!error.empty());
}

void EpubBookTest::hrefs_are_percent_decoded() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ZipWriter zip(make_file(directory, "book.epub"));
    QVERIFY(zip.valid());
    zip.add("mimetype", "application/epub+zip");
    zip.add("META-INF/container.xml", container_xml("content.opf"));
    zip.add("content.opf",
            "<?xml version=\"1.0\"?><package xmlns=\"http://www.idpf.org/2007/opf\" "
            "version=\"3.0\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
            "<dc:title>Space</dc:title></metadata><manifest>"
            "<item id=\"c\" href=\"text/chapter%20one.xhtml\" "
            "media-type=\"application/xhtml+xml\"/></manifest>"
            "<spine><itemref idref=\"c\"/></spine></package>");
    zip.add("text/chapter one.xhtml", "<html><body>Spaced</body></html>");
    zip.close();

    auto book = EpubBook::open(make_file(directory, "book.epub"));
    QVERIFY(book.has_value());
    QCOMPARE(book->spine().size(), 1u);
    QCOMPARE(book->spine().front().href, std::string("text/chapter one.xhtml"));
    const auto content = book->read(book->spine().front().href);
    QVERIFY(content.has_value());
    QVERIFY(content->find("Spaced") != std::string::npos);
}

QTEST_APPLESS_MAIN(EpubBookTest)
#include "tst_epub_book.moc"
