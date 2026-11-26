// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's imported-font stylesheet and the reader://font resource,
// following Core/FontManager.swift (fontfaceCSS, cssFontName, resourceName) and
// the reader scheme NativeReaderView loads. The model inserts the block at the
// start of the style block of every chapter document and answers the font host
// from <app>/Fonts, so the test writes one font there and opens a fixture book.

#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"
#include "viewmodels/reader/reader_view_model.h"

#include <QDir>
#include <QFile>
#include <QtTest>

#include <filesystem>
#include <string>

#include "data_root.h"
#include "epub_fixture.h"

using niratan::test::TempDataRoot;
using niratan::viewmodels::ReaderViewModel;

namespace {

const QString kFolder = QStringLiteral("Fonts Book");
const QByteArray kFontBytes = QByteArrayLiteral("imported-font-bytes");

bool make_book() {
    const niratan::books::BookStorage storage(niratan::storage::data_root());
    niratan::books::BookMetadata book;
    book.id = "Fonts";
    book.folder = kFolder.toStdString();
    book.title = "Fonts";
    book.epub = "book.epub";
    const auto directory = storage.book_directory(book.folder);
    if (!directory || !QDir().mkpath(QString::fromStdString(directory->string()))) {
        return false;
    }
    return niratan::test::write_epub(*directory / "book.epub", "Fonts", "あいうえお",
                                     "かきくけこ") &&
        storage.save_metadata(book);
}

bool write_font() {
    const auto directory = niratan::storage::data_root() / "Fonts";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    QFile file(QString::fromStdString((directory / "Imported.ttf").string()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(kFontBytes) == kFontBytes.size();
}

}  // namespace

class ReaderFontsTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void cleanup();

    void font_face_block_and_resource_reach_the_reader();

   private:
    TempDataRoot data_;
};

void ReaderFontsTest::initTestCase() {
    QVERIFY(data_.isValid());
    QVERIFY(make_book());
}

void ReaderFontsTest::cleanup() {
    if (auto* reader = ReaderViewModel::instance(0); reader->has_book()) {
        reader->closeBook();
    }
    std::error_code error;
    std::filesystem::remove(niratan::storage::data_root() / "Fonts/Imported.ttf", error);
}

// B4 and B5: one @font-face per stored font in the chapter's style block, and
// the same file served by reader://font with a font MIME type.
void ReaderFontsTest::font_face_block_and_resource_reach_the_reader() {
    auto* reader = ReaderViewModel::instance(0);
    QVERIFY(reader);
    QVERIFY(write_font());
    const QString css = QStringLiteral("@font-face {\n"
                                       "    font-family: 'Imported';\n"
                                       "    src: url('reader://font/Imported.ttf');\n"
                                       "}");
    QCOMPARE(reader->font_face_css(), css);
    const auto resource = reader->font_resource("Imported.ttf");
    QVERIFY(resource.has_value());
    QCOMPARE(QByteArray::fromStdString(resource->bytes), kFontBytes);
    QCOMPARE(QString::fromStdString(resource->mime_type), QStringLiteral("font/ttf"));
    QVERIFY(!reader->font_resource("../Imported.ttf").has_value());
    QVERIFY(!reader->font_resource("Missing.ttf").has_value());

    QVERIFY(reader->openBook(kFolder));
    const std::string document = reader->document_for("first.xhtml");
    QVERIFY(!document.empty());
    QVERIFY(document.find(css.toStdString()) != std::string::npos);

    std::error_code error;
    std::filesystem::remove(niratan::storage::data_root() / "Fonts/Imported.ttf", error);
    QCOMPARE(reader->font_face_css(), QString());
}

QTEST_GUILESS_MAIN(ReaderFontsTest)
#include "tst_reader_fonts.moc"
