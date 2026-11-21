// SPDX-License-Identifier: GPL-3.0-or-later
//
// The bookshelf model: ordering, shelves, selection and reading progress,
// following Features/Bookshelf/BookshelfViewModel.swift (sortBooks,
// shelfSection, shelfSections, bookCount, readingBooks, unshelvedBooks,
// loadBookProgress, loadManualBookOrder, normalizedOrder, normalizeShelves,
// moveBook(_:to:), moveBook(_:in:before:), markRead,
// resolvedShelfSelection), Features/Bookshelf/LibraryShelfSelection.swift
// (LibraryShelfNaming) and Models/Book.swift (SortOption, BookReorder,
// BookShelf, Bookmark, BookInfo).
//
// This is a pure model. A caller reads BookStorage into LibraryBook values and
// the whole state into Library, then asks for sorted lists and changes the
// state through the move helpers; nothing here opens a file and nothing here
// remembers a progress cache, so book_progress is recalculated from the values
// passed in.
//
// Identifiers are the UUID strings BookMetadata carries, compared bytewise.
// A shelf is keyed by its name, also compared bytewise, so two canonically
// equivalent spellings (NFC and NFD) are different shelves here where Swift's
// String equality would merge them. Only the uniqueness checks fold case.
//
// Display titles use Qt's locale collation with numeric ordering ("Vol 2"
// before "Vol 10"), following localizedStandardCompare. Qt and Foundation may
// order some spellings differently. The C locale uses English collation so
// numeric ordering remains available. Equal titles fall to the identifier.
//
// Progress divides the bookmark's character count by the book's total
// character count and is clamped to [0, 1]; Swift keeps the raw quotient.
// Only a bookmark outside the book differs, and both agree on whether a book
// is in progress. bookmark.progress itself is not read, exactly as Swift only
// reads the character counts. This is the bookmark and bookinfo branch of
// loadBookProgress; the synced sharedProgress override from SyncStorage is not
// modelled yet.
//
// Google Drive selections and their title-only manual sort are out of scope:
// this library has no remote books, so LibraryShelf has no remote case.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/books/book.h"

namespace niratan::books {

// One library book with the files loadBookProgress reads beside its metadata.
struct LibraryBook {
    BookMetadata metadata;
    // bookmark.json; nullopt when it is missing or could not be read.
    std::optional<Bookmark> bookmark;
    // bookinfo.json; nullopt when it is missing or could not be read.
    std::optional<BookInfo> book_info;

    bool operator==(const LibraryBook&) const = default;
};

// The whole shelf state: the books in load order, the shelves in display order
// with their stored book order, and the manual order of book IDs.
struct Library {
    std::vector<LibraryBook> books;
    std::vector<BookShelf> shelves;
    std::vector<std::string> manual_order;

    bool operator==(const Library&) const = default;
};

// A shelf column entry: the whole library, the reading list, the books on no
// shelf, or one user shelf named by its key.
enum class LibraryShelf { all, reading, unshelved, shelf };

struct LibraryShelfSelection {
    LibraryShelf shelf = LibraryShelf::all;
    // The shelf key for LibraryShelf::shelf; ignored for the other cases.
    std::string name;

    bool operator==(const LibraryShelfSelection&) const = default;
};

// One group of shelfSections, in display order.
struct LibrarySection {
    LibraryShelf shelf = LibraryShelf::all;
    // The shelf key for LibraryShelf::shelf, empty for the other cases.
    std::string name;
    std::vector<LibraryBook> books;

    bool operator==(const LibrarySection&) const = default;
};

// bookProgress[book.id] from the bookmark and bookinfo branch of
// loadBookProgress: the bookmark's character count over the bookinfo total, or
// over the metadata character count when the book has no bookinfo. Zero when
// the total is not positive or the bookmark is missing, clamped to [0, 1].
// The synced sharedProgress override is not modelled yet.
double book_progress(const LibraryBook& book);

// LibraryShelfNaming.normalized: trim whitespace and newlines, then reject a
// name that is empty. Whitespace inside the name is kept.
std::optional<std::string> normalized_shelf_name(std::string_view name);

// LibraryShelfNaming.isAvailable: no name matches case-insensitively, apart
// from the name equal to `excluding`, which is the shelf being renamed. Names
// are compared bytewise, so canonically equivalent spellings stay distinct. The
// name is not trimmed or validated here.
bool shelf_name_available(std::string_view name, const std::vector<std::string>& names,
                          std::optional<std::string_view> excluding = std::nullopt);

// LibraryShelfNaming.uniqueName: base when it is available, otherwise the
// first "base N" with N counting up from 2. base is used as given and is not
// trimmed.
std::string unique_shelf_name(std::string_view base, const std::vector<std::string>& names);

// sortBooks(_:by:manualOrder:). recent orders by last_access descending, with
// a NaN timestamp after every real one; title orders by display title; manual
// follows manual_order, keeping books it does not list after the ones it does,
// in title order. The first occurrence of an ID in manual_order wins. The
// input and manual_order are left alone.
std::vector<LibraryBook> sort_books(const std::vector<LibraryBook>& books, SortOption option,
                                    const std::vector<std::string>& manual_order);

// normalizedOrder: the IDs of order that are in valid_ids, without repeats and
// in order, then the IDs of fallback_ids that are in valid_ids and still
// missing, in fallback order. Manual order passes the library's IDs as
// fallback; a shelf passes its own stored list.
std::vector<std::string> normalized_order(const std::vector<std::string>& order,
                                          const std::vector<std::string>& valid_ids,
                                          const std::vector<std::string>& fallback_ids);

// normalizeShelves: every shelf is normalized against the library's IDs with
// its own list as fallback. Unknown and repeated IDs go, stored order stays,
// and nothing is appended: a shelf never gains a book, and a book missing from
// every shelf stays unshelved. Shelf order and names are kept.
std::vector<BookShelf> normalized_shelves(const std::vector<BookShelf>& shelves,
                                          const std::vector<LibraryBook>& books);

// The list moveBook(_:in:before:) works on: append the IDs of fallback_ids the
// order lacks, then move source_id in front of target_id. A drop on the same
// book, or an ID that is in neither list, only leaves the appended order.
std::vector<std::string> reorder_ids(std::vector<std::string> order,
                                     std::string_view source_id, std::string_view target_id,
                                     const std::vector<std::string>& fallback_ids);

// moveBook(_:to:): take the ID out of every shelf, then append it to the first
// shelf whose name equals shelf_name. A name no shelf has only removes it; an
// empty name matches a shelf stored with an empty name.
void move_book_to_shelf(std::vector<BookShelf>& shelves, std::string_view book_id,
                        const std::optional<std::string>& shelf_name);

// readingBooks: books with 0 < progress < 0.999, in library order.
std::vector<LibraryBook> reading_books(const std::vector<LibraryBook>& books);

// unshelvedBooks: books whose ID is on no shelf, in library order.
std::vector<LibraryBook> unshelved_books(const Library& library);

// shelfSection(for:sortedBy:). A user shelf that does not exist falls back to
// the whole library, as Swift does when a shelf disappears. A user shelf sorts
// manual order by its own stored list; the other cases use library.manual_order.
std::vector<LibraryBook> section_books(const Library& library,
                                       const LibraryShelfSelection& selection,
                                       SortOption option);

// shelfSections(sortedBy:showReading:), without the Google Drive section:
// reading first when show_reading is set and it has books, then each shelf in
// stored order, then unshelved, which is always present even when empty.
std::vector<LibrarySection> shelf_sections(const Library& library, SortOption option,
                                           bool show_reading = false);

// bookCount(for:). An unknown shelf counts nothing. A repeated ID on one shelf
// counts once per occurrence, as Swift's filter does; normalize_shelves is what
// removes the repeats.
int book_count(const Library& library, const LibraryShelfSelection& selection);

// resolvedShelfSelection: a user shelf that no longer exists resolves to all.
LibraryShelfSelection resolved_selection(const Library& library,
                                         const LibraryShelfSelection& selection);

// moveBook(_:in:before:). A user shelf rewrites only that shelf's stored order,
// with the books displayed in the section, sorted by option, as the fallback.
// All and unshelved rewrite manual_order: the section's IDs keep their manual
// order, the drop moves one of them, and the section is then placed at the end
// of manual_order while the other books keep their relative order. The reading
// list ignores drops.
void move_book_before(Library& library, const LibraryShelfSelection& selection, SortOption option,
                      std::string_view source_id, std::string_view target_id);

// markRead(book:). A bookmark that finishes the book: progress 1, the bookinfo
// character total, the highest spine index the bookinfo names (0 when it names
// none), and last_modified in reference seconds. Empty when the book has no
// bookinfo, which is when Swift returns without writing.
std::optional<Bookmark> read_bookmark(const LibraryBook& book, double last_modified);

}  // namespace niratan::books
