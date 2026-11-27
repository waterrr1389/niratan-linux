// SPDX-License-Identifier: GPL-3.0-or-later
// TtuConverter.swift: both conversions, archive entries and Swift XML templates.

#include "fakes/ttu_converter_fixture.h"
#include "services/novels/epub_book.h"
#include "services/sync/ttu_convert.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

class TtuConvertTest : public QObject {
    Q_OBJECT

   private slots:
    void exports_staticdata_and_stored_assets();
    void export_guards_data();
    void export_guards();
    void imports_exact_epub_templates_and_sidecars();
    void rejects_invalid_bookdata_data();
    void rejects_invalid_bookdata();
    void existing_destination_is_left_alone_data();
    void existing_destination_is_left_alone();
    void exports_then_imports_a_readable_book();
    void imports_a_label_less_toc_without_blobs_or_cover();
};

#ifndef Q_MOC_RUN
using namespace niratan;
using namespace ttu_converter_fixture;

namespace {

constexpr std::int64_t now_ms = 1690000000123;
constexpr auto opf_id = "12345678-0000-0000-0000-000000000001";
constexpr auto book_id = "12345678-0000-0000-0000-000000000002";

struct Library {
    QTemporaryDir root;
    books::BookStorage storage{root.path().toStdString()};
    books::BookMetadata book;
    books::BookInfo info;
    int clock_reads = 0;
    int uuids = 0;

    Library() {
        book.title = "Export title";
        book.renamed_title = "Display title";
        book.folder = "Export title";
        book.id = "local";
        book.epub = "source.epub";
        book.cover = "Books/Export title/original.JPG";
        book.last_access = 1690000000.45675 - 978307200;
        info.character_count = 4;
        info.chapter_info = {{"OEBPS/Text/one.xhtml", {0, 0, 3, {}}},
                             {"OEBPS/Text/two.xhtml", {1, 3, 0, {}}},
                             {"OEBPS/Text/three.xhtml", {2, 3, 1, {}}}};
    }

    std::filesystem::path folder() const { return storage.books_directory() / book.folder; }

    bool seed(QStringView fault = {}) {
        if (!root.isValid() || !std::filesystem::create_directories(folder())) {
            return false;
        }
        if (fault == u"no-epub-name") {
            book.epub.reset();
        } else if (fault == u"bad-epub") {
            QFile file(QString::fromStdString((folder() / *book.epub).string()));
            if (!file.open(QIODevice::WriteOnly) || file.write("Not an EPUB") < 0) {
                return false;
            }
        } else if (!make_epub(folder() / *book.epub, fault != u"missing-chapter",
                              fault != u"missing-css")) {
            return false;
        }
        if (fault == u"no-cover") {
            book.cover.reset();
        } else if (fault != u"missing-cover") {
            QFile cover(QString::fromStdString((folder() / "original.JPG").string()));
            if (!cover.open(QIODevice::WriteOnly) || cover.write("ROOT COVER") != 10) {
                return false;
            }
        }
        return (fault == u"no-metadata" || storage.save_metadata(book)) &&
            (fault == u"no-bookinfo" || storage.save_book_info(book.folder, info));
    }

    sync::TtuConverter converter() {
        return sync::TtuConverter(
            storage,
            {
                .now_unix_ms =
                    [this] {
                        ++clock_reads;
                        return now_ms;
                    },
                .new_uuid = [this] { return std::string(++uuids == 1 ? opf_id : book_id); },
            });
    }
};

QByteArray bytes_at(const std::vector<Entry>& entries, std::string_view name) {
    const auto entry = std::ranges::find(entries, name, &Entry::name);
    return entry == entries.end() ? QByteArray{} : entry->bytes;
}

constexpr auto container_xml = R"(<?xml version="1.0"?>
<container
 version="1.0"
 xmlns="urn:oasis:names:tc:opendocument:xmlns:container"
>
<rootfiles>
<rootfile
 full-path="item/standard.opf"
 media-type="application/oebps-package+xml"
/>
</rootfiles>
</container>)";

constexpr auto first_chapter = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html
 xmlns="http://www.w3.org/1999/xhtml"
 xmlns:epub="http://www.idpf.org/2007/ops"
 xml:lang="ja"
 class="top"
>
<head>
<meta charset="UTF-8"/>
<title>A &amp; B: 猫</title>
<link rel="stylesheet" type="text/css" href="../stylesheet.css"/>
</head>
<body class="main">
<p>猫&#160;犬<br/><hr/><img src="../Images/pic.png"/><img src="../Images/cover.JPG"/><img src="../Images/self.png"//></p><svg xmlns="http://www.w3.org/2000/svg"/>
</body>
</html>)";

constexpr auto second_chapter = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html
 xmlns="http://www.w3.org/1999/xhtml"
 xmlns:epub="http://www.idpf.org/2007/ops"
 xml:lang="ja"
 class=""
>
<head>
<meta charset="UTF-8"/>
<title>A &amp; B: 猫</title>
<link rel="stylesheet" type="text/css" href="../stylesheet.css"/>
</head>
<body class="">
<p>鳥</p>
</body>
</html>)";

constexpr auto nav_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html
 xmlns="http://www.w3.org/1999/xhtml"
 xmlns:epub="http://www.idpf.org/2007/ops"
 xml:lang="ja"
>
<head>
<meta charset="UTF-8"/>
<title>Navigation</title>
</head>
<body>
<nav epub:type="toc" id="toc">
<h1>Navigation</h1>
<ol>
<li><a href="xhtml/ztoc.xhtml">目次 &amp; 本</a></li>
</ol>
</nav>

<nav epub:type="landmarks" id="guide">
<h1>Guide</h1>
<ol>
<li><a epub:type="toc" href="xhtml/ztoc.xhtml">目次 &amp; 本</a></li>
</ol>
</nav>

</body>
</html>)";

constexpr auto opf_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<package
 xmlns="http://www.idpf.org/2007/opf"
 version="3.0"
 xml:lang="ja"
 unique-identifier="book-uuid"
>

<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">

<!-- 作品名 -->
<dc:title id="title">A &amp; B: 猫</dc:title>

<!-- 言語 -->
<dc:language>ja</dc:language>

<!-- ファイルid -->
<dc:identifier id="book-uuid">12345678-0000-0000-0000-000000000001</dc:identifier>

</metadata>

<manifest>

<!-- navigation -->
<item media-type="application/xhtml+xml" id="nav" href="navigation-documents.xhtml" properties="nav"/>

<!-- style -->
<item media-type="text/css" id="stylesheet" href="stylesheet.css"/>

<!-- image -->
<item media-type="image/gif" id="i-a" href="nested/a.GIF"/>
<item media-type="image/jpeg" id="cover" href="Images/cover.JPG" properties="cover-image"/>
<item media-type="image/png" id="i-pic" href="Images/pic.png"/>
<item media-type="image/svg+xml" id="i-z" href="z.svg"/>

<!-- xhtml -->
<item media-type="application/xhtml+xml" id="a" href="xhtml/a.xhtml"/>
<item media-type="application/xhtml+xml" id="ztoc" href="xhtml/ztoc.xhtml" properties="svg"/>

</manifest>

<spine page-progression-direction="rtl">

<itemref linear="yes" idref="ztoc"/>
<itemref linear="yes" idref="a"/>

</spine>

</package>)";

}  // namespace
void TtuConvertTest::exports_staticdata_and_stored_assets() {
    Library library;
    QVERIFY(library.seed());
    QTemporaryDir out;
    QVERIFY(out.isValid());
    const auto output_dir = std::filesystem::path(out.path().toStdString());
    const auto actual = library.converter().export_zip(library.folder(), output_dir);
    QVERIFY(actual);
    QCOMPARE(*actual, output_dir / "bookdata_1_6_4_1690000000123_1690000000456.zip");
    QCOMPARE(library.clock_reads, 1);
    const auto entries = read_zip(*actual);
    QVERIFY(entries);
    QCOMPARE(entries->size(), 3U);
    QCOMPARE((*entries)[0].name, "staticdata.json");
    QCOMPARE((*entries)[0].compression, ZIP_CM_DEFLATE);
    QCOMPARE((*entries)[1].name, "blobs/Images/pic.png");
    QCOMPARE((*entries)[1].compression, ZIP_CM_STORE);
    QCOMPARE((*entries)[1].bytes, "PNG");
    QCOMPARE((*entries)[2].name, "cover.JPG");
    QCOMPARE((*entries)[2].compression, ZIP_CM_STORE);
    QCOMPARE((*entries)[2].bytes, "ROOT COVER");
    const auto data = QJsonDocument::fromJson(entries->front().bytes).object();
    QCOMPARE(data.size(), 4);
    QCOMPARE(data.value("title").toString(), QStringLiteral("Export title"));
    QCOMPARE(data.value("styleSheet").toString(), QStringLiteral("p{color:red}body{margin:0}"));
    const QString image =
        QStringLiteral("data:image/gif;ttu:Images/"
                       "pic.png;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==");
    const auto expected =
        QStringLiteral(
            "<div id=\"ttu-one\"><div class=\"ttu-book-html-wrapper top\"><div "
            "class=\"ttu-book-body-wrapper main\"><p id=\"start\">猫&nbsp;犬<br><hr ><img "
            "src=\"%1\"><image xlink:href=\"%1\"></image><image "
            "href=\"%1\"></image></p><p>a<br>b<hr>c<img src=\"%1\">d</p></div></div></div>"
            "<div id=\"ttu-two\"><div class=\"ttu-book-html-wrapper ttu-no-text\"><div "
            "class=\"ttu-book-body-wrapper ttu-no-text\"><p id=\"here\"></p></div></div></div>"
            "<div id=\"ttu-three\"><div class=\"ttu-book-html-wrapper\"><div "
            "class=\"ttu-book-body-wrapper\"><p>鳥</p></div></div></div>")
            .arg(image);
    QCOMPARE(data.value("elementHtml").toString(), expected);
    const auto sections = QJsonDocument::fromJson(R"([
          {"reference":"ttu-one","charactersWeight":3,"label":"Preface","startCharacter":0,"characters":3},
          {"reference":"ttu-two","charactersWeight":1,"label":"Part & Two","startCharacter":3,"characters":1},
          {"reference":"ttu-three","charactersWeight":1,"parentChapter":"ttu-two"}])")
                              .array();
    QCOMPARE(data.value("sections").toArray(), sections);
}

void TtuConvertTest::export_guards_data() {
    QTest::addColumn<QString>("fault");
    for (const auto* fault : {"no-metadata", "no-bookinfo", "no-epub-name", "bad-epub",
                              "missing-chapter", "missing-css", "missing-cover", "output-file"}) {
        QTest::newRow(fault) << QString::fromLatin1(fault);
    }
}

void TtuConvertTest::export_guards() {
    QFETCH(QString, fault);
    Library library;
    QVERIFY(library.seed(fault));
    QTemporaryDir out;
    QVERIFY(out.isValid());
    auto path = std::filesystem::path(out.path().toStdString());
    if (fault == "output-file") {
        path /= "file";
        QFile file(QString::fromStdString(path.string()));
        QVERIFY(file.open(QIODevice::WriteOnly));
    }
    QVERIFY(!library.converter().export_zip(library.folder(), path));
    QVERIFY(fault == "output-file" ? std::filesystem::is_regular_file(path)
                                   : std::filesystem::is_empty(path));
    Library valid;
    QVERIFY(valid.seed());
    QVERIFY(valid.converter().export_zip(valid.folder(), out.path().toStdString()));
}

void TtuConvertTest::imports_exact_epub_templates_and_sidecars() {
    Library library;
    QVERIFY(library.root.isValid());
    const auto source = library.root.filePath("input.zip").toStdString();
    QVERIFY(make_ttu(source));
    const auto dest = library.converter().import_zip(source, library.storage.books_directory());
    QVERIFY(dest);
    QCOMPARE(*dest, library.storage.books_directory() / "A & B_ 猫");
    const auto epub = *dest / "A & B_ 猫.epub";
    const auto entries = read_zip(epub);
    QVERIFY(entries);
    const std::vector<std::string> names{"mimetype",
                                         "item/stylesheet.css",
                                         "META-INF/container.xml",
                                         "item/nested/a.GIF",
                                         "item/Images/cover.JPG",
                                         "item/Images/pic.png",
                                         "item/z.svg",
                                         "item/xhtml/ztoc.xhtml",
                                         "item/xhtml/a.xhtml",
                                         "item/navigation-documents.xhtml",
                                         "item/standard.opf"};
    std::vector<std::string> actual;
    for (const auto& entry : *entries) {
        actual.push_back(entry.name);
        QCOMPARE(entry.compression, entry.name == "mimetype" ? ZIP_CM_STORE : ZIP_CM_DEFLATE);
    }
    QCOMPARE(actual, names);
    QCOMPARE(entries->front().bytes, "application/epub+zip");
    QCOMPARE(bytes_at(*entries, "item/stylesheet.css"), "p{color:red}");
    QCOMPARE(bytes_at(*entries, "META-INF/container.xml"), container_xml);
    QCOMPARE(bytes_at(*entries, "item/xhtml/ztoc.xhtml"), first_chapter);
    QCOMPARE(bytes_at(*entries, "item/xhtml/a.xhtml"), second_chapter);
    QCOMPARE(bytes_at(*entries, "item/navigation-documents.xhtml"), nav_xml);
    QCOMPARE(bytes_at(*entries, "item/standard.opf"), opf_xml);
    QCOMPARE(bytes_at(*entries, "item/Images/pic.png"), "PNG");
    QFile cover(QString::fromStdString((*dest / "cover.jpg").string()));
    QVERIFY(cover.open(QIODevice::ReadOnly));
    QCOMPARE(cover.readAll(), "ROOT COVER");
    const auto metadata = library.storage.load_metadata("A & B_ 猫");
    QVERIFY(metadata);
    books::BookMetadata expected;
    expected.id = book_id;
    expected.title = "A & B: 猫";
    expected.folder = "A & B_ 猫";
    expected.epub = "A & B_ 猫.epub";
    expected.cover = "Books/A & B_ 猫/cover.jpg";
    expected.last_access = now_ms / 1000.0 - 978307200;
    QCOMPARE(*metadata, expected);
    QCOMPARE(library.uuids, 2);
    const auto info = library.storage.load_book_info(expected.folder);
    QVERIFY(info);
    QCOMPARE(info->chapter_info.size(), 2U);
    const auto& first = info->chapter_info.at("item/xhtml/ztoc.xhtml");
    const auto& second = info->chapter_info.at("item/xhtml/a.xhtml");
    QCOMPARE(first.spine_index, std::optional{0});
    QCOMPARE(first.current_total, 0);
    QCOMPARE(second.spine_index, std::optional{1});
    QCOMPARE(second.current_total, first.chapter_count);
    QCOMPARE(info->character_count, first.chapter_count + second.chapter_count);
    QVERIFY(info->character_count > 0);
}

void TtuConvertTest::rejects_invalid_bookdata_data() {
    QTest::addColumn<QByteArray>("data");
    QTest::newRow("bad-json") << QByteArray("{");
    QTest::newRow("missing-title")
        << QByteArray(R"({"styleSheet":"","elementHtml":"","sections":[]})");
    QTest::newRow("bad-stylesheet")
        << QByteArray(R"({"title":"T","styleSheet":12,"elementHtml":"","sections":[]})");
    QTest::newRow("missing-html") << QByteArray(R"({"title":"T","styleSheet":"","sections":[]})");
    QTest::newRow("bad-sections") << QByteArray(
        R"({"title":"T","styleSheet":"","elementHtml":"","sections":{}})");
    QTest::newRow("missing-reference") << QByteArray(
        R"({"title":"T","styleSheet":"","elementHtml":"","sections":[{"charactersWeight":1}]})");
    QTest::newRow("fractional-weight") << QByteArray(
        R"({"title":"T","styleSheet":"","elementHtml":"","sections":[{"reference":"ttu-a","charactersWeight":0.5}]})");
    QTest::newRow("bad-optional") << QByteArray(
        R"({"title":"T","styleSheet":"","elementHtml":"","sections":[{"reference":"ttu-a","charactersWeight":1,"label":1}]})");
    QTest::newRow("empty-title") << QByteArray(
        R"({"title":"   ","styleSheet":"","elementHtml":"","sections":[]})");
    QTest::newRow("missing-entry") << QByteArray();
    QTest::newRow("not-an-archive") << QByteArray("NOT ZIP");
}

void TtuConvertTest::rejects_invalid_bookdata() {
    QFETCH(QByteArray, data);
    Library library;
    QVERIFY(library.root.isValid());
    const auto source = library.root.filePath("input.zip").toStdString();
    if (data == "NOT ZIP") {
        QFile file(QString::fromStdString(source));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
    } else {
        QVERIFY(write_zip(source, {{data.isNull() ? "other.json" : "staticdata.json", data}}));
    }
    const auto converter = library.converter();
    QVERIFY(!converter.import_zip(source, library.storage.books_directory()));
    // A valid input must succeed with the same storage and converter.
    const auto valid = library.root.filePath("valid.zip").toStdString();
    QVERIFY(make_ttu(valid));
    QVERIFY(converter.import_zip(valid, library.storage.books_directory()));
}

void TtuConvertTest::existing_destination_is_left_alone_data() {
    QTest::addColumn<bool>("file");
    QTest::newRow("directory") << false;
    QTest::newRow("file") << true;
}

void TtuConvertTest::existing_destination_is_left_alone() {
    QFETCH(bool, file);
    Library library;
    QVERIFY(library.root.isValid());
    const auto source = library.root.filePath("input.zip").toStdString();
    QVERIFY(make_ttu(source));
    const auto dest = library.storage.books_directory() / "A & B_ 猫";
    QVERIFY(std::filesystem::create_directories(file ? dest.parent_path() : dest));
    const auto marker = file ? dest : dest / "keep";
    QFile sentinel(QString::fromStdString(marker.string()));
    QVERIFY(sentinel.open(QIODevice::WriteOnly));
    QCOMPARE(sentinel.write("keep"), 4);
    sentinel.close();
    QCOMPARE(library.converter().import_zip(source, library.storage.books_directory()),
             std::optional{dest});
    QVERIFY(sentinel.open(QIODevice::ReadOnly));
    QCOMPARE(sentinel.readAll(), "keep");
    if (!file) {
        QVERIFY(!std::filesystem::exists(dest / "metadata.json"));
    }
    QCOMPARE(library.uuids, 0);
}

void TtuConvertTest::exports_then_imports_a_readable_book() {
    Library source;
    QVERIFY(source.seed());
    Library target;
    QVERIFY(target.root.isValid());
    const auto zip =
        source.converter().export_zip(source.folder(), target.root.path().toStdString());
    QVERIFY(zip);
    const auto book = target.converter().import_zip(*zip, target.storage.books_directory());
    QVERIFY(book);
    QCOMPARE(*book, target.storage.books_directory() / "Export title");
    const auto metadata = target.storage.load_metadata("Export title");
    QVERIFY(metadata);
    QCOMPARE(metadata->title, "Export title");
    QVERIFY(metadata->epub);
    const auto epub = novels::EpubBook::open(*book / *metadata->epub);
    QVERIFY(epub);
    QCOMPARE(epub->spine().size(), 3U);
    QVERIFY(epub->read("item/Images/pic.png"));
    const auto info = target.storage.load_book_info("Export title");
    QVERIFY(info);
    QCOMPARE(info->chapter_info.size(), 3U);
    QVERIFY(std::filesystem::is_regular_file(*book / "cover.JPG"));
}
void TtuConvertTest::imports_a_label_less_toc_without_blobs_or_cover() {
    Library library;
    QVERIFY(library.root.isValid());
    const auto source = library.root.filePath("minimal.zip").toStdString();
    const QByteArray data =
        R"({"title":"Minimal","styleSheet":"","elementHtml":"<div id=\"ttu-toc\"><p>x</p></div>","sections":[{"reference":"ttu-toc","charactersWeight":1}]})";
    QVERIFY(write_zip(source, {{"staticdata.json", data}}));
    const auto dest = library.converter().import_zip(source, library.storage.books_directory());
    QVERIFY(dest);
    QCOMPARE(*dest, library.storage.books_directory() / "Minimal");
    const auto entries = read_zip(*dest / "Minimal.epub");
    QVERIFY(entries);
    QVERIFY(!bytes_at(*entries, "item/standard.opf").contains("media-type=\"image/"));
    const auto nav = bytes_at(*entries, "item/navigation-documents.xhtml");
    QVERIFY(nav.contains("<li><a epub:type=\"toc\" href=\"xhtml/toc.xhtml\">toc</a></li>"));
    const auto metadata = library.storage.load_metadata("Minimal");
    QVERIFY(metadata);
    QVERIFY(!metadata->cover);
    QVERIFY(!std::filesystem::exists(*dest / "cover.jpg"));
    QVERIFY(novels::EpubBook::open(*dest / "Minimal.epub"));
}
#endif

QTEST_APPLESS_MAIN(TtuConvertTest)
#include "tst_ttu_convert.moc"
