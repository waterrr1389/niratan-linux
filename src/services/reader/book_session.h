// SPDX-License-Identifier: GPL-3.0-or-later
//
// An EPUB opened for reading, following Util/BookProcessor.swift (process),
// Core/BookStorage.swift and the reader's chapter accounting.
//
// open() parses the container with services/novels/epub_book.h and counts each
// spine item's readable characters with services/reader/character_normalizer.h,
// building the same BookInfo the importer stores: per chapter path the spine
// index, the running character total before the chapter, the chapter's own
// count and, when the table of contents points inside the chapter, the
// fragment offsets. Chapters that cannot be read are skipped, which also keeps
// their spine index unused, as Swift does when the manifest item is missing.
//
// The session keeps the archive open and serves chapter documents (injected
// with services/reader/reader_shell.h) and raw container entries (images,
// stylesheets and fonts) for the reader's URL scheme handler. MIME types come
// from a fixed extension table, because the container paths are the only
// source and the mapping must be deterministic.
//
// chapter_title() prefers the first table of contents entry that points at the
// chapter path without a fragment, else the chapter's own path. This is the
// label the chapter list shows when the table of contents has no entry.
// contents() preserves navigation order and nesting, following
// ChapterListViewModel's TOC-to-spine mapping. Unmapped entries are omitted;
// only a missing TOC falls back to the spine. Fragment IDs are decoded for the DOM.

#pragma once

#include <QFileSystemWatcher>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models/books/book_position.h"
#include "models/profiles/dictionary_settings.h"
#include "models/profiles/reader_settings.h"
#include "services/novels/epub_book.h"
#include "services/reader/reader_shell.h"
#include "services/reader/session_resource.h"

namespace niratan::reader {

struct ContentsEntry {
    int spine_index;
    int level;
    std::string title;
    std::string fragment;
};

class BookSession {
   public:
    static std::optional<BookSession> open(const std::filesystem::path& path,
                                           std::string* error = nullptr);

    const niratan::novels::EpubBook& book() const;
    const niratan::books::BookInfo& info() const;

    int spine_count() const;
    std::string spine_path(int spine_index) const;
    std::string chapter_title(int spine_index) const;
    const std::vector<ContentsEntry>& contents() const;
    // ReaderChapterIndex.chapterStart: unknown fragments use the spine start;
    // a missing chapter count means the entry has no character position.
    std::optional<int> contents_character_start(const ContentsEntry& entry) const;

    // The chapter with the stylesheet and script inserted, or an empty string
    // when the index is out of range or the entry cannot be read.
    std::string chapter_document(int spine_index, const ShellInput& input,
                                 const ReaderScripts& scripts) const;

    // Raw container bytes for a path relative to the archive root.
    std::optional<SessionResource> resource(std::string_view path) const;

    // A bookmark position: the global character offset of `progress` inside
    // the chapter. Chapters without a recorded count map to their start.
    int character_offset(int spine_index, double progress) const;

    // The chapter and progress for a global character offset, or nullopt when
    // no chapter covers it.
    std::optional<std::pair<int, double>> position_for_character(int characters) const;

    // The characters left in the table-of-contents chapter that holds
    // character_offset(spine_index, progress), following
    // NativeReaderView.displayedChapterCharactersRemaining
    // (displayedProgressChapterRange and ReaderChapterIndex.ChapterRange):
    // chapter starts are 0 plus every contents entry, shifted by that
    // entry's fragment offset, and a range ends at the next start or the book
    // end. The lookup is kept inside the spine item, so the end of a chapter
    // reports zero rather than the next chapter's count. A book without
    // characters, or a position with no chapter, has nothing remaining.
    int chapter_characters_remaining(int spine_index, double progress) const;
    // NativeReaderView.progressString uses the same range for its chapter line.
    double logical_chapter_progress(int spine_index, double progress) const;
    // The position inside that range and the range's character count.
    std::pair<int, int> logical_chapter_position(int spine_index, double progress) const;

   private:
    BookSession(niratan::novels::EpubBook book, niratan::books::BookInfo info,
                 std::vector<std::string> spine_paths,
                 std::vector<std::optional<std::string>> spine_titles,
                 std::vector<ContentsEntry> contents);

    niratan::novels::EpubBook book_;
    niratan::books::BookInfo info_;
    std::vector<std::string> spine_paths_;
    std::vector<std::optional<std::string>> spine_titles_;
    std::vector<ContentsEntry> contents_;
};

// Resolves the active profile from profiles.json on every read. Directory
// watches cover atomic replacement and settings files not yet created.
class ReaderSettingsWatch {
   public:
    ReaderSettingsWatch(std::filesystem::path root, std::function<void()> changed);
    std::optional<profiles::ReaderProfileSettings> read();
    std::string content_language = "ja";
    profiles::DictionaryProfileSettings lookup_settings = profiles::default_dictionary_settings();

   private:
    std::filesystem::path root_;
    std::filesystem::path settings_path_;
    QFileSystemWatcher watcher_;
};

// The container MIME type for a path: the reference EPUB types by extension,
// else application/octet-stream.
std::string container_mime_type(std::string_view path);

}  // namespace niratan::reader
