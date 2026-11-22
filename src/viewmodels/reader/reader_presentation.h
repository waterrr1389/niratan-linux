// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader's information text, as pure functions of the values the reader
// view model holds, following NativeMac/NativeReaderView.swift
// (NativeReaderView.showsPageCount, progressString, progressLine and
// statisticsString), Features/Reader/Search/ReaderGoToView.swift
// (progressText and the Jump to alert's Go action) and Models/Profile.swift
// (ContentLanguageProfile.displayCount and rawCharacters).
//
// Every number is written with ASCII digits and no grouping, whatever the
// host locale: Swift interpolates plain Ints, formats percentages with
// String(format:) and the reading speed with grouping .never. The page
// count "current of total" is the one translatable string.
//
// progress_line:
// - shows_page_count (readerProgressCount Pages and not continuous) writes
//   "<current> of <total>" from `pages`, or "…" before the pages are
//   measured; otherwise a count mode other than Off writes
//   "<current> / <total>" in display units (English rounds raw characters
//   up to words of five).
// - show_percentage appends current / total * 100 with two decimals and a
//   percent sign, "0.00%" when total is not positive. The parts are joined
//   with a space; no part gives an empty line.
//
// progress_string: the book line when show_book, then the chapter line in
// parentheses when show_chapter, skipping empty lines, joined by a space at
// the top and by a newline at the bottom. The chapter line uses the
// table-of-contents chapter range's own current and total, never the
// characters remaining in it. Both lines read the same optional page
// progress (NativeReaderModel.pageProgress, a ReaderPageIndex.Progress), so
// either both have measured pages or both show "…".
//
// statistics_string: empty unless statistics are enabled. The reading speed
// in display units followed by " / h", then the session time as hours and
// two-digit minutes (Duration's hourMinute pattern, seconds rounded to the
// nearest minute, ties to even), joined by a space. Neither depends on
// whether tracking is running.
//
// position_text: "<current> / <total> (<percent with one decimal>%)" in
// display units, the percent from the raw counts.
//
// parse_jump_count: Swift's Int(String) for the Jump to alert: an optional
// sign then ASCII digits only, nothing else, no surrounding spaces, within
// the integer range; the count must not be negative. nullopt means Invalid
// input.

#pragma once

#include <QString>

#include <optional>

namespace niratan::viewmodels::reader_presentation {

// ContentLanguageProfile.displayCount(forRawCharacters:).
int display_count(int raw_characters, bool english);

// ContentLanguageProfile.rawCharacters(forDisplayCount:).
int raw_characters(int display_count, bool english);

struct PageCount {
    int current = 0;
    int total = 0;

    bool operator==(const PageCount&) const = default;
};

struct ProgressLineOptions {
    bool shows_page_count = false;
    bool count_off = false;
    bool show_percentage = true;
    bool english = false;
};

QString progress_line(int current, int total, std::optional<PageCount> pages,
                      const ProgressLineOptions& options);

// ReaderPageIndex.Progress: the book page and total, and the page and total
// inside the table-of-contents chapter range, all from measured pages.
struct PageProgress {
    int page = 0;
    int total = 0;
    int chapter_page = 0;
    int chapter_total = 0;

    bool operator==(const PageProgress&) const = default;
};

struct ProgressInput {
    ProgressLineOptions line;
    bool show_book = true;
    bool show_chapter = false;
    bool top = true;
    int book_current = 0;
    int book_total = 0;
    int chapter_current = 0;
    int chapter_total = 0;
    std::optional<PageProgress> pages;
};

QString progress_string(const ProgressInput& input);

QString statistics_string(bool enabled, bool show_speed, bool show_time, int raw_speed,
                          double reading_seconds, bool english);

QString position_text(int raw_character, int raw_total, bool english);

std::optional<int> parse_jump_count(const QString& input);

}  // namespace niratan::viewmodels::reader_presentation
