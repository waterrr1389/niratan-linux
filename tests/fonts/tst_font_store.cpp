// SPDX-License-Identifier: GPL-3.0-or-later
//
// Imported reader fonts, following Core/FontManager.swift (fontsDirectory,
// storedFonts, storedFontUrl, importFont, deleteFont, isDefaultFont,
// defaultFonts, cssFontName, resourceName, fontfaceCSS) and the copy helpers in
// Core/BookStorage.swift. The store reads and writes <app>/Fonts, so the test
// points the data root at a temporary directory.
//
// Swift's storedFonts lists whatever contentsOfDirectory returns except the
// System directory; this port skips a directory whose name looks like a font,
// because such an entry is neither servable nor deletable.

#include "services/fonts/font_store.h"

#include "data_root.h"
#include "services/storage/app_paths.h"

#include <QFile>
#include <QTest>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

using niratan::fonts::FontStore;
using niratan::test::TempDataRoot;

namespace {

bool prepare_app() {
    std::error_code error;
    std::filesystem::create_directories(niratan::storage::data_root(), error);
    return !error;
}

bool write_bytes(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(bytes) == bytes.size();
}

QByteArray read_bytes(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

}  // namespace

class FontStoreTest : public QObject {
    Q_OBJECT

   private slots:
    void init();
    void stored_fonts_lists_files_only();
    void import_copies_replaces_and_refuses();
    void delete_removes_only_stored_fonts();
    void css_lists_each_stored_font();
    void font_resource_serves_font_files();

   private:
    TempDataRoot data_;
};

void FontStoreTest::init() {
    std::error_code error;
    std::filesystem::remove_all(niratan::storage::data_root() / "Fonts", error);
}

// B1: the list is the regular files of Fonts in name order; hidden entries,
// the System directory and a directory named like a font are not fonts.
void FontStoreTest::stored_fonts_lists_files_only() {
    QVERIFY(data_.isValid());
    QVERIFY(prepare_app());
    const FontStore store(niratan::storage::data_root());
    const auto directory = store.directory();
    std::error_code error;
    std::filesystem::create_directories(directory / "System", error);
    std::filesystem::create_directories(directory / "Folder.ttf", error);
    QVERIFY(write_bytes(directory / "Bold.otf", QByteArrayLiteral("otf")));
    QVERIFY(write_bytes(directory / "Alpha.ttf", QByteArrayLiteral("ttf")));
    QVERIFY(write_bytes(directory / ".Hidden.ttf", QByteArrayLiteral("hidden")));
    QVERIFY(write_bytes(directory / "System" / "System.ttf", QByteArrayLiteral("system")));

    QCOMPARE(store.stored_font_names(), (std::vector<std::string>{"Alpha", "Bold"}));
    const auto files = store.stored_fonts();
    QCOMPARE(files.size(), std::size_t(2));
    QCOMPARE(files.front().filename().string(), std::string("Alpha.ttf"));
    QCOMPARE(store.stored_font("Bold"), std::optional(directory / "Bold.otf"));
    QVERIFY(!store.stored_font("Folder").has_value());
    QVERIFY(!store.stored_font("Hidden").has_value());
    QVERIFY(!store.stored_font("System").has_value());
}

// B2: import copies under the source file name, creating Fonts, and replaces
// an existing file; a missing source, a directory or an empty name is refused.
void FontStoreTest::import_copies_replaces_and_refuses() {
    QVERIFY(data_.isValid());
    QVERIFY(prepare_app());
    const FontStore store(niratan::storage::data_root());
    const auto app = niratan::storage::data_root();
    const QByteArray first = QByteArrayLiteral("first-bytes");
    QVERIFY(write_bytes(app / "Imported Font.ttf", first));

    std::string error;
    QVERIFY(store.import_font(app / "Imported Font.ttf", &error));
    QVERIFY(error.empty());
    const auto destination = store.directory() / "Imported Font.ttf";
    QCOMPARE(read_bytes(destination), first);
    QCOMPARE(store.stored_font_names(), (std::vector<std::string>{"Imported Font"}));

    const QByteArray second = QByteArrayLiteral("second-bytes");
    QVERIFY(write_bytes(app / "Imported Font.ttf", second));
    QVERIFY(store.import_font(app / "Imported Font.ttf", &error));
    QCOMPARE(read_bytes(destination), second);

    error.clear();
    QVERIFY(!store.import_font(app / "missing.ttf", &error));
    QVERIFY(!error.empty());
    QVERIFY(!std::filesystem::exists(store.directory() / "missing.ttf"));

    std::error_code ignored;
    std::filesystem::create_directories(app / "Directory.ttf", ignored);
    error.clear();
    QVERIFY(!store.import_font(app / "Directory.ttf", &error));
    QVERIFY(!error.empty());

    error.clear();
    QVERIFY(!store.import_font(std::filesystem::path{}, &error));
    QVERIFY(!error.empty());
    QCOMPARE(store.stored_font_names(), (std::vector<std::string>{"Imported Font"}));
}

// B3: delete removes a stored file by name; an unknown name or a default font
// changes nothing.
void FontStoreTest::delete_removes_only_stored_fonts() {
    QVERIFY(data_.isValid());
    QVERIFY(prepare_app());
    const FontStore store(niratan::storage::data_root());
    const auto app = niratan::storage::data_root();
    QVERIFY(write_bytes(app / "Gone.ttf", QByteArrayLiteral("gone")));
    QVERIFY(write_bytes(app / "Hiragino Mincho ProN.ttf", QByteArrayLiteral("kept")));
    std::string error;
    QVERIFY(store.import_font(app / "Gone.ttf", &error));
    QVERIFY(store.import_font(app / "Hiragino Mincho ProN.ttf", &error));

    QVERIFY(store.delete_font("Missing"));
    QVERIFY(store.delete_font("Hiragino Mincho ProN"));
    QVERIFY(std::filesystem::exists(store.directory() / "Hiragino Mincho ProN.ttf"));
    QVERIFY(store.delete_font("Gone", &error));
    QVERIFY(!std::filesystem::exists(store.directory() / "Gone.ttf"));
    QCOMPARE(store.stored_font_names(), (std::vector<std::string>{"Hiragino Mincho ProN"}));
}

// B4: one block per stored font, family = base name, source the reader scheme
// URL with the percent-encoded file name; empty without fonts.
void FontStoreTest::css_lists_each_stored_font() {
    QVERIFY(data_.isValid());
    QVERIFY(prepare_app());
    const FontStore store(niratan::storage::data_root());
    QCOMPARE(store.font_face_css(), std::string{});
    const auto directory = store.directory();
    QVERIFY(write_bytes(directory / "My Font.ttf", QByteArrayLiteral("a")));
    QCOMPARE(store.font_face_css(),
             std::string("@font-face {\n"
                         "    font-family: 'My Font';\n"
                         "    src: url('reader://font/My%20Font.ttf');\n"
                         "}"));
    QVERIFY(write_bytes(directory / "Second.otf", QByteArrayLiteral("b")));
    QCOMPARE(store.font_face_css(),
             std::string("@font-face {\n"
                         "    font-family: 'My Font';\n"
                         "    src: url('reader://font/My%20Font.ttf');\n"
                         "}@font-face {\n"
                         "    font-family: 'Second';\n"
                         "    src: url('reader://font/Second.otf');\n"
                         "}"));
}

// B5: a stored font's exact file name is served with its font MIME type; a
// path, an unknown name or a non-font extension is not a resource.
void FontStoreTest::font_resource_serves_font_files() {
    QVERIFY(data_.isValid());
    QVERIFY(prepare_app());
    const FontStore store(niratan::storage::data_root());
    const auto directory = store.directory();
    std::error_code directory_error;
    std::filesystem::create_directories(directory, directory_error);
    QVERIFY(write_bytes(directory / "My Font.ttf", QByteArrayLiteral("ttf-bytes")));
    QVERIFY(write_bytes(directory / "Color.woff2", QByteArrayLiteral("woff2-bytes")));
    QVERIFY(write_bytes(directory / "Notes.txt", QByteArrayLiteral("text")));

    const auto ttf = store.font_resource("My Font.ttf");
    QVERIFY(ttf.has_value());
    QCOMPARE(ttf->bytes, std::string("ttf-bytes"));
    QCOMPARE(ttf->mime_type, std::string("font/ttf"));
    const auto woff2 = store.font_resource("Color.woff2");
    QVERIFY(woff2.has_value());
    QCOMPARE(woff2->mime_type, std::string("font/woff2"));

    QVERIFY(!store.font_resource("Missing.ttf").has_value());
    QVERIFY(!store.font_resource("../My Font.ttf").has_value());
    QVERIFY(!store.font_resource("sub/My Font.ttf").has_value());
    QVERIFY(!store.font_resource("Notes.txt").has_value());
    QCOMPARE(std::string(niratan::fonts::font_mime_type("x.otf")), std::string("font/otf"));
    QCOMPARE(std::string(niratan::fonts::font_mime_type("x.ttc")), std::string("font/collection"));
    QCOMPARE(std::string(niratan::fonts::font_mime_type("x.woff")), std::string("font/woff"));
    QCOMPARE(std::string(niratan::fonts::font_mime_type("x.txt")), std::string{});
}

QTEST_GUILESS_MAIN(FontStoreTest)
#include "tst_font_store.moc"
