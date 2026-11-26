// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/books/library_model.h"

#include <QLocale>
#include <QtTest>

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using niratan::books::BookInfo;
using niratan::books::Bookmark;
using niratan::books::BookMetadata;
using niratan::books::BookShelf;
using niratan::books::ChapterInfo;
using niratan::books::Library;
using niratan::books::LibraryBook;
using niratan::books::LibrarySection;
using niratan::books::LibraryShelf;
using niratan::books::LibraryShelfSelection;
using niratan::books::SortOption;
using niratan::books::book_count;
using niratan::books::book_progress;
using niratan::books::move_book_before;
using niratan::books::move_book_to_shelf;
using niratan::books::normalized_order;
using niratan::books::normalized_shelf_name;
using niratan::books::normalized_shelves;
using niratan::books::read_bookmark;
using niratan::books::reading_books;
using niratan::books::reorder_ids;
using niratan::books::resolved_selection;
using niratan::books::section_books;
using niratan::books::shelf_name_available;
using niratan::books::shelf_sections;
using niratan::books::sort_books;
using niratan::books::unique_shelf_name;
using niratan::books::unshelved_books;

// Features/Bookshelf/BookshelfViewModel.swift and LibraryShelfSelection.swift
// state the ordering, shelving and progress rules. Each test names the rule it
// holds, including the places where the port deliberately differs from macOS:
// a clamped progress quotient and byte-equal shelf keys where Swift compares
// canonically equivalent spellings as the same name.
namespace {

BookMetadata metadata(std::string id, std::string title, double last_access = 0.0) {
    BookMetadata book;
    book.id = std::move(id);
    book.title = std::move(title);
    book.folder = book.title;
    book.last_access = last_access;
    return book;
}

LibraryBook library_book(std::string id, std::string title, double last_access = 0.0) {
    LibraryBook book;
    book.metadata = metadata(std::move(id), std::move(title), last_access);
    return book;
}

// The pair loadBookProgress divides: a bookmark character count and a bookinfo
// total. bookmark.progress is set to a value the model must ignore.
void set_progress(LibraryBook& book, int characters, int total) {
    Bookmark bookmark;
    bookmark.progress = 0.5;
    bookmark.character_count = characters;
    book.bookmark = bookmark;

    BookInfo info;
    info.character_count = total;
    book.book_info = info;
}

LibraryShelfSelection selection_of(LibraryShelf shelf, std::string name = {}) {
    LibraryShelfSelection selection;
    selection.shelf = shelf;
    selection.name = std::move(name);
    return selection;
}

std::vector<std::string> ids_of(const std::vector<LibraryBook>& books) {
    std::vector<std::string> ids;
    ids.reserve(books.size());
    for (const LibraryBook& book : books) {
        ids.push_back(book.metadata.id);
    }
    return ids;
}

// a and d are in progress, a is on the first shelf, c on the second.
Library progress_library() {
    std::vector<LibraryBook> books{
        library_book("a", "Alpha"),
        library_book("b", "Beta"),
        library_book("c", "Gamma"),
        library_book("d", "Delta"),
    };
    set_progress(books[0], 500, 1000);
    set_progress(books[1], 999, 1000);
    set_progress(books[2], 0, 1000);
    set_progress(books[3], 1, 1000);

    Library library;
    library.books = std::move(books);
    library.shelves = {BookShelf{"First", {"a"}}, BookShelf{"Second", {"c"}}};
    return library;
}

}  // namespace

class LibraryModelTest : public QObject {
    Q_OBJECT

   private slots:
    void recent_sort_keeps_the_newest_first_and_breaks_ties_by_title();
    void title_sort_uses_localized_numeric_display_titles();
    void manual_sort_keeps_known_ids_first_and_breaks_unknown_ties_by_id();
    void normalized_order_drops_unknown_and_duplicate_ids();
    void normalized_shelves_drop_unknown_and_duplicate_ids();
    void normalized_shelf_name_trims_and_rejects_blank();
    void shelf_name_availability_ignores_case();
    void unique_shelf_name_counts_from_two();
    void reading_books_are_between_zero_and_the_done_threshold();
    void progress_uses_the_bookmark_over_the_bookinfo_total();
    void progress_falls_back_to_the_metadata_character_count();
    void progress_is_zero_without_a_positive_total_or_a_bookmark();
    void progress_clamps_out_of_range_character_counts();
    void section_books_follow_the_selection();
    void section_books_sort_a_shelf_by_its_own_order();
    void book_count_counts_the_selection();
    void resolved_selection_falls_back_to_all();
    void duplicate_shelf_names_use_the_first_match();
    void shelf_sections_put_reading_first_and_unshelved_last();
    void move_book_to_shelf_removes_from_every_shelf_then_appends();
    void reorder_ids_moves_down_and_up();
    void reorder_ids_appends_missing_fallback_ids();
    void move_book_before_reorders_a_shelf_list();
    void move_book_before_reorders_only_the_manual_slice();
    void move_book_before_ignores_the_reading_section();
    void move_shelves_uses_original_insertion_offsets();
    void read_bookmark_uses_the_bookinfo_total_and_highest_spine_index();
    void read_bookmark_needs_book_info();
};

void LibraryModelTest::recent_sort_keeps_the_newest_first_and_breaks_ties_by_title() {
    const std::vector<LibraryBook> input{
        library_book("a", "Mango", 300.0),
        library_book("b", "Beta", 100.0),
        library_book("c", "Gamma", 200.0),
        // Date.distantPast, the timestamp of a book that was never opened.
        library_book("d", "Delta", -63114076800.0),
        library_book("e", "Apple", 300.0),
        // A NaN date makes Swift's comparison unordered; here it is defined to
        // sort after every real date.
        library_book("n", "Nan", std::numeric_limits<double>::quiet_NaN()),
    };
    const std::vector<std::string> manual_order;

    const std::vector<LibraryBook> sorted = sort_books(input, SortOption::recent, manual_order);

    const std::vector<std::string> expected{"e", "a", "c", "b", "d", "n"};
    QCOMPARE(ids_of(sorted), expected);
}

void LibraryModelTest::title_sort_uses_localized_numeric_display_titles() {
    std::vector<LibraryBook> input{
        library_book("b", "Banana"),
        library_book("a", "Zebra"),
        library_book("v10", "Vol 10"),
        library_book("v2", "Vol 2"),
        library_book("zz", "Same"),
        library_book("bb", "Same"),
    };
    input[1].metadata.renamed_title = "Äpple";
    const QLocale original;
    QLocale::setDefault(QLocale(QLocale::English));
    const auto english_ids = ids_of(sort_books(input, SortOption::title, {}));
    QLocale::setDefault(QLocale::c());
    const auto fallback_ids = ids_of(sort_books(input, SortOption::title, {}));
    QLocale::setDefault(original);

    const std::vector<std::string> expected{"a", "b", "bb", "zz", "v2", "v10"};
    QCOMPARE(english_ids, expected);
    QCOMPARE(fallback_ids, expected);
}

void LibraryModelTest::manual_sort_keeps_known_ids_first_and_breaks_unknown_ties_by_id() {
    const std::vector<LibraryBook> input{
        library_book("x2", "Same"),
        library_book("x1", "Same"),
        library_book("known2", "Alpha"),
        library_book("known", "Beta"),
    };
    // A repeated ID keeps its first position, which places known2 before known.
    const std::vector<std::string> manual_order{"known2", "known", "known2"};

    const std::vector<LibraryBook> sorted = sort_books(input, SortOption::manual, manual_order);

    const std::vector<std::string> expected{"known2", "known", "x1", "x2"};
    QCOMPARE(ids_of(sorted), expected);
}

void LibraryModelTest::normalized_order_drops_unknown_and_duplicate_ids() {
    const std::vector<std::string> order{"c", "zz", "a", "c"};
    const std::vector<std::string> valid_ids{"a", "b", "c", "d"};
    // The fallback order decides where the appended IDs land.
    const std::vector<std::string> fallback_ids{"d", "b", "a"};

    const std::vector<std::string> normalized = normalized_order(order, valid_ids, fallback_ids);

    const std::vector<std::string> expected{"c", "a", "d", "b"};
    QCOMPARE(normalized, expected);
}

void LibraryModelTest::normalized_shelves_drop_unknown_and_duplicate_ids() {
    const std::vector<LibraryBook> library_books{
        library_book("a", "A"),
        library_book("b", "B"),
        library_book("c", "C"),
    };
    const std::vector<BookShelf> shelves{
        BookShelf{"First", {"c", "gone", "b", "c"}},
        BookShelf{"Empty", {}},
        BookShelf{"Third", {"a", "b"}},
    };

    const std::vector<BookShelf> normalized = normalized_shelves(shelves, library_books);

    // Order is kept, repeats and unknown IDs go, and no shelf gains a book.
    const std::vector<BookShelf> expected{
        BookShelf{"First", {"c", "b"}},
        BookShelf{"Empty", {}},
        BookShelf{"Third", {"a", "b"}},
    };
    QCOMPARE(normalized, expected);
}

void LibraryModelTest::normalized_shelf_name_trims_and_rejects_blank() {
    const auto padded = normalized_shelf_name("  Favourites \t\n");
    QVERIFY(padded.has_value());
    const std::string trimmed = "Favourites";
    QCOMPARE(*padded, trimmed);

    // U+3000 IDEOGRAPHIC SPACE and U+00A0 NO-BREAK SPACE are whitespace too.
    const auto ideographic = normalized_shelf_name("\xE3\x80\x80Read\xC2\xA0");
    QVERIFY(ideographic.has_value());
    const std::string read = "Read";
    QCOMPARE(*ideographic, read);

    const auto inner = normalized_shelf_name("Read  Later");
    QVERIFY(inner.has_value());
    const std::string inner_space = "Read  Later";
    QCOMPARE(*inner, inner_space);

    QVERIFY(!normalized_shelf_name("").has_value());
    QVERIFY(!normalized_shelf_name(" \t\r\n").has_value());
    QVERIFY(!normalized_shelf_name("\xC2\xA0").has_value());
}

void LibraryModelTest::shelf_name_availability_ignores_case() {
    const std::vector<std::string> names{"Favourites", "Read Later"};
    const std::string_view being_renamed = "Favourites";

    QVERIFY(!shelf_name_available("favourites", names));
    QVERIFY(!shelf_name_available("READ LATER", names));
    QVERIFY(shelf_name_available("Later", names));
    QVERIFY(shelf_name_available("Favourites", names, being_renamed));
    QVERIFY(shelf_name_available("Favourites ", names));

    // U+00C5 and "A" + U+030A are one name to Swift's String equality. Shelf
    // keys compare bytewise here, so they stay two names.
    const std::string composed = "\xC3\x85";
    const std::vector<std::string> canonical_names{composed};
    QVERIFY(shelf_name_available("A\xCC\x8A", canonical_names));
    QVERIFY(!shelf_name_available(composed, canonical_names));
}

void LibraryModelTest::unique_shelf_name_counts_from_two() {
    const std::string base = "New Shelf";
    const std::vector<std::string> none;
    const std::string first = unique_shelf_name(base, none);
    QCOMPARE(first, base);

    const std::vector<std::string> taken_in_other_case{"new shelf"};
    const std::string second = unique_shelf_name(base, taken_in_other_case);
    const std::string second_expected = "New Shelf 2";
    QCOMPARE(second, second_expected);

    const std::vector<std::string> taken_twice{"New Shelf", "New Shelf 2"};
    const std::string third = unique_shelf_name(base, taken_twice);
    const std::string third_expected = "New Shelf 3";
    QCOMPARE(third, third_expected);

    const std::string numbered_base = "New Shelf 2";
    const std::vector<std::string> numbered_taken{"New Shelf 2"};
    const std::string numbered = unique_shelf_name(numbered_base, numbered_taken);
    const std::string numbered_expected = "New Shelf 2 2";
    QCOMPARE(numbered, numbered_expected);
}

void LibraryModelTest::reading_books_are_between_zero_and_the_done_threshold() {
    std::vector<LibraryBook> input{
        library_book("zero", "Zero"),
        library_book("low", "Low"),
        library_book("half", "Half"),
        library_book("near", "Near"),
        library_book("edge", "Edge"),
        library_book("done", "Done"),
    };
    set_progress(input[0], 0, 1000);
    set_progress(input[1], 1, 1000);
    set_progress(input[2], 500, 1000);
    set_progress(input[3], 998, 1000);
    set_progress(input[4], 999, 1000);
    set_progress(input[5], 1000, 1000);

    const std::vector<LibraryBook> reading = reading_books(input);

    const std::vector<std::string> expected{"low", "half", "near"};
    QCOMPARE(ids_of(reading), expected);
}

void LibraryModelTest::progress_uses_the_bookmark_over_the_bookinfo_total() {
    LibraryBook book = library_book("a", "A");
    set_progress(book, 250, 1000);
    // The metadata count loses to a bookinfo total.
    book.metadata.character_count = 500;

    QCOMPARE(book_progress(book), 0.25);
}

void LibraryModelTest::progress_falls_back_to_the_metadata_character_count() {
    LibraryBook book = library_book("a", "A");
    Bookmark bookmark;
    bookmark.progress = 1.0;
    bookmark.character_count = 250;
    book.bookmark = bookmark;
    book.metadata.character_count = 500;

    QCOMPARE(book_progress(book), 0.5);
}

void LibraryModelTest::progress_is_zero_without_a_positive_total_or_a_bookmark() {
    // A complete pair still divides, so the zero cases below are about the
    // missing or unusable part, not about a model that never reads anything.
    LibraryBook complete = library_book("z", "Z");
    set_progress(complete, 250, 1000);
    QCOMPARE(book_progress(complete), 0.25);

    LibraryBook without_bookmark = library_book("a", "A");
    BookInfo info;
    info.character_count = 1000;
    without_bookmark.book_info = info;
    QCOMPARE(book_progress(without_bookmark), 0.0);

    LibraryBook zero_total = library_book("b", "B");
    set_progress(zero_total, 250, 0);
    QCOMPARE(book_progress(zero_total), 0.0);

    // A bookinfo total of zero is still the total; metadata does not stand in.
    LibraryBook zero_info = library_book("c", "C");
    set_progress(zero_info, 250, 0);
    zero_info.metadata.character_count = 500;
    QCOMPARE(book_progress(zero_info), 0.0);

    LibraryBook without_total = library_book("d", "D");
    Bookmark bookmark;
    bookmark.progress = 1.0;
    bookmark.character_count = 10;
    without_total.bookmark = bookmark;
    QCOMPARE(book_progress(without_total), 0.0);
}

void LibraryModelTest::progress_clamps_out_of_range_character_counts() {
    LibraryBook above = library_book("a", "A");
    set_progress(above, 1500, 1000);
    QCOMPARE(book_progress(above), 1.0);

    LibraryBook below = library_book("b", "B");
    set_progress(below, -100, 1000);
    QCOMPARE(book_progress(below), 0.0);
}

void LibraryModelTest::section_books_follow_the_selection() {
    const Library library = progress_library();
    const LibraryShelfSelection all = selection_of(LibraryShelf::all);
    const LibraryShelfSelection reading = selection_of(LibraryShelf::reading);
    const LibraryShelfSelection unshelved = selection_of(LibraryShelf::unshelved);
    const LibraryShelfSelection first = selection_of(LibraryShelf::shelf, "First");
    const LibraryShelfSelection second = selection_of(LibraryShelf::shelf, "Second");
    const LibraryShelfSelection gone = selection_of(LibraryShelf::shelf, "Gone");

    const std::vector<std::string> expected_all{"a", "b", "d", "c"};
    QCOMPARE(ids_of(section_books(library, all, SortOption::title)), expected_all);

    const std::vector<std::string> expected_reading{"a", "d"};
    QCOMPARE(ids_of(section_books(library, reading, SortOption::title)), expected_reading);

    const std::vector<std::string> expected_unshelved{"b", "d"};
    QCOMPARE(ids_of(section_books(library, unshelved, SortOption::title)), expected_unshelved);

    // unshelved_books keeps library order; a section is sorted afterwards.
    QCOMPARE(ids_of(unshelved_books(library)), expected_unshelved);

    const std::vector<std::string> expected_first{"a"};
    QCOMPARE(ids_of(section_books(library, first, SortOption::title)), expected_first);

    const std::vector<std::string> expected_second{"c"};
    QCOMPARE(ids_of(section_books(library, second, SortOption::title)), expected_second);

    // A selection whose shelf disappeared shows the whole library.
    QCOMPARE(ids_of(section_books(library, gone, SortOption::title)), expected_all);
}

void LibraryModelTest::section_books_sort_a_shelf_by_its_own_order() {
    Library library;
    library.books = {
        library_book("a", "Alpha"),
        library_book("b", "Beta"),
        library_book("c", "Gamma"),
    };
    library.shelves = {BookShelf{"Late", {"c", "a"}}};
    library.manual_order = {"b", "a", "c"};

    const LibraryShelfSelection late = selection_of(LibraryShelf::shelf, "Late");
    const std::vector<std::string> expected_shelf{"c", "a"};
    QCOMPARE(ids_of(section_books(library, late, SortOption::manual)), expected_shelf);

    const LibraryShelfSelection all = selection_of(LibraryShelf::all);
    const std::vector<std::string> expected_all{"b", "a", "c"};
    QCOMPARE(ids_of(section_books(library, all, SortOption::manual)), expected_all);

    const std::vector<std::string> expected_title{"a", "c"};
    QCOMPARE(ids_of(section_books(library, late, SortOption::title)), expected_title);
}

void LibraryModelTest::book_count_counts_the_selection() {
    Library library = progress_library();
    const LibraryShelfSelection all = selection_of(LibraryShelf::all);
    const LibraryShelfSelection reading = selection_of(LibraryShelf::reading);
    const LibraryShelfSelection unshelved = selection_of(LibraryShelf::unshelved);
    const LibraryShelfSelection first = selection_of(LibraryShelf::shelf, "First");
    const LibraryShelfSelection gone = selection_of(LibraryShelf::shelf, "Gone");

    const int expected_all = 4;
    QCOMPARE(book_count(library, all), expected_all);
    const int expected_reading = 2;
    QCOMPARE(book_count(library, reading), expected_reading);
    const int expected_unshelved = 2;
    QCOMPARE(book_count(library, unshelved), expected_unshelved);
    const int expected_first = 1;
    QCOMPARE(book_count(library, first), expected_first);
    const int expected_gone = 0;
    QCOMPARE(book_count(library, gone), expected_gone);

    // A stored repeat counts per occurrence until normalize_shelves runs.
    library.shelves = {BookShelf{"Twice", {"a", "a"}}};
    const LibraryShelfSelection twice = selection_of(LibraryShelf::shelf, "Twice");
    const int expected_twice = 2;
    QCOMPARE(book_count(library, twice), expected_twice);
}

void LibraryModelTest::resolved_selection_falls_back_to_all() {
    Library library;
    library.books = {library_book("a", "A")};
    library.shelves = {BookShelf{"First", {"a"}}};

    const LibraryShelfSelection existing = selection_of(LibraryShelf::shelf, "First");
    const LibraryShelfSelection kept = resolved_selection(library, existing);
    QVERIFY(kept == existing);

    const LibraryShelfSelection missing = selection_of(LibraryShelf::shelf, "Gone");
    const LibraryShelfSelection resolved = resolved_selection(library, missing);
    const LibraryShelfSelection expected = selection_of(LibraryShelf::all);
    QCOMPARE(resolved, expected);
    QVERIFY(resolved.name.empty());
}

void LibraryModelTest::duplicate_shelf_names_use_the_first_match() {
    Library library;
    library.books = {
        library_book("a", "Alpha"),
        library_book("b", "Beta"),
        library_book("c", "Gamma"),
    };
    library.shelves = {BookShelf{"Same", {"a", "b"}}, BookShelf{"Same", {"b", "c"}}};

    const std::size_t expected_shelves = 2;
    QCOMPARE(library.shelves.size(), expected_shelves);

    // Every lookup takes the first shelf with that name.
    const LibraryShelfSelection same = selection_of(LibraryShelf::shelf, "Same");
    const std::vector<std::string> expected_section{"a", "b"};
    QCOMPARE(ids_of(section_books(library, same, SortOption::title)), expected_section);

    const int expected_count = 2;
    QCOMPARE(book_count(library, same), expected_count);

    move_book_before(library, same, SortOption::manual, "b", "a");
    const std::vector<std::string> expected_first{"b", "a"};
    QCOMPARE(library.shelves[0].book_ids, expected_first);
    const std::vector<std::string> expected_second{"b", "c"};
    QCOMPARE(library.shelves[1].book_ids, expected_second);

    const std::string same_name = "Same";
    move_book_to_shelf(library.shelves, "c", same_name);
    const std::vector<std::string> expected_first_with_c{"b", "a", "c"};
    QCOMPARE(library.shelves[0].book_ids, expected_first_with_c);
    const std::vector<std::string> expected_second_without_c{"b"};
    QCOMPARE(library.shelves[1].book_ids, expected_second_without_c);
}

void LibraryModelTest::shelf_sections_put_reading_first_and_unshelved_last() {
    const Library library = progress_library();
    const std::vector<LibrarySection> sections =
        shelf_sections(library, SortOption::title, true);

    const std::size_t expected_count = 4;
    QCOMPARE(sections.size(), expected_count);

    QVERIFY(sections[0].shelf == LibraryShelf::reading);
    QVERIFY(sections[0].name.empty());
    const std::vector<std::string> expected_reading{"a", "d"};
    QCOMPARE(ids_of(sections[0].books), expected_reading);

    QVERIFY(sections[1].shelf == LibraryShelf::shelf);
    const std::string first_name = "First";
    QCOMPARE(sections[1].name, first_name);
    const std::vector<std::string> expected_first{"a"};
    QCOMPARE(ids_of(sections[1].books), expected_first);

    const std::string second_name = "Second";
    QCOMPARE(sections[2].name, second_name);
    const std::vector<std::string> expected_second{"c"};
    QCOMPARE(ids_of(sections[2].books), expected_second);

    QVERIFY(sections[3].shelf == LibraryShelf::unshelved);
    const std::vector<std::string> expected_unshelved{"b", "d"};
    QCOMPARE(ids_of(sections[3].books), expected_unshelved);

    const std::vector<LibrarySection> without_reading =
        shelf_sections(library, SortOption::title);
    const std::size_t expected_without_reading = 3;
    QCOMPARE(without_reading.size(), expected_without_reading);
    QCOMPARE(without_reading[0].name, first_name);
}

void LibraryModelTest::move_book_to_shelf_removes_from_every_shelf_then_appends() {
    std::vector<BookShelf> shelves{
        BookShelf{"First", {"a", "b"}},
        BookShelf{"Second", {"b", "c"}},
    };
    const std::string second = "Second";

    move_book_to_shelf(shelves, "b", second);
    const std::vector<BookShelf> expected_moved{
        BookShelf{"First", {"a"}},
        BookShelf{"Second", {"c", "b"}},
    };
    QCOMPARE(shelves, expected_moved);

    move_book_to_shelf(shelves, "a", std::nullopt);
    const std::vector<BookShelf> expected_removed{
        BookShelf{"First", {}},
        BookShelf{"Second", {"c", "b"}},
    };
    QCOMPARE(shelves, expected_removed);

    // An unknown shelf name only takes the book off its shelves.
    const std::string gone = "Gone";
    move_book_to_shelf(shelves, "c", gone);
    const std::vector<BookShelf> expected_unknown{
        BookShelf{"First", {}},
        BookShelf{"Second", {"b"}},
    };
    QCOMPARE(shelves, expected_unknown);

    // The id view points into the shelf this call empties, so it outlives the
    // erase unless the id is copied first. UUID-length ids take the heap and
    // turn the stale read into a use-after-free rather than a wrong byte.
    const std::string first_id = "0F0F0F0F-1111-2222-3333-444444444444";
    const std::string second_id = "1F1F1F1F-1111-2222-3333-444444444444";
    std::vector<BookShelf> aliased_shelves{
        BookShelf{"First", {first_id, second_id}},
        BookShelf{"Second", {}},
    };
    const std::string_view aliased = aliased_shelves.front().book_ids.front();
    move_book_to_shelf(aliased_shelves, aliased, second);

    const std::size_t expected_aliased_shelves = 2;
    QCOMPARE(aliased_shelves.size(), expected_aliased_shelves);
    const std::vector<std::string> expected_source{second_id};
    QCOMPARE(aliased_shelves[0].book_ids, expected_source);
    const std::vector<std::string> expected_target{first_id};
    QCOMPARE(aliased_shelves[1].book_ids, expected_target);
}

void LibraryModelTest::reorder_ids_moves_down_and_up() {
    const std::vector<std::string> order{"a", "b", "c", "d"};
    const std::vector<std::string> no_fallback;

    const std::vector<std::string> down = reorder_ids(order, "b", "d", no_fallback);
    const std::vector<std::string> expected_down{"a", "c", "d", "b"};
    QCOMPARE(down, expected_down);

    const std::vector<std::string> up = reorder_ids(order, "d", "a", no_fallback);
    const std::vector<std::string> expected_up{"d", "a", "b", "c"};
    QCOMPARE(up, expected_up);

    // A drop on itself and an ID outside the list leave the order alone.
    const std::vector<std::string> same = reorder_ids(order, "b", "b", no_fallback);
    QCOMPARE(same, order);
    const std::vector<std::string> unknown = reorder_ids(order, "zz", "a", no_fallback);
    QCOMPARE(unknown, order);
}

void LibraryModelTest::reorder_ids_appends_missing_fallback_ids() {
    const std::vector<std::string> order{"a", "b"};
    const std::vector<std::string> fallback{"c", "b", "a"};

    // The appended ID is in the list even when the drop moves nothing.
    const std::vector<std::string> appended = reorder_ids(order, "a", "a", fallback);
    const std::vector<std::string> expected_appended{"a", "b", "c"};
    QCOMPARE(appended, expected_appended);

    const std::vector<std::string> moved = reorder_ids(order, "c", "a", fallback);
    const std::vector<std::string> expected_moved{"c", "a", "b"};
    QCOMPARE(moved, expected_moved);
}

void LibraryModelTest::move_book_before_reorders_a_shelf_list() {
    Library library;
    library.books = {
        library_book("a", "Alpha"),
        library_book("b", "Beta"),
        library_book("c", "Gamma"),
    };
    library.shelves = {BookShelf{"First", {"a", "b", "c"}}};
    library.manual_order = {"b", "a", "c"};

    const LibraryShelfSelection first = selection_of(LibraryShelf::shelf, "First");
    move_book_before(library, first, SortOption::manual, "c", "a");

    QVERIFY(!library.shelves.empty());
    const std::vector<std::string> expected_shelf{"c", "a", "b"};
    QCOMPARE(library.shelves.front().book_ids, expected_shelf);

    // A shelf drop leaves the global manual order untouched.
    const std::vector<std::string> expected_manual{"b", "a", "c"};
    QCOMPARE(library.manual_order, expected_manual);
}

void LibraryModelTest::move_book_before_reorders_only_the_manual_slice() {
    Library library;
    library.books = {
        library_book("a", "Alpha"),
        library_book("b", "Beta"),
        library_book("c", "Gamma"),
        library_book("d", "Delta"),
    };
    library.shelves = {BookShelf{"First", {"a", "b"}}};
    library.manual_order = {"d", "c", "b", "a"};

    // Unshelved holds c and d; the drop moves them to the end in their new
    // order while a and b keep their relative order ahead of them.
    const LibraryShelfSelection unshelved = selection_of(LibraryShelf::unshelved);
    move_book_before(library, unshelved, SortOption::manual, "d", "c");

    const std::vector<std::string> expected_manual{"b", "a", "c", "d"};
    QCOMPARE(library.manual_order, expected_manual);
    QVERIFY(!library.shelves.empty());
    const std::vector<std::string> expected_shelf{"a", "b"};
    QCOMPARE(library.shelves.front().book_ids, expected_shelf);

    // All books form the whole slice, so the drop rewrites the whole order.
    Library all_library;
    all_library.books = library.books;
    all_library.shelves = library.shelves;
    all_library.manual_order = {"d", "c", "b", "a"};
    const LibraryShelfSelection all = selection_of(LibraryShelf::all);
    move_book_before(all_library, all, SortOption::manual, "a", "d");

    const std::vector<std::string> expected_all{"a", "d", "c", "b"};
    QCOMPARE(all_library.manual_order, expected_all);
}

// BookshelfViewModel.moveShelves uses Array.move with an IndexSet and an
// insertion offset in the original shelf list.
void LibraryModelTest::move_shelves_uses_original_insertion_offsets() {
    const BookShelf a{"A", {"a"}};
    const BookShelf b{"B", {"b", "c"}};
    const BookShelf c{"C", {}};
    const BookShelf d{"D", {"d"}};
    std::vector<BookShelf> shelves{a, b, c, d};
    QVERIFY(niratan::books::move_shelves(shelves, {0, 2}, 4));
    const std::vector<BookShelf> at_end{b, d, a, c};
    QCOMPARE(shelves, at_end);
    QVERIFY(niratan::books::move_shelves(shelves, {1, 3}, 0));
    const std::vector<BookShelf> at_start{d, c, b, a};
    QCOMPARE(shelves, at_start);
}

void LibraryModelTest::move_book_before_ignores_the_reading_section() {
    Library library = progress_library();
    library.manual_order = {"a", "b", "c", "d"};

    const LibraryShelfSelection reading = selection_of(LibraryShelf::reading);
    move_book_before(library, reading, SortOption::manual, "d", "a");
    const std::vector<std::string> expected_unchanged{"a", "b", "c", "d"};
    QCOMPARE(library.manual_order, expected_unchanged);

    // The same drop in All is applied, so the rejection is the reading rule.
    const LibraryShelfSelection all = selection_of(LibraryShelf::all);
    move_book_before(library, all, SortOption::manual, "d", "a");
    const std::vector<std::string> expected_moved{"d", "a", "b", "c"};
    QCOMPARE(library.manual_order, expected_moved);
}

void LibraryModelTest::read_bookmark_uses_the_bookinfo_total_and_highest_spine_index() {
    BookInfo info;
    info.character_count = 5000;
    info.chapter_info["one"] = ChapterInfo{0, 0, 100, std::nullopt};
    info.chapter_info["two"] = ChapterInfo{2, 100, 100, std::nullopt};
    info.chapter_info["three"] = ChapterInfo{std::nullopt, 200, 100, std::nullopt};

    LibraryBook book = library_book("a", "A");
    book.book_info = info;

    const double last_modified = 123.5;
    const auto bookmark = read_bookmark(book, last_modified);

    QVERIFY(bookmark.has_value());
    const int expected_chapter = 2;
    QCOMPARE(bookmark->chapter_index, expected_chapter);
    QCOMPARE(bookmark->progress, 1.0);
    const int expected_characters = 5000;
    QCOMPARE(bookmark->character_count, expected_characters);
    QVERIFY(bookmark->last_modified.has_value());
    QCOMPARE(*bookmark->last_modified, last_modified);
}

void LibraryModelTest::read_bookmark_needs_book_info() {
    const LibraryBook without_info = library_book("a", "A");
    QVERIFY(!read_bookmark(without_info, 1.0).has_value());

    BookInfo info;
    info.character_count = 10;
    info.chapter_info["one"] = ChapterInfo{std::nullopt, 0, 100, std::nullopt};
    LibraryBook book = library_book("b", "B");
    book.book_info = info;

    const auto bookmark = read_bookmark(book, 2.0);
    QVERIFY(bookmark.has_value());
    const int expected_chapter = 0;
    QCOMPARE(bookmark->chapter_index, expected_chapter);
    const int expected_characters = 10;
    QCOMPARE(bookmark->character_count, expected_characters);
}

QTEST_APPLESS_MAIN(LibraryModelTest)
#include "tst_library_model.moc"
