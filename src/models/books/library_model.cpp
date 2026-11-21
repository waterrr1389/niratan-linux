// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/books/library_model.h"

#include <QCollator>
#include <QLocale>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace niratan::books {
namespace {

QString qstring(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

bool same_id(const std::string& id, std::string_view value) {
    return std::string_view(id) == value;
}

bool title_before(const LibraryBook& left, const LibraryBook& right, const QCollator& collator) {
    const int order = collator.compare(QString::fromStdString(left.metadata.display_title()),
                                       QString::fromStdString(right.metadata.display_title()));
    if (order != 0) {
        return order < 0;
    }
    return left.metadata.id < right.metadata.id;
}

// Newest first. A NaN timestamp sorts after every real one instead of making
// the comparison unordered.
bool recent_before(const LibraryBook& left, const LibraryBook& right, const QCollator& collator) {
    const double left_time = left.metadata.last_access;
    const double right_time = right.metadata.last_access;
    const bool left_nan = std::isnan(left_time);
    const bool right_nan = std::isnan(right_time);
    if (left_nan != right_nan) {
        return !left_nan;
    }
    if (!left_nan && left_time != right_time) {
        return left_time > right_time;
    }
    return title_before(left, right, collator);
}

std::vector<std::string> book_ids(const std::vector<LibraryBook>& books) {
    std::vector<std::string> ids;
    ids.reserve(books.size());
    for (const LibraryBook& book : books) {
        ids.push_back(book.metadata.id);
    }
    return ids;
}

// Keeps library order and takes each book at most once, matching Swift's
// books.filter { shelf.bookIds.contains($0.id) }.
std::vector<LibraryBook> books_with_ids(const std::vector<LibraryBook>& books,
                                        const std::vector<std::string>& ids) {
    const std::unordered_set<std::string> wanted(ids.begin(), ids.end());
    std::vector<LibraryBook> selected;
    for (const LibraryBook& book : books) {
        if (wanted.contains(book.metadata.id)) {
            selected.push_back(book);
        }
    }
    return selected;
}

std::unordered_set<std::string> shelved_ids(const std::vector<BookShelf>& shelves) {
    std::unordered_set<std::string> ids;
    for (const BookShelf& shelf : shelves) {
        ids.insert(shelf.book_ids.begin(), shelf.book_ids.end());
    }
    return ids;
}

}  // namespace

double book_progress(const LibraryBook& book) {
    if (!book.bookmark) {
        return 0.0;
    }

    int total = 0;
    if (book.book_info) {
        total = book.book_info->character_count;
    } else if (book.metadata.character_count) {
        total = *book.metadata.character_count;
    }
    if (total <= 0) {
        return 0.0;
    }

    const double progress =
        static_cast<double>(book.bookmark->character_count) / static_cast<double>(total);
    return std::clamp(progress, 0.0, 1.0);
}

std::optional<std::string> normalized_shelf_name(std::string_view name) {
    const QString trimmed = qstring(name).trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }
    return trimmed.toStdString();
}

bool shelf_name_available(std::string_view name, const std::vector<std::string>& names,
                          std::optional<std::string_view> excluding) {
    for (const std::string& existing : names) {
        if (excluding && same_id(existing, *excluding)) {
            continue;
        }
        if (QString::compare(qstring(existing), qstring(name), Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return true;
}

std::string unique_shelf_name(std::string_view base, const std::vector<std::string>& names) {
    if (shelf_name_available(base, names)) {
        return std::string(base);
    }

    int suffix = 2;
    while (true) {
        std::string candidate = std::string(base) + " " + std::to_string(suffix);
        if (shelf_name_available(candidate, names)) {
            return candidate;
        }
        ++suffix;
    }
}

std::vector<LibraryBook> sort_books(const std::vector<LibraryBook>& books, SortOption option,
                                     const std::vector<std::string>& manual_order) {
    const QLocale locale = QLocale().collation();
    // Qt disables numeric collation in the C locale.
    QCollator collator(locale == QLocale::c() ? QLocale(QLocale::English) : locale);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::vector<LibraryBook> sorted = books;
    switch (option) {
        case SortOption::recent:
            std::stable_sort(sorted.begin(), sorted.end(), [&collator](const auto& left, const auto& right) {
                return recent_before(left, right, collator);
            });
            break;
        case SortOption::title:
            std::stable_sort(sorted.begin(), sorted.end(), [&collator](const auto& left, const auto& right) {
                return title_before(left, right, collator);
            });
            break;
        case SortOption::manual: {
            // The first occurrence of an identifier fixes its position, as
            // Swift's enumerated dictionary cannot keep a later one.
            std::unordered_map<std::string, std::size_t> positions;
            positions.reserve(manual_order.size());
            for (std::size_t index = 0; index < manual_order.size(); ++index) {
                positions.emplace(manual_order[index], index);
            }
            std::stable_sort(
                sorted.begin(), sorted.end(),
                [&positions, &collator](const LibraryBook& left, const LibraryBook& right) {
                    const auto left_position = positions.find(left.metadata.id);
                    const auto right_position = positions.find(right.metadata.id);
                    const bool left_known = left_position != positions.end();
                    const bool right_known = right_position != positions.end();
                    if (left_known != right_known) {
                        return left_known;
                    }
                    if (left_known) {
                        return left_position->second < right_position->second;
                    }
                    return title_before(left, right, collator);
                });
            break;
        }
    }
    return sorted;
}

std::vector<std::string> normalized_order(const std::vector<std::string>& order,
                                          const std::vector<std::string>& valid_ids,
                                          const std::vector<std::string>& fallback_ids) {
    const std::unordered_set<std::string> valid(valid_ids.begin(), valid_ids.end());
    std::unordered_set<std::string> seen;
    std::vector<std::string> normalized;
    normalized.reserve(order.size() + fallback_ids.size());

    for (const std::string& id : order) {
        if (valid.contains(id) && seen.insert(id).second) {
            normalized.push_back(id);
        }
    }
    for (const std::string& id : fallback_ids) {
        if (valid.contains(id) && seen.insert(id).second) {
            normalized.push_back(id);
        }
    }
    return normalized;
}

std::vector<BookShelf> normalized_shelves(const std::vector<BookShelf>& shelves,
                                          const std::vector<LibraryBook>& books) {
    const std::vector<std::string> valid_ids = book_ids(books);

    std::vector<BookShelf> normalized;
    normalized.reserve(shelves.size());
    for (const BookShelf& shelf : shelves) {
        // The shelf's own list is its fallback, so normalization can only drop
        // entries and never shelve a book that is unshelved.
        normalized.push_back(BookShelf{
            shelf.name, normalized_order(shelf.book_ids, valid_ids, shelf.book_ids)});
    }
    return normalized;
}

std::vector<std::string> reorder_ids(std::vector<std::string> order, std::string_view source_id,
                                     std::string_view target_id,
                                     const std::vector<std::string>& fallback_ids) {
    std::unordered_set<std::string> seen(order.begin(), order.end());
    for (const std::string& id : fallback_ids) {
        if (seen.insert(id).second) {
            order.push_back(id);
        }
    }

    const auto source = std::find_if(order.begin(), order.end(),
                                     [source_id](const std::string& id) {
                                         return same_id(id, source_id);
                                     });
    const auto target = std::find_if(order.begin(), order.end(),
                                     [target_id](const std::string& id) {
                                         return same_id(id, target_id);
                                     });
    if (source == order.end() || target == order.end()) {
        return order;
    }

    const auto source_index = static_cast<std::size_t>(std::distance(order.begin(), source));
    const auto target_index = static_cast<std::size_t>(std::distance(order.begin(), target));
    const std::optional<int> destination =
        reorder_destination(static_cast<int>(source_index), static_cast<int>(target_index));
    if (!destination) {
        return order;
    }

    const std::string moved = std::move(order[source_index]);
    order.erase(order.begin() + static_cast<std::ptrdiff_t>(source_index));
    // destinationOffset counts positions before the removal, so a downward move
    // lands one place earlier once the element is gone.
    const std::size_t insert_index = static_cast<std::size_t>(*destination) >
                                             source_index
                                         ? static_cast<std::size_t>(*destination) - 1
                                         : static_cast<std::size_t>(*destination);
    order.insert(order.begin() + static_cast<std::ptrdiff_t>(insert_index), moved);
    return order;
}

void move_book_to_shelf(std::vector<BookShelf>& shelves, std::string_view book_id,
                        const std::optional<std::string>& shelf_name) {
    // Copy first: book_id may point into a shelf list that the erase below
    // modifies.
    const std::string id(book_id);
    for (BookShelf& shelf : shelves) {
        std::erase_if(shelf.book_ids, [&id](const std::string& candidate) {
            return same_id(candidate, id);
        });
    }
    if (!shelf_name) {
        return;
    }

    const auto destination = std::find_if(shelves.begin(), shelves.end(),
                                          [&shelf_name](const BookShelf& shelf) {
                                              return shelf.name == *shelf_name;
                                          });
    if (destination != shelves.end()) {
        destination->book_ids.push_back(id);
    }
}

std::vector<LibraryBook> reading_books(const std::vector<LibraryBook>& books) {
    std::vector<LibraryBook> reading;
    for (const LibraryBook& book : books) {
        if (is_in_progress(book_progress(book))) {
            reading.push_back(book);
        }
    }
    return reading;
}

std::vector<LibraryBook> unshelved_books(const Library& library) {
    const std::unordered_set<std::string> ids = shelved_ids(library.shelves);
    std::vector<LibraryBook> unshelved;
    for (const LibraryBook& book : library.books) {
        if (!ids.contains(book.metadata.id)) {
            unshelved.push_back(book);
        }
    }
    return unshelved;
}

std::vector<LibraryBook> section_books(const Library& library,
                                       const LibraryShelfSelection& selection,
                                       SortOption option) {
    switch (selection.shelf) {
        case LibraryShelf::all:
            return sort_books(library.books, option, library.manual_order);
        case LibraryShelf::reading:
            return sort_books(reading_books(library.books), option, library.manual_order);
        case LibraryShelf::unshelved:
            return sort_books(unshelved_books(library), option, library.manual_order);
        case LibraryShelf::shelf: {
            const auto shelf = std::find_if(library.shelves.begin(), library.shelves.end(),
                                            [&selection](const BookShelf& candidate) {
                                                return candidate.name == selection.name;
                                            });
            if (shelf == library.shelves.end()) {
                return sort_books(library.books, option, library.manual_order);
            }
            return sort_books(books_with_ids(library.books, shelf->book_ids), option,
                              shelf->book_ids);
        }
    }
    return {};
}

std::vector<LibrarySection> shelf_sections(const Library& library, SortOption option,
                                           bool show_reading) {
    std::vector<LibrarySection> sections;

    if (show_reading) {
        std::vector<LibraryBook> reading = reading_books(library.books);
        if (!reading.empty()) {
            LibrarySection section;
            section.shelf = LibraryShelf::reading;
            section.books = sort_books(reading, option, library.manual_order);
            sections.push_back(std::move(section));
        }
    }

    for (const BookShelf& shelf : library.shelves) {
        LibrarySection section;
        section.shelf = LibraryShelf::shelf;
        section.name = shelf.name;
        section.books =
            sort_books(books_with_ids(library.books, shelf.book_ids), option, shelf.book_ids);
        sections.push_back(std::move(section));
    }

    LibrarySection unshelved;
    unshelved.shelf = LibraryShelf::unshelved;
    unshelved.books = sort_books(unshelved_books(library), option, library.manual_order);
    sections.push_back(std::move(unshelved));

    return sections;
}

int book_count(const Library& library, const LibraryShelfSelection& selection) {
    switch (selection.shelf) {
        case LibraryShelf::all:
            return static_cast<int>(library.books.size());
        case LibraryShelf::reading:
            return static_cast<int>(reading_books(library.books).size());
        case LibraryShelf::unshelved:
            return static_cast<int>(unshelved_books(library).size());
        case LibraryShelf::shelf: {
            const auto shelf = std::find_if(library.shelves.begin(), library.shelves.end(),
                                            [&selection](const BookShelf& candidate) {
                                                return candidate.name == selection.name;
                                            });
            if (shelf == library.shelves.end()) {
                return 0;
            }
            // Swift counts stored entries, so a repeated identifier counts
            // again until normalize_shelves removes it.
            const std::vector<std::string> ids = book_ids(library.books);
            const std::unordered_set<std::string> valid(ids.begin(), ids.end());
            int count = 0;
            for (const std::string& id : shelf->book_ids) {
                if (valid.contains(id)) {
                    ++count;
                }
            }
            return count;
        }
    }
    return 0;
}

LibraryShelfSelection resolved_selection(const Library& library,
                                         const LibraryShelfSelection& selection) {
    if (selection.shelf != LibraryShelf::shelf) {
        return selection;
    }
    const auto shelf = std::find_if(library.shelves.begin(), library.shelves.end(),
                                    [&selection](const BookShelf& candidate) {
                                        return candidate.name == selection.name;
                                    });
    if (shelf == library.shelves.end()) {
        return LibraryShelfSelection{};
    }
    return selection;
}

void move_book_before(Library& library, const LibraryShelfSelection& selection, SortOption option,
                      std::string_view source_id, std::string_view target_id) {
    // The reading list mixes books from every shelf, so a drop there has no
    // stored order to rewrite.
    if (selection.shelf == LibraryShelf::reading) {
        return;
    }

    const std::vector<LibraryBook> section = section_books(library, selection, option);
    const std::vector<std::string> section_ids = book_ids(section);
    const std::unordered_set<std::string> section_id_set(section_ids.begin(), section_ids.end());

    if (selection.shelf == LibraryShelf::shelf) {
        const auto shelf = std::find_if(library.shelves.begin(), library.shelves.end(),
                                        [&selection](const BookShelf& candidate) {
                                            return candidate.name == selection.name;
                                        });
        if (shelf != library.shelves.end()) {
            shelf->book_ids = reorder_ids(shelf->book_ids, source_id, target_id, section_ids);
            return;
        }
    }

    std::vector<std::string> section_order;
    section_order.reserve(section_ids.size());
    for (const std::string& id : library.manual_order) {
        if (section_id_set.contains(id)) {
            section_order.push_back(id);
        }
    }
    section_order = reorder_ids(std::move(section_order), source_id, target_id, section_ids);

    std::erase_if(library.manual_order, [&section_id_set](const std::string& id) {
        return section_id_set.contains(id);
    });
    library.manual_order.insert(library.manual_order.end(), section_order.begin(),
                                section_order.end());
}

std::optional<Bookmark> read_bookmark(const LibraryBook& book, double last_modified) {
    if (!book.book_info) {
        return std::nullopt;
    }

    int chapter_index = 0;
    bool has_spine_index = false;
    for (const auto& entry : book.book_info->chapter_info) {
        const ChapterInfo& chapter = entry.second;
        if (!chapter.spine_index) {
            continue;
        }
        if (!has_spine_index || *chapter.spine_index > chapter_index) {
            chapter_index = *chapter.spine_index;
            has_spine_index = true;
        }
    }

    Bookmark bookmark;
    bookmark.chapter_index = chapter_index;
    bookmark.progress = 1.0;
    bookmark.character_count = book.book_info->character_count;
    bookmark.last_modified = last_modified;
    return bookmark;
}

}  // namespace niratan::books
