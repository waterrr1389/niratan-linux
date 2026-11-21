// SPDX-License-Identifier: GPL-3.0-or-later

#include "book_storage.h"

#include <QChar>
#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "services/novels/epub_book.h"
#include "services/reader/book_session.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

namespace niratan::books {
namespace {

constexpr const char* kMetadataFile = "metadata.json";
constexpr const char* kBookmarkFile = "bookmark.json";
constexpr const char* kSasayakiPlaybackFile = "sasayaki_playback.json";
constexpr const char* kSasayakiMatchFile = "sasayaki_match.json";
constexpr const char* kBookInfoFile = "bookinfo.json";
constexpr const char* kBookOrderFile = "book_order.json";
constexpr const char* kShelvesFile = "shelves.json";
constexpr const char* kLibraryPrefsFile = "library_prefs.json";
constexpr auto kBookshelfSortOption = "bookshelfSortOption";

// Foundation's reference date, 2001-01-01T00:00:00Z.
constexpr double kReferenceDateOffset = 978307200.0;

QString from_utf8(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::string to_utf8(const QString& text) { return text.toUtf8().toStdString(); }

// macOS forbids these characters in a file name; the Swift sanitizer also
// splits on every newline and control character. Only the file name component
// is sanitized, so a sanitized value never contains a separator.
bool is_forbidden_character(QChar character) {
    static const QString forbidden = QStringLiteral("\\/:*?\"<>|");
    return forbidden.contains(character) || character.category() == QChar::Other_Control ||
           character == QChar(0x2028) || character == QChar(0x2029);
}

// A missing or damaged sidecar is indistinguishable to the library: both mean
// the stored value is unavailable, and the bytes stay on disk untouched.
std::optional<QJsonDocument> read_document(const std::filesystem::path& path) {
    const storage::JsonReadResult read = storage::read_json_file(path);
    if (read.status != storage::JsonReadStatus::success) {
        return std::nullopt;
    }
    return read.document;
}

void remove_folder(const std::filesystem::path& folder) {
    std::error_code ignored;
    std::filesystem::remove_all(folder, ignored);
}

// Copies the package's cover image next to the EPUB and returns the reference
// stored in metadata: relative to the app directory, so the library stays
// relocatable. An absent or unreadable cover leaves the field empty.
std::optional<std::string> import_cover(const novels::EpubBook& book,
                                        const std::filesystem::path& folder,
                                        std::string_view folder_name) {
    const novels::EpubMetadata& metadata = book.metadata();
    if (!metadata.cover_href.has_value()) {
        return std::nullopt;
    }

    const std::string file_name =
        std::filesystem::path(*metadata.cover_href).filename().string();
    if (file_name.empty() || file_name == "." || file_name == "..") {
        return std::nullopt;
    }

    const std::optional<std::string> bytes = book.read(*metadata.cover_href);
    if (!bytes.has_value()) {
        return std::nullopt;
    }

    const std::filesystem::path destination = folder / file_name;
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) {
        return std::nullopt;
    }
    output.write(bytes->data(), static_cast<std::streamsize>(bytes->size()));
    output.close();
    if (!output) {
        std::error_code ignored;
        std::filesystem::remove(destination, ignored);
        return std::nullopt;
    }
    return "Books/" + std::string(folder_name) + "/" + file_name;
}

}  // namespace

std::string sanitize_file_name(std::string_view name) {
    const QString source = from_utf8(name);
    QString sanitized;
    sanitized.reserve(source.size());
    for (const QChar character : source) {
        sanitized.append(is_forbidden_character(character) ? QLatin1Char('_') : character);
    }
    return to_utf8(sanitized.trimmed());
}

double reference_now() {
    const double seconds = std::chrono::duration<double>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    return seconds - kReferenceDateOffset;
}

BookStorage::BookStorage(std::filesystem::path app_directory)
    : app_directory_(std::move(app_directory)), books_directory_(app_directory_ / "Books") {}

std::filesystem::path BookStorage::app_directory() const { return app_directory_; }
std::filesystem::path BookStorage::books_directory() const { return books_directory_; }

std::optional<std::filesystem::path> BookStorage::book_directory(std::string_view folder) const {
    if (folder.empty()) {
        return std::nullopt;
    }
    return storage::resolve_within(books_directory_, folder);
}

std::vector<BookMetadata> BookStorage::load_all_books() const {
    std::error_code error;
    std::filesystem::create_directories(books_directory_, error);

    std::vector<std::filesystem::path> folders;
    std::filesystem::directory_iterator iterator(books_directory_, error);
    const std::filesystem::directory_iterator end;
    while (!error && iterator != end) {
        std::error_code type_error;
        if (iterator->is_directory(type_error)) {
            folders.push_back(iterator->path());
        }
        iterator.increment(error);
    }

    std::sort(folders.begin(), folders.end(),
              [](const std::filesystem::path& left, const std::filesystem::path& right) {
                  return left.filename().string() < right.filename().string();
              });

    std::vector<BookMetadata> books;
    books.reserve(folders.size());
    for (const std::filesystem::path& folder : folders) {
        // Reading the file directly instead of through load_metadata keeps a
        // folder name that book_directory would reject out of the loop.
        const std::optional<QJsonDocument> document = read_document(folder / kMetadataFile);
        if (!document.has_value()) {
            continue;
        }
        if (std::optional<BookMetadata> book = book_metadata_from_json(*document)) {
            books.push_back(std::move(*book));
        }
    }
    return books;
}

std::optional<BookMetadata> BookStorage::load_metadata(std::string_view folder) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        return std::nullopt;
    }
    const std::optional<QJsonDocument> document = read_document(*directory / kMetadataFile);
    if (!document.has_value()) {
        return std::nullopt;
    }
    return book_metadata_from_json(*document);
}

bool BookStorage::save_metadata(const BookMetadata& book) const {
    const std::optional<std::filesystem::path> directory = book_directory(book.folder);
    if (!directory.has_value()) {
        return false;
    }
    return storage::write_json_file(*directory / kMetadataFile, book_metadata_to_json(book));
}

std::optional<Bookmark> BookStorage::load_bookmark(std::string_view folder) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        return std::nullopt;
    }
    const std::optional<QJsonDocument> document = read_document(*directory / kBookmarkFile);
    if (!document.has_value()) {
        return std::nullopt;
    }
    return bookmark_from_json(*document);
}

bool BookStorage::save_bookmark(std::string_view folder, const Bookmark& bookmark) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        return false;
    }
    return storage::write_json_file(*directory / kBookmarkFile, bookmark_to_json(bookmark));
}

bool BookStorage::has_book_directory(std::string_view folder) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    std::error_code error;
    return directory.has_value() && std::filesystem::is_directory(*directory, error);
}

bool BookStorage::delete_book(std::string_view folder) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        return false;
    }
    // A folder of "." resolves to Books itself.
    const std::filesystem::path relative = directory->lexically_relative(books_directory_);
    if (relative.empty() || relative == ".") {
        return false;
    }
    std::error_code error;
    std::filesystem::remove_all(*directory, error);
    return !error;
}

std::optional<sasayaki::SasayakiPlaybackData> BookStorage::load_sasayaki_playback(
    std::string_view folder) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        return std::nullopt;
    }
    const std::optional<QJsonDocument> document = read_document(*directory / kSasayakiPlaybackFile);
    if (!document.has_value()) {
        return std::nullopt;
    }
    return sasayaki::playback_data_from_json(*document);
}

bool BookStorage::save_sasayaki_playback(std::string_view folder,
                                         const sasayaki::SasayakiPlaybackData& playback) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        return false;
    }
    return storage::write_json_file(*directory / kSasayakiPlaybackFile,
                                    sasayaki::playback_data_to_json(playback));
}

std::optional<sasayaki::SasayakiMatchData> BookStorage::load_sasayaki_match(
    std::string_view folder) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        return std::nullopt;
    }
    const std::optional<QJsonDocument> document = read_document(*directory / kSasayakiMatchFile);
    if (!document.has_value()) {
        return std::nullopt;
    }
    return sasayaki::match_data_from_json(*document);
}

bool BookStorage::save_sasayaki_match(std::string_view folder,
                                      const sasayaki::SasayakiMatchData& match,
                                      std::string* reason) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        if (reason != nullptr) {
            *reason = "the book folder is outside the library";
        }
        return false;
    }
    return storage::write_json_file(*directory / kSasayakiMatchFile,
                                    sasayaki::match_data_to_json(match), reason);
}

std::optional<std::vector<Highlight>> BookStorage::load_highlights(std::string_view folder) const {
    const auto directory = book_directory(folder);
    if (!directory) {
        return std::nullopt;
    }
    const auto data = storage::read_json_file(*directory / "highlights.json");
    if (data.status == storage::JsonReadStatus::missing) {
        return std::vector<Highlight>{};
    }
    return highlights_from_json(data.document);
}

bool BookStorage::save_highlights(std::string_view folder,
                                   const std::vector<Highlight>& highlights) const {
    const auto directory = book_directory(folder);
    return directory && load_highlights(folder) &&
           storage::write_json_file(*directory / "highlights.json", highlights_to_json(highlights));
}

std::optional<BookInfo> BookStorage::load_book_info(std::string_view folder) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        return std::nullopt;
    }
    const std::optional<QJsonDocument> document = read_document(*directory / kBookInfoFile);
    if (!document.has_value()) {
        return std::nullopt;
    }
    return book_info_from_json(*document);
}

bool BookStorage::save_book_info(std::string_view folder, const BookInfo& info) const {
    const std::optional<std::filesystem::path> directory = book_directory(folder);
    if (!directory.has_value()) {
        return false;
    }
    return storage::write_json_file(*directory / kBookInfoFile, book_info_to_json(info));
}

std::optional<std::filesystem::path> BookStorage::epub_path(const BookMetadata& book) const {
    if (!book.epub.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::filesystem::path> directory = book_directory(book.folder);
    if (!directory.has_value()) {
        return std::nullopt;
    }
    return *directory / *book.epub;
}

std::optional<std::filesystem::path> BookStorage::cover_path(const BookMetadata& book) const {
    if (!book.cover.has_value()) {
        return std::nullopt;
    }
    // A cover outside the app directory is stored as an absolute path, as the
    // library does for a downloaded Google Drive thumbnail.
    if (book.cover->starts_with('/')) {
        return std::filesystem::path(*book.cover);
    }
    return app_directory_ / *book.cover;
}

std::vector<std::string> BookStorage::load_book_order() const {
    const std::optional<QJsonDocument> document =
        read_document(books_directory_ / kBookOrderFile);
    if (!document.has_value()) {
        return {};
    }
    return book_order_from_json(*document).value_or(std::vector<std::string>{});
}

bool BookStorage::save_book_order(const std::vector<std::string>& order) const {
    return storage::write_json_file(books_directory_ / kBookOrderFile,
                                    book_order_to_json(order));
}

std::vector<BookShelf> BookStorage::load_shelves() const {
    const std::optional<QJsonDocument> document = read_document(books_directory_ / kShelvesFile);
    if (!document.has_value()) {
        return {};
    }
    return shelves_from_json(*document).value_or(std::vector<BookShelf>{});
}

bool BookStorage::save_shelves(const std::vector<BookShelf>& shelves) const {
    return storage::write_json_file(books_directory_ / kShelvesFile, shelves_to_json(shelves));
}

int BookStorage::load_library_prefs() const {
    const auto document = read_document(app_directory_ / kLibraryPrefsFile);
    const int option = document ? document->object().value(kBookshelfSortOption).toInt() : 0;
    return option >= 0 && option <= 2 ? option : 0;
}

void BookStorage::save_library_prefs(int sort_option) const {
    storage::write_json_file(app_directory_ / kLibraryPrefsFile,
                             QJsonDocument(QJsonObject{{kBookshelfSortOption, sort_option}}));
}

ImportResult BookStorage::import_book(std::string_view source_path) const {
    const std::filesystem::path source(source_path);
    std::string error;
    const std::optional<novels::EpubBook> document = novels::EpubBook::open(source, &error);
    if (!document.has_value()) {
        return {{}, error, false};
    }

    const novels::EpubMetadata& metadata = document->metadata();
    const std::string title =
        metadata.title.empty() ? source.stem().string() : metadata.title;

    std::string folder_name = sanitize_file_name(title);
    if (folder_name.empty()) {
        folder_name = sanitize_file_name(source.stem().string());
    }
    if (folder_name.empty()) {
        return {{}, "Could not derive a book folder name from the title or file name", false};
    }

    const std::optional<std::filesystem::path> folder = book_directory(folder_name);
    if (!folder.has_value()) {
        return {{}, "The book folder is not inside the books directory", false};
    }

    std::error_code filesystem_error;
    if (std::filesystem::exists(*folder, filesystem_error)) {
        return {{}, "", true};
    }

    std::filesystem::create_directories(*folder, filesystem_error);
    if (filesystem_error) {
        return {{}, "Cannot create the book folder: " + filesystem_error.message(), false};
    }

    const std::filesystem::path epub = *folder / source.filename();
    std::filesystem::copy_file(source, epub, filesystem_error);
    if (filesystem_error) {
        remove_folder(*folder);
        return {{}, "Cannot copy the EPUB: " + filesystem_error.message(), false};
    }

    // The reader opens the copy, so the copy is what the import validates and
    // what the chapter counts come from.
    const std::optional<reader::BookSession> session = reader::BookSession::open(epub, &error);
    if (!session.has_value()) {
        remove_folder(*folder);
        return {{}, error, false};
    }

    BookMetadata book;
    book.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
    book.title = title;
    book.epub = source.filename().string();
    book.cover = import_cover(session->book(), *folder, folder_name);
    book.folder = folder_name;
    book.last_access = reference_now();
    book.book_language = metadata.language;

    if (!save_metadata(book)) {
        remove_folder(*folder);
        return {{}, "Cannot write " + std::string(kMetadataFile), false};
    }
    if (!save_book_info(folder_name, session->info())) {
        remove_folder(*folder);
        return {{}, "Cannot write " + std::string(kBookInfoFile), false};
    }

    return {book, "", false};
}

}  // namespace niratan::books
