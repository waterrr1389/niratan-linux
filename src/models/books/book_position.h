// SPDX-License-Identifier: GPL-3.0-or-later
//
// Models/Book.swift: Bookmark records the reading position; BookInfo maps
// between chapter progress and a book-global character count.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace niratan::books {

struct Bookmark {
    int chapter_index = 0;
    double progress = 0.0;
    int character_count = 0;
    std::optional<double> last_modified;

    bool operator==(const Bookmark&) const = default;
};

struct ChapterInfo {
    std::optional<int> spine_index;
    int current_total = 0;
    int chapter_count = 0;
    std::optional<std::map<std::string, int>> fragment_offsets;

    bool operator==(const ChapterInfo&) const = default;
};

struct BookInfo {
    int character_count = 0;
    std::map<std::string, ChapterInfo> chapter_info;
    std::optional<std::vector<std::string>> images;
    std::optional<std::map<std::string, int>> image_positions;

    bool operator==(const BookInfo&) const = default;

    // BookInfo.resolveCharacterPosition: the spine index and the progress
    // inside that chapter for a global character offset. nullopt when no
    // chapter covers it.
    std::optional<std::pair<int, double>> resolve_character_position(int characters) const;
};

}  // namespace niratan::books
