// SPDX-License-Identifier: GPL-3.0-or-later
//
// Exercises the reader's native lookup boundary and the mining metadata
// properties without reimplementing selection.js or the popup.

#include "viewmodels/reader/reader_view_model.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTcpServer>
#include <QUrlQuery>
#include <QtTest>

#include <zip.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "data_root.h"
#include "services/anki/field_content.h"
#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"

using niratan::viewmodels::ReaderViewModel;

class ReaderMiningContextTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void cleanup();
    void relays_the_js_sentence_and_lookup_text_independently();
    void rejects_invalid_selections_data();
    void rejects_invalid_selections();
    void reports_the_lookup_offset_data();
    void reports_the_lookup_offset();
    void exposes_current_book_context_and_clears_it();
    void keeps_url_for_an_unreadable_cover();
    void serves_cover_and_gallery_on_the_same_port();
    void keeps_cover_path_when_the_port_is_in_use();

   private:
    static bool open_book(ReaderViewModel& reader, const QString& folder);
    bool open_book(const QString& folder) { return open_book(*reader_, folder); }

    niratan::test::TempDataRoot data_root_;
    ReaderViewModel* reader_ = nullptr;
};

namespace {

bool write_epub(const std::filesystem::path& path, std::string_view title_markup) {
    int error = 0;
    zip_t* archive = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!archive) {
        return false;
    }
    const std::string opf = R"XML(
        <package xmlns="http://www.idpf.org/2007/opf" version="3.0">
            <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    )XML" +
        std::string(title_markup) +
        R"XML(
            </metadata>
            <manifest>
                <item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/>
                <item id="illustration" href="illustration.png" media-type="image/png"/>
            </manifest>
            <spine><itemref idref="chapter"/></spine>
        </package>
    )XML";
    const std::pair<const char*, std::string_view> entries[]{
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", R"XML(
            <container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0">
                <rootfiles><rootfile full-path="book.opf"
                    media-type="application/oebps-package+xml"/></rootfiles>
            </container>
        )XML"},
        {"book.opf", opf},
        {"chapter.xhtml",
         R"XML(<html><body><p>本文です。</p><img src="illustration.png"/></body></html>)XML"},
        {"illustration.png", "gallery image bytes"},
    };
    for (const auto& [name, bytes] : entries) {
        auto* source = zip_source_buffer(archive, bytes.data(), bytes.size(), 0);
        if (!source || zip_file_add(archive, name, source, 0) < 0) {
            if (source) {
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

bool make_book(const QString& folder, std::string_view title_markup,
               std::optional<std::string> cover = std::nullopt) {
    const niratan::books::BookStorage storage(niratan::storage::data_root());
    niratan::books::BookMetadata book;
    book.id = folder.toStdString();
    book.folder = folder.toStdString();
    book.title = "Library title";
    book.renamed_title = "Renamed " + book.folder;
    book.epub = "book.epub";
    book.cover = std::move(cover);
    const auto directory = storage.book_directory(book.folder);
    return directory && QDir().mkpath(QString::fromStdString(directory->string())) &&
        write_epub(*directory / "book.epub", title_markup) && storage.save_metadata(book);
}

QString revision(const ReaderViewModel& reader) {
    return QUrlQuery(QUrl(reader.chapter_url())).queryItemValue("v");
}

void send(ReaderViewModel& reader, const QString& name, const QJsonObject& data) {
    reader.message(name, QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
}

QJsonObject selection(const ReaderViewModel& reader, const QString& sentence) {
    const QJsonObject sheet_sentence{{"id", "0"}, {"text", "A different context-sheet sentence."}};
    return {{"token", revision(reader)},
            {"text", "　学校へ "},
            {"sentence", sentence},
            {"rect", QJsonObject{{"x", -2.5}, {"y", 0}, {"width", 15}, {"height", 21.5}}},
            {"normalizedOffset", 42},
            {"miningContext",
             QJsonObject{{"currentIndex", 0}, {"sentences", QJsonArray{sheet_sentence}}}}};
}

bool write_cover(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

void expect_response(QNetworkAccessManager& network, const QUrl& url, const QByteArray& bytes) {
    auto* reply = network.get(QNetworkRequest(url));
    QSignalSpy finished(reply, &QNetworkReply::finished);
    QVERIFY(reply->isFinished() || finished.wait(5000));
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    QCOMPARE(reply->readAll(), bytes);
}

}  // namespace

void ReaderMiningContextTest::initTestCase() {
    QVERIFY(data_root_.isValid());
    QVERIFY(make_book("First", "<dc:title>原題</dc:title>", "Books/First/表紙.PNG"));
    QVERIFY(make_book("Second", "<dc:title/>"));
    const auto missing_cover = data_root_.path() + "/missing.jpg";
    QVERIFY(make_book("Third", "<dc:title>Without an image file</dc:title>",
                      missing_cover.toStdString()));
    const auto cover_path = niratan::storage::data_root() / "Books/First/表紙.PNG";
    QVERIFY(
        write_cover(QString::fromStdString(cover_path.string()), QByteArray("cover\0bytes", 11)));
    reader_ = ReaderViewModel::instance(0);
}

bool ReaderMiningContextTest::open_book(ReaderViewModel& reader, const QString& folder) {
    if (!reader.openBook(folder) || revision(reader).isEmpty()) {
        return false;
    }
    send(reader, "restoreCompleted", {{"token", revision(reader)}});
    return !reader.restoring();
}

void ReaderMiningContextTest::cleanup() {
    if (reader_) {
        reader_->closeBook();
    }
}

// B1, B7, B8, B14, B15: NativeReaderWebView.Coordinator.textSelected,
// NativeReaderModel.handleSelection, PopupView.mineEntry, mineAnkiEntry,
// and AnkiManager.handlebarToValue (.sentence).
void ReaderMiningContextTest::relays_the_js_sentence_and_lookup_text_independently() {
    QVERIFY(open_book("First"));
    QSignalSpy lookup(reader_, &ReaderViewModel::lookupRequested);
    const QString sentence = "　学校へ行って、学校で読みます。\n";
    send(*reader_, "textSelected", selection(*reader_, sentence));
    send(*reader_, "textSelected", selection(*reader_, QStringLiteral("")));
    QCOMPARE(lookup.size(), 2);
    const auto first = lookup.at(0);
    QCOMPARE(first.value(0).toString(), QString("　学校へ "));
    QCOMPARE(first.value(1).toString(), sentence);
    QCOMPARE(first.value(2).toRectF(), QRectF(-2.5, 0, 15, 21.5));
    QVERIFY(lookup.at(1).value(1).toString().isEmpty());

    niratan::anki::MiningContext context;
    context.sentence = first.value(1).toString().toStdString();
    QCOMPARE(niratan::anki::render_field("{sentence}", context, {{"matched", "学校"}}, {}, {}),
             std::string("　<b>学校</b>へ行って、<b>学校</b>で読みます。\n"));
}

// B7: NativeReaderWebView.Coordinator.textSelected and cgFloatValue; Linux
// retains its guards against blank text and nonpositive rect sizes.
void ReaderMiningContextTest::rejects_invalid_selections_data() {
    QTest::addColumn<QString>("key");
    QTest::addColumn<bool>("in_rect");
    QTest::addColumn<QJsonValue>("value");

    QTest::newRow("missing-sentence")
        << QString("sentence") << false << QJsonValue(QJsonValue::Undefined);
    QTest::newRow("nonstring-sentence") << QString("sentence") << false << QJsonValue(7);
    QTest::newRow("null-sentence") << QString("sentence") << false << QJsonValue();
    QTest::newRow("nonstring-text") << QString("text") << false << QJsonValue(7);
    QTest::newRow("missing-x") << QString("x") << true << QJsonValue(QJsonValue::Undefined);
    QTest::newRow("nonnumeric-x") << QString("x") << true << QJsonValue("0");
    QTest::newRow("missing-y") << QString("y") << true << QJsonValue(QJsonValue::Undefined);
    QTest::newRow("nonnumeric-y") << QString("y") << true << QJsonValue("0");
    QTest::newRow("zero-width") << QString("width") << true << QJsonValue(0);
    QTest::newRow("negative-height") << QString("height") << true << QJsonValue(-1);
    QTest::newRow("blank-text") << QString("text") << false << QJsonValue("　\t");
}

// B7: NativeReaderWebView.Coordinator.textSelected rejects malformed payloads.
void ReaderMiningContextTest::rejects_invalid_selections() {
    QFETCH(QString, key);
    QFETCH(bool, in_rect);
    QFETCH(QJsonValue, value);

    QVERIFY(open_book("Second"));
    QSignalSpy lookup(reader_, &ReaderViewModel::lookupRequested);
    auto payload = selection(*reader_, "本文です。");
    send(*reader_, "textSelected", payload);
    QCOMPARE(lookup.size(), 1);
    if (in_rect) {
        auto rect = payload.value("rect").toObject();
        rect.insert(key, value);
        payload.insert("rect", rect);
    } else {
        payload.insert(key, value);
    }
    send(*reader_, "textSelected", payload);
    QCOMPARE(lookup.size(), 1);
}

// B2: SelectionData.normalizedOffset reaches lookupOffset; only an integral
// JSON number counts.
void ReaderMiningContextTest::reports_the_lookup_offset_data() {
    QTest::addColumn<QJsonValue>("offset");
    QTest::addColumn<int>("expected");

    QTest::newRow("integer") << QJsonValue(42) << 42;
    QTest::newRow("absent") << QJsonValue(QJsonValue::Undefined) << -1;
    QTest::newRow("null") << QJsonValue() << -1;
    QTest::newRow("fractional") << QJsonValue(42.5) << -1;
    QTest::newRow("boolean") << QJsonValue(true) << -1;
}

void ReaderMiningContextTest::reports_the_lookup_offset() {
    QFETCH(QJsonValue, offset);
    QFETCH(int, expected);

    QVERIFY(open_book("Second"));
    QSignalSpy lookup(reader_, &ReaderViewModel::lookupRequested);
    auto payload = selection(*reader_, "本文です。");
    if (offset.isUndefined()) {
        payload.remove("normalizedOffset");
    } else {
        payload.insert("normalizedOffset", offset);
    }
    send(*reader_, "textSelected", payload);
    QCOMPARE(lookup.size(), 1);
    QCOMPARE(reader_->lookup_offset(), expected);
}

// B9–B13: NativeReaderModel.title and coverURL, popupLayer, PopupView.mineEntry,
// BookMetadata.coverURL, LocalFileServer.setCover and AnkiManager.handlebarToValue (.bookCover).
void ReaderMiningContextTest::exposes_current_book_context_and_clears_it() {
    const QString path =
        QString::fromStdString((niratan::storage::data_root() / "Books/First/表紙.PNG").string());
    QList<QStringList> lookups;
    const auto connection = connect(
        reader_, &ReaderViewModel::lookupRequested, this,
        [reader = reader_, &lookups](const QString&, const QString& sentence, const QRectF&) {
            lookups.append({sentence, reader->property("documentTitle").toString(),
                            reader->property("coverPath").toString(),
                            reader->property("coverUrl").toString()});
        });
    const auto disconnect_lookup = qScopeGuard([&] { disconnect(connection); });

    QVERIFY(open_book("First"));
    QCOMPARE(reader_->title(), QString("Renamed First"));
    const int port = QUrl(reader_->cover_url()).port();
    QVERIFY(port > 0);
    const QString url = QStringLiteral("http://localhost:%1/cover/cover.PNG").arg(port);
    send(*reader_, "textSelected", selection(*reader_, "First sentence."));
    QCOMPARE(lookups.size(), 1);
    QCOMPARE(lookups.at(0), (QStringList{"First sentence.", "原題", path, url}));

    QVERIFY(open_book("Second"));
    send(*reader_, "textSelected", selection(*reader_, "Second sentence."));
    QCOMPARE(lookups.size(), 2);
    QCOMPARE(lookups.at(1), (QStringList{"Second sentence.", "Renamed Second", "", ""}));
    const auto late_selection = selection(*reader_, "Closed book.");
    reader_->closeBook();
    QVERIFY(reader_->document_title().isEmpty());
    QVERIFY(reader_->cover_path().isEmpty());
    QVERIFY(reader_->cover_url().isEmpty());
    send(*reader_, "textSelected", late_selection);
    QCOMPARE(lookups.size(), 2);
}

// B10, B11: BookMetadata.coverURL, LocalFileServer.setCover and
// AnkiManager.handlebarToValue (.bookCover) ignore the cover read error.
void ReaderMiningContextTest::keeps_url_for_an_unreadable_cover() {
    QVERIFY(open_book("Third"));
    const QString missing_cover = data_root_.path() + "/missing.jpg";
    QVERIFY(!QFile::exists(missing_cover));
    QCOMPARE(reader_->cover_path(), missing_cover);
    const QString url = reader_->cover_url();
    const int port = QUrl(url).port();
    QVERIFY(port > 0);
    QCOMPARE(url, QStringLiteral("http://localhost:%1/cover/cover.jpg").arg(port));
}

// B11: AnkiManager.handlebarToValue (.bookCover) and LocalFileServer.setCover,
// getCover and port. The reader hosts the cover and gallery on one server.
void ReaderMiningContextTest::serves_cover_and_gallery_on_the_same_port() {
    QVERIFY(open_book("First"));
    const QString url = reader_->cover_url();
    const int port = QUrl(url).port();
    QVERIFY(port > 0);
    QCOMPARE(url, QStringLiteral("http://localhost:%1/cover/cover.PNG").arg(port));
    QNetworkAccessManager network;
    expect_response(network, QUrl(url), QByteArray("cover\0bytes", 11));

    reader_->loadGallery();
    QTRY_VERIFY(!reader_->gallery_loading());
    const auto images = reader_->gallery_images();
    QCOMPARE(images.size(), 1);
    const QUrl image_url = images.at(0).toMap().value("url").toUrl();
    QCOMPARE(image_url.port(), port);
    expect_response(network, image_url, QByteArray("gallery image bytes"));
}

// B11, B12: LocalFileServer.port and AnkiManager.addNoteAnkiConnect; Linux
// preserves the attachment path and serves the gallery on an ephemeral port.
void ReaderMiningContextTest::keeps_cover_path_when_the_port_is_in_use() {
    QTcpServer other_listener;
    QVERIFY2(other_listener.listen(QHostAddress::Any, 0), qPrintable(other_listener.errorString()));
    ReaderViewModel reader(other_listener.serverPort());
    QVERIFY(open_book(reader, "First"));
    QVERIFY(reader.cover_url().isEmpty());

    reader.loadGallery();
    QTRY_VERIFY(!reader.gallery_loading());
    const auto images = reader.gallery_images();
    QCOMPARE(images.size(), 1);
    const QUrl image_url = images.at(0).toMap().value("url").toUrl();
    QVERIFY(image_url.port() > 0);
    QVERIFY(image_url.port() != other_listener.serverPort());
    QNetworkAccessManager network;
    expect_response(network, image_url, QByteArray("gallery image bytes"));
    const auto cover_path = niratan::storage::data_root() / "Books/First/表紙.PNG";
    QCOMPARE(reader.cover_path(), QString::fromStdString(cover_path.string()));
    QVERIFY(reader.cover_url().isEmpty());
}

QTEST_GUILESS_MAIN(ReaderMiningContextTest)
#include "tst_reader_mining_context.moc"
