// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/books/book.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

#include <optional>
#include <string>
#include <vector>

#include "services/storage/json_file.h"

using niratan::books::BookInfo;
using niratan::books::BookMetadata;
using niratan::books::BookShelf;
using niratan::books::Bookmark;
using niratan::books::ChapterInfo;
using niratan::books::book_info_from_json;
using niratan::books::book_info_to_json;
using niratan::books::book_metadata_from_json;
using niratan::books::book_metadata_to_json;
using niratan::books::book_order_from_json;
using niratan::books::book_order_to_json;
using niratan::books::bookmark_from_json;
using niratan::books::bookmark_to_json;
using niratan::books::is_in_progress;
using niratan::books::reorder_book_id;
using niratan::books::reorder_destination;
using niratan::books::reorder_payload;
using niratan::books::shelves_from_json;
using niratan::books::shelves_to_json;

// Models/Book.swift (BookMetadata, Bookmark, BookInfo, BookShelf, BookReorder)
// and script tests define the shapes; the JSON keys are the synthesized
// Codable names and dates are Foundation's reference seconds.
namespace {

BookMetadata sample_metadata() {
    BookMetadata book;
    book.id = "0F0F0F0F-1111-2222-3333-444444444444";
    book.title = "Title";
    book.epub = "book.epub";
    book.cover = "cover.jpg";
    book.folder = "Title";
    book.last_access = 700000000.5;
    book.renamed_title = "Renamed";
    book.profile_id = "default-ja";
    book.book_language = "ja";
    book.external_source_id = "source";
    book.external_isbn = "9780000000000";
    book.author = "Author";
    book.character_count = 12345;
    book.modified = 1700000000;
    return book;
}

}  // namespace

class BookModelTest : public QObject {
    Q_OBJECT

   private slots:
    void metadata_round_trips();
    void metadata_uses_the_swift_keys();
    void metadata_missing_optional_keys_stay_empty();
    void metadata_requires_id_title_folder_and_last_access();
    void display_title_prefers_the_renamed_title();
    void bookmark_round_trips();
    void bookmark_requires_its_keys();
    void book_info_round_trips();
    void chapter_info_round_trips_the_optional_fields();
    void resolving_a_character_position_uses_chapter_ranges();
    void shelves_and_order_round_trip();
    void reorder_payloads_carry_the_book_id();
    void reorder_destinations_follow_book_reorder();
    void progress_counts_between_zero_and_the_done_threshold();
};

void BookModelTest::metadata_round_trips() {
    const BookMetadata book = sample_metadata();
    const auto decoded = book_metadata_from_json(book_metadata_to_json(book));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, book);
}

void BookModelTest::metadata_uses_the_swift_keys() {
    const QJsonObject object = book_metadata_to_json(sample_metadata()).object();

    QCOMPARE(object.value("id").toString(), QString("0F0F0F0F-1111-2222-3333-444444444444"));
    QCOMPARE(object.value("title").toString(), QString("Title"));
    QCOMPARE(object.value("epub").toString(), QString("book.epub"));
    QCOMPARE(object.value("cover").toString(), QString("cover.jpg"));
    QCOMPARE(object.value("folder").toString(), QString("Title"));
    QCOMPARE(object.value("lastAccess").toDouble(), 700000000.5);
    QCOMPARE(object.value("renamedTitle").toString(), QString("Renamed"));
    QCOMPARE(object.value("profileId").toString(), QString("default-ja"));
    QCOMPARE(object.value("bookLanguage").toString(), QString("ja"));
    QCOMPARE(object.value("externalSourceID").toString(), QString("source"));
    QCOMPARE(object.value("externalISBN").toString(), QString("9780000000000"));
    QCOMPARE(object.value("author").toString(), QString("Author"));
    QCOMPARE(object.value("characterCount").toInt(), 12345);
    QCOMPARE(object.value("modified").toInteger(), 1700000000);
}

void BookModelTest::metadata_missing_optional_keys_stay_empty() {
    const auto book = book_metadata_from_json(QJsonDocument::fromJson(
        "{\"id\":\"0F0F0F0F-1111-2222-3333-444444444444\",\"title\":\"Title\","
        "\"folder\":\"Title\",\"lastAccess\":0}"));

    QVERIFY(book.has_value());
    QVERIFY(!book->epub.has_value());
    QVERIFY(!book->cover.has_value());
    QVERIFY(!book->renamed_title.has_value());
    QCOMPARE(book->last_access, 0.0);
}

void BookModelTest::metadata_requires_id_title_folder_and_last_access() {
    QVERIFY(!book_metadata_from_json(QJsonDocument::fromJson("{}")).has_value());
    QVERIFY(!book_metadata_from_json(QJsonDocument::fromJson(
                 "{\"id\":1,\"title\":\"T\",\"folder\":\"F\",\"lastAccess\":0}"))
                 .has_value());
    QVERIFY(!book_metadata_from_json(QJsonDocument::fromJson(
                 "{\"id\":\"A\",\"title\":\"T\",\"folder\":\"F\"}"))
                 .has_value());
}

void BookModelTest::display_title_prefers_the_renamed_title() {
    BookMetadata book = sample_metadata();
    QCOMPARE(book.display_title(), std::string("Renamed"));
    book.renamed_title.reset();
    QCOMPARE(book.display_title(), std::string("Title"));
}

void BookModelTest::bookmark_round_trips() {
    Bookmark bookmark;
    bookmark.chapter_index = 3;
    bookmark.progress = 0.25;
    bookmark.character_count = 900;
    bookmark.last_modified = 700000000.25;

    const auto decoded = bookmark_from_json(bookmark_to_json(bookmark));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, bookmark);
}

void BookModelTest::bookmark_requires_its_keys() {
    QVERIFY(!bookmark_from_json(QJsonDocument::fromJson("{}")).has_value());
    QVERIFY(!bookmark_from_json(QJsonDocument::fromJson(
                 "{\"chapterIndex\":0,\"progress\":0.5}"))
                 .has_value());

    const auto bookmark = bookmark_from_json(QJsonDocument::fromJson(
        "{\"chapterIndex\":0,\"progress\":0.5,\"characterCount\":10}"));
    QVERIFY(bookmark.has_value());
    QVERIFY(!bookmark->last_modified.has_value());
}

void BookModelTest::book_info_round_trips() {
    BookInfo info;
    info.character_count = 5000;
    info.chapter_info["ch1.xhtml"] =
        ChapterInfo{0, 0, 3000, std::map<std::string, int>{{"s1", 10}}};
    info.chapter_info["ch2.xhtml"] = ChapterInfo{1, 3000, 2000, std::nullopt};
    info.images = std::vector<std::string>{"a.jpg", "b.jpg"};
    info.image_positions = std::map<std::string, int>{{"a.jpg", 5}, {"b.jpg", 3001}};

    const auto decoded = book_info_from_json(book_info_to_json(info));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, info);
}

void BookModelTest::chapter_info_round_trips_the_optional_fields() {
    BookInfo info;
    info.character_count = 10;
    info.chapter_info["ch.xhtml"] = ChapterInfo{std::nullopt, 0, 10, std::nullopt};

    const QJsonObject object = book_info_to_json(info).object();
    const QJsonObject chapter =
        object.value("chapterInfo").toObject().value("ch.xhtml").toObject();
    QVERIFY(!chapter.contains("spineIndex"));
    QVERIFY(!chapter.contains("fragmentOffsets"));
    QCOMPARE(chapter.value("currentTotal").toInt(), 0);
    QCOMPARE(chapter.value("chapterCount").toInt(), 10);

    const auto decoded = book_info_from_json(book_info_to_json(info));
    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, info);
}

void BookModelTest::resolving_a_character_position_uses_chapter_ranges() {
    BookInfo info;
    info.character_count = 100;
    info.chapter_info["a"] = ChapterInfo{0, 0, 40, std::nullopt};
    info.chapter_info["b"] = ChapterInfo{1, 40, 60, std::nullopt};

    const auto inside_b = info.resolve_character_position(70);
    QVERIFY(inside_b.has_value());
    QCOMPARE(inside_b->first, 1);
    QCOMPARE(inside_b->second, 0.5);

    const auto first = info.resolve_character_position(0);
    QVERIFY(first.has_value());
    QCOMPARE(first->first, 0);
    QCOMPARE(first->second, 0.0);

    const auto clamped = info.resolve_character_position(1000);
    QVERIFY(clamped.has_value());
    QCOMPARE(clamped->first, 1);
    QCOMPARE(clamped->second, 59.0 / 60.0);

    const auto negative = info.resolve_character_position(-1);
    QVERIFY(negative.has_value());
    QCOMPARE(negative->first, 0);
    QCOMPARE(negative->second, 0.0);
}

void BookModelTest::shelves_and_order_round_trip() {
    const std::vector<BookShelf> shelves{
        BookShelf{"Favourites",
                  {"0F0F0F0F-1111-2222-3333-444444444444",
                   "1F1F1F1F-1111-2222-3333-444444444444"}},
        BookShelf{"Later", {}},
    };
    const auto decoded = shelves_from_json(shelves_to_json(shelves));
    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, shelves);

    const std::vector<std::string> order{"0F0F0F0F-1111-2222-3333-444444444444"};
    const auto decoded_order = book_order_from_json(book_order_to_json(order));
    QVERIFY(decoded_order.has_value());
    QCOMPARE(*decoded_order, order);
}

void BookModelTest::reorder_payloads_carry_the_book_id() {
    const std::string id = "0F0F0F0F-1111-2222-3333-444444444444";
    QCOMPARE(reorder_payload(id), std::string("hoshi-book:") + id);
    QCOMPARE(reorder_book_id(reorder_payload(id)), std::optional<std::string>{id});
    QVERIFY(!reorder_book_id(std::string("other:") + id).has_value());
    QVERIFY(!reorder_book_id(std::string("hoshi-book:not-a-uuid")).has_value());
}

void BookModelTest::reorder_destinations_follow_book_reorder() {
    QVERIFY(!reorder_destination(2, 2).has_value());
    QCOMPARE(reorder_destination(1, 3), std::optional<int>{4});
    QCOMPARE(reorder_destination(3, 1), std::optional<int>{1});
    QVERIFY(!reorder_destination(0, 0).has_value());
}

void BookModelTest::progress_counts_between_zero_and_the_done_threshold() {
    QVERIFY(!is_in_progress(0.0));
    QVERIFY(is_in_progress(0.001));
    QVERIFY(is_in_progress(0.9989));
    QVERIFY(!is_in_progress(0.999));
    QVERIFY(!is_in_progress(1.0));
}

QTEST_APPLESS_MAIN(BookModelTest)
#include "tst_book_model.moc"
