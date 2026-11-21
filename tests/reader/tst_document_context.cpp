// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/document_context.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using niratan::books::BookMetadata;
using niratan::books::BookStorage;
using niratan::reader::document_cover_path;
using niratan::reader::document_cover_url;
using niratan::reader::document_title;
using niratan::reader::read_document_cover;

class DocumentContextTest : public QObject {
    Q_OBJECT

   private slots:
    void epub_title_takes_precedence();
    void missing_or_empty_epub_title_uses_library_display_title();
    void relative_absolute_and_absent_covers();
    void cover_url_preserves_the_path_extension();
    void cover_read_returns_bytes_only_for_a_readable_file();
};

// B9: NativeReaderModel.title and EPUBDocument.title, before BookMetadata.displayTitle.
void DocumentContextTest::epub_title_takes_precedence() {
    BookMetadata book;
    book.title = "Original library title";
    book.renamed_title = "Renamed library title";
    QCOMPARE(document_title("本文の題名", book), std::string("本文の題名"));
}

// B9: NativeReaderModel.title and BookMetadata.displayTitle, with the Linux
// fallback for an explicitly empty dc:title as well as an absent one.
void DocumentContextTest::missing_or_empty_epub_title_uses_library_display_title() {
    BookMetadata book;
    book.title = "Original library title";
    book.renamed_title = "Renamed library title";
    QCOMPARE(document_title({}, book), std::string("Renamed library title"));
    book.renamed_title.reset();
    QCOMPARE(document_title({}, book), std::string("Original library title"));
}

// B10: BookMetadata.coverURL and NativeReaderModel.coverURL.
void DocumentContextTest::relative_absolute_and_absent_covers() {
    const std::filesystem::path app_directory("/app/data");
    const BookStorage storage(app_directory);
    BookMetadata book;
    QVERIFY(!document_cover_path(storage, book).has_value());

    book.cover = "Books/Title/表紙.png";
    const auto relative = document_cover_path(storage, book);
    QVERIFY(relative.has_value());
    QCOMPARE(relative->string(), (app_directory / "Books/Title/表紙.png").string());

    book.cover = "/other/library/表紙.jpg";
    const auto absolute = document_cover_path(storage, book);
    QVERIFY(absolute.has_value());
    QCOMPARE(absolute->string(), std::string("/other/library/表紙.jpg"));
}

// B11: AnkiManager.handlebarToValue (.bookCover) and LocalFileServer.port.
void DocumentContextTest::cover_url_preserves_the_path_extension() {
    QVERIFY(document_cover_url({}).empty());
    QCOMPARE(document_cover_url("/library/cover.scan.JPEG"),
             std::string("http://localhost:18765/cover/cover.JPEG"));
    QCOMPARE(document_cover_url("/library/cover"),
             std::string("http://localhost:18765/cover/cover."));
}

// B10, B11: LocalFileServer.setCover(file:) and AnkiManager.addNoteAnkiConnect.
void DocumentContextTest::cover_read_returns_bytes_only_for_a_readable_file() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString file_name = directory.filePath("表紙.png");
    const QByteArray expected("PNG\0image", 9);
    QFile file(file_name);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(expected), expected.size());
    file.close();

    const auto bytes = read_document_cover(file_name.toStdString());
    QVERIFY(bytes.has_value());
    QCOMPARE(*bytes, expected);
    QVERIFY(!read_document_cover(directory.filePath("missing.png").toStdString()).has_value());
}

QTEST_APPLESS_MAIN(DocumentContextTest)
#include "tst_document_context.moc"
