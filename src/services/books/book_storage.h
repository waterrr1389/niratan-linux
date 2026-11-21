// SPDX-License-Identifier: GPL-3.0-or-later
//
// The books directory, following Core/BookStorage.swift and
// Features/Bookshelf/BookshelfViewModel.swift (processImport, finalizeImport).
//
// Layout under the app directory:
//   Books/<folder>/metadata.json    BookMetadata
//   Books/<folder>/bookmark.json    Bookmark
//   Books/<folder>/bookinfo.json    BookInfo
//   Books/<folder>/<epub>           the imported EPUB
//   Books/<folder>/<cover>          the extracted cover image
//   Books/<folder>/sasayaki_playback.json  SasayakiPlaybackData
//   Books/<folder>/sasayaki_match.json     SasayakiMatchData
//   Books/book_order.json           [] of book IDs
//   Books/shelves.json              [] of BookShelf
//
// Import copies the source EPUB into a folder named after the sanitized title,
// reads its title, language, cover and chapter counts, and writes metadata and
// bookinfo. A folder that already exists is reported as a duplicate and left
// alone. A failure removes whatever this import created.
//
// Folders and stored paths are confined to the app directory with
// storage::resolve_within; a metadata file that cannot be decoded is skipped,
// not repaired. The cover reference is stored relative to the app directory,
// as Swift does ("Books/<folder>/<file>").
//
// sanitize_file_name replaces the characters macOS forbids (backslash, slash,
// colon, asterisk, question mark, quote, angle brackets), newlines and control
// characters with "_" and trims whitespace. The result may be empty, which
// import rejects by using the EPUB file name instead.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/books/book.h"
#include "models/sasayaki/sasayaki.h"

namespace niratan::books {

std::string sanitize_file_name(std::string_view name);

// Seconds since 2001-01-01T00:00:00Z, the reference date Foundation's JSON
// date strategy uses.
double reference_now();

struct ImportResult {
    std::optional<BookMetadata> book;
    // Empty unless the import failed.
    std::string error;
    bool already_exists = false;
};

class BookStorage {
   public:
    explicit BookStorage(std::filesystem::path app_directory);

    std::filesystem::path app_directory() const;
    std::filesystem::path books_directory() const;

    // nullopt when the folder is absolute or climbs out of Books.
    std::optional<std::filesystem::path> book_directory(std::string_view folder) const;
    // Whether book_directory(folder) names an existing directory.
    bool has_book_directory(std::string_view folder) const;

    // Directories without a readable metadata.json are skipped.
    std::vector<BookMetadata> load_all_books() const;
    std::optional<BookMetadata> load_metadata(std::string_view folder) const;
    bool save_metadata(const BookMetadata& book) const;

    std::optional<Bookmark> load_bookmark(std::string_view folder) const;
    bool save_bookmark(std::string_view folder, const Bookmark& bookmark) const;
    // A missing sidecar is empty. A damaged sidecar is not writable.
    std::optional<std::vector<Highlight>> load_highlights(std::string_view folder) const;
    bool save_highlights(std::string_view folder, const std::vector<Highlight>& highlights) const;

    // loadSasayakiPlayback / loadSasayakiMatch and BookStorage.save for the
    // Sasayaki sidecars. A missing or undecodable file loads as none.
    std::optional<sasayaki::SasayakiPlaybackData> load_sasayaki_playback(
        std::string_view folder) const;
    bool save_sasayaki_playback(std::string_view folder,
                                const sasayaki::SasayakiPlaybackData& playback) const;
    std::optional<sasayaki::SasayakiMatchData> load_sasayaki_match(std::string_view folder) const;
    // reason, when given, receives why the write failed.
    bool save_sasayaki_match(std::string_view folder, const sasayaki::SasayakiMatchData& match,
                             std::string* reason = nullptr) const;

    std::optional<BookInfo> load_book_info(std::string_view folder) const;
    bool save_book_info(std::string_view folder, const BookInfo& info) const;

    std::optional<std::filesystem::path> epub_path(const BookMetadata& book) const;
    std::optional<std::filesystem::path> cover_path(const BookMetadata& book) const;

    std::vector<std::string> load_book_order() const;
    bool save_book_order(const std::vector<std::string>& order) const;

    std::vector<BookShelf> load_shelves() const;
    bool save_shelves(const std::vector<BookShelf>& shelves) const;

    ImportResult import_book(const std::filesystem::path& source) const;

   private:
    std::filesystem::path app_directory_;
    std::filesystem::path books_directory_;
};

}  // namespace niratan::books
