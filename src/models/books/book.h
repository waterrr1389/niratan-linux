// SPDX-License-Identifier: GPL-3.0-or-later
//
// Library data, following Models/Book.swift (SortOption, BookReorder,
// BookMetadata, Bookmark, BookInfo, BookShelf) and the files
// Core/BookStorage.swift reads and writes.
//
// A book lives in Books/<folder>/. metadata.json holds BookMetadata,
// bookmark.json the reading position, bookinfo.json the character totals,
// statistics.json the reading sessions and highlights.json the highlights.
// Book order and shelves are stored next to the book directories, in
// Books/book_order.json and Books/shelves.json.
//
// Dates are seconds since 2001-01-01T00:00:00Z, which is what Foundation's
// deferred JSON date strategy writes; callers convert to and from the local
// clock. Identifiers are UUID strings in Foundation's uppercase form.
//
// The JSON codecs follow synthesized Codable: required keys must be present
// with the right type, optional keys may be absent, and extra keys are
// ignored. A wrong document is rejected as a whole rather than repaired.

#pragma once

#include <QtCore/qtconfigmacros.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/books/book_position.h"

QT_BEGIN_NAMESPACE
class QJsonDocument;
QT_END_NAMESPACE

namespace niratan::books {

enum class SortOption { recent, title, manual };

// The library order of a dragged book, following BookReorder. Empty when the
// book was dropped on itself.
std::optional<int> reorder_destination(int source_index, int target_index);

// The drag payload a shelf drop carries.
std::string reorder_payload(const std::string& book_id);
std::optional<std::string> reorder_book_id(std::string_view payload);

struct BookMetadata {
    std::string id;
    std::string title;
    std::optional<std::string> epub;
    std::optional<std::string> cover;
    std::string folder;
    double last_access = 0.0;
    std::optional<std::string> renamed_title;
    std::optional<std::string> profile_id;
    std::optional<std::string> book_language;
    std::optional<std::string> external_source_id;
    std::optional<std::string> external_isbn;
    std::optional<std::string> author;
    std::optional<int> character_count;
    std::optional<std::int64_t> modified;

    // renamedTitle ?? title
    std::string display_title() const;

    bool operator==(const BookMetadata&) const = default;
};

// Models/Highlight.swift (Highlight): character is book-global; offset is
// chapter-local raw DOM text. They use different counting rules.
struct Highlight {
    std::string id;
    int character;
    int offset;
    std::string text;
    std::string color;
    double created_at;
    bool operator==(const Highlight&) const = default;
};

bool is_highlight_color(std::string_view color);
QJsonDocument highlights_to_json(const std::vector<Highlight>& highlights);
std::optional<std::vector<Highlight>> highlights_from_json(const QJsonDocument& document);
std::vector<Highlight> highlights_in_range(const std::vector<Highlight>& highlights,
                                           int start, int end);

struct BookShelf {
    std::string name;
    std::vector<std::string> book_ids;

    bool operator==(const BookShelf&) const = default;
};

std::optional<BookMetadata> book_metadata_from_json(const QJsonDocument& document);
QJsonDocument book_metadata_to_json(const BookMetadata& book);

std::optional<Bookmark> bookmark_from_json(const QJsonDocument& document);
QJsonDocument bookmark_to_json(const Bookmark& bookmark);

std::optional<BookInfo> book_info_from_json(const QJsonDocument& document);
QJsonDocument book_info_to_json(const BookInfo& info);

std::optional<std::vector<BookShelf>> shelves_from_json(const QJsonDocument& document);
QJsonDocument shelves_to_json(const std::vector<BookShelf>& shelves);

std::optional<std::vector<std::string>> book_order_from_json(const QJsonDocument& document);
QJsonDocument book_order_to_json(const std::vector<std::string>& order);

// A book counts as in progress when 0 < progress < 0.999, following the
// library's reading filter.
bool is_in_progress(double progress);

}  // namespace niratan::books
