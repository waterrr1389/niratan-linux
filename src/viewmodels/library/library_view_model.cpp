// SPDX-License-Identifier: GPL-3.0-or-later

#include "library_view_model.h"
#include "library_view_model_state.h"

#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models/books/library_model.h"
#include "services/books/book_storage.h"
#include "services/storage/app_roots.h"

namespace niratan::viewmodels {
namespace {

using niratan::books::BookMetadata;
using niratan::books::BookShelf;
using niratan::books::Bookmark;
using niratan::books::BookStorage;
using niratan::books::Library;
using niratan::books::LibraryBook;
using niratan::books::LibraryShelf;
using niratan::books::LibraryShelfSelection;
using niratan::books::SortOption;
using niratan::books::book_progress;
using niratan::books::move_book_to_shelf;
using niratan::books::normalized_order;
using niratan::books::normalized_shelf_name;
using niratan::books::normalized_shelves;
using niratan::books::read_bookmark;
using niratan::books::resolved_selection;
using niratan::books::section_books;
using niratan::books::shelf_name_available;

// ShelfProgressStrip shows its finished check from this value up.
constexpr double kFinished = 0.999;

// The keys LibraryPage.qml assigns to selectedShelf. Anything else selects the
// whole library, which is also where an unknown shelf lands.
LibraryShelfSelection parse_selection(const std::string& value) {
    if (value == "reading") {
        return LibraryShelfSelection{LibraryShelf::reading, {}};
    }
    if (value == "unshelved") {
        return LibraryShelfSelection{LibraryShelf::unshelved, {}};
    }
    constexpr std::string_view kShelfPrefix = "shelf:";
    if (value.starts_with(kShelfPrefix)) {
        return LibraryShelfSelection{LibraryShelf::shelf, value.substr(kShelfPrefix.size())};
    }
    return LibraryShelfSelection{};
}

std::string selection_text(const LibraryShelfSelection& selection) {
    switch (selection.shelf) {
        case LibraryShelf::all:
            return "all";
        case LibraryShelf::reading:
            return "reading";
        case LibraryShelf::unshelved:
            return "unshelved";
        case LibraryShelf::shelf:
            return "shelf:" + selection.name;
    }
    return "all";
}

SortOption sort_option_of(int option) {
    switch (option) {
        case 1:
            return SortOption::title;
        case 2:
            return SortOption::manual;
        default:
            return SortOption::recent;
    }
}

std::vector<std::string> library_ids(const std::vector<LibraryBook>& books) {
    std::vector<std::string> ids;
    ids.reserve(books.size());
    for (const LibraryBook& book : books) {
        ids.push_back(book.metadata.id);
    }
    return ids;
}

std::vector<std::string> shelf_names(const std::vector<BookShelf>& shelves) {
    std::vector<std::string> names;
    names.reserve(shelves.size());
    for (const BookShelf& shelf : shelves) {
        names.push_back(shelf.name);
    }
    return names;
}

std::string trim(const std::string& text) {
    return QString::fromStdString(text).trimmed().toStdString();
}

// URL.pathExtension.lowercased() == "epub": the extension of the local path.
bool is_epub_file(const QUrl& url) {
    const QString path = url.toLocalFile();
    return !path.isEmpty() &&
        QFileInfo(path).suffix().compare(QStringLiteral("epub"), Qt::CaseInsensitive) == 0;
}

}  // namespace

LibraryViewModel::State::State() : storage(storage::data_root()) {}

LibraryShelfSelection LibraryViewModel::State::selection() const {
    return parse_selection(selected);
}

SortOption LibraryViewModel::State::sort_option() const {
    return sort_option_of(prefs.sort_option);
}

std::optional<std::size_t> LibraryViewModel::State::find_book(const std::string& folder) const {
    for (std::size_t index = 0; index < library.books.size(); ++index) {
        if (library.books[index].metadata.folder == folder) {
            return index;
        }
    }
    return std::nullopt;
}

// Reads every book with the two sidecars book_progress and read_bookmark
// use, then replaces the stored order and shelves with their normalized
// form. Only a changed value is written back.
void LibraryViewModel::State::load() {
    const std::vector<BookMetadata> metadata = storage.load_all_books();
    library.books.clear();
    library.books.reserve(metadata.size());
    for (const BookMetadata& book : metadata) {
        LibraryBook entry;
        entry.metadata = book;
        entry.bookmark = storage.load_bookmark(book.folder);
        entry.book_info = storage.load_book_info(book.folder);
        library.books.push_back(std::move(entry));
    }

    const std::vector<std::string> ids = library_ids(library.books);
    const std::vector<std::string> stored_order = storage.load_book_order();
    library.manual_order = normalized_order(stored_order, ids, ids);
    if (library.manual_order != stored_order) {
        storage.save_book_order(library.manual_order);
    }

    const std::vector<BookShelf> stored_shelves = storage.load_shelves();
    library.shelves = normalized_shelves(stored_shelves, library.books);
    if (library.shelves != stored_shelves) {
        storage.save_shelves(library.shelves);
    }
}

void LibraryViewModel::State::rebuild_views() {
    books_view.clear();
    const std::vector<LibraryBook> section =
        section_books(library, selection(), sort_option_of(prefs.sort_option));
    for (const LibraryBook& book : section) {
        const double value = book_progress(book);
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), QString::fromStdString(book.metadata.id));
        entry.insert(QStringLiteral("folder"), QString::fromStdString(book.metadata.folder));
        entry.insert(QStringLiteral("title"),
                     QString::fromStdString(book.metadata.display_title()));
        entry.insert(QStringLiteral("storedTitle"), QString::fromStdString(book.metadata.title));
        entry.insert(QStringLiteral("progress"), value);
        entry.insert(QStringLiteral("percent"), QString::number(value * 100.0, 'f', 1));
        entry.insert(QStringLiteral("finished"), value >= kFinished);
        const auto cover = storage.cover_path(book.metadata);
        entry.insert(QStringLiteral("cover"),
                     cover.has_value()
                         ? QUrl::fromLocalFile(QString::fromStdString(cover->string())).toString()
                         : QString());
        books_view.append(entry);
    }

    shelves_view.clear();
    for (const BookShelf& shelf : library.shelves) {
        const LibraryShelfSelection shelf_selection{LibraryShelf::shelf, shelf.name};
        shelves_view.append(
            QVariantMap{{QStringLiteral("name"), QString::fromStdString(shelf.name)},
                        {QStringLiteral("count"), books::book_count(library, shelf_selection)}});
    }
}

LibraryViewModel::LibraryViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->prefs = state_->storage.load_library_prefs();
    refresh();
}

LibraryViewModel::~LibraryViewModel() = default;

QVariantList LibraryViewModel::books() const { return state_->books_view; }

QVariantList LibraryViewModel::shelves() const { return state_->shelves_view; }

QString LibraryViewModel::selected_shelf() const {
    return QString::fromStdString(state_->selected);
}

void LibraryViewModel::setSelectedShelf(const QString& shelf) {
    const std::string resolved =
        selection_text(resolved_selection(state_->library, parse_selection(shelf.toStdString())));
    if (state_->selected == resolved) {
        return;
    }
    state_->selected = resolved;
    state_->rebuild_views();
    emit booksChanged();
    emit selectionChanged();
}

int LibraryViewModel::sort_option() const { return state_->prefs.sort_option; }

void LibraryViewModel::setSortOption(int option) {
    const int clamped = std::clamp(option, 0, 2);
    if (state_->prefs.sort_option == clamped) {
        return;
    }
    state_->prefs.sort_option = clamped;
    state_->storage.save_library_prefs(state_->prefs);
    state_->rebuild_views();
    emit booksChanged();
    emit sortChanged();
}

int LibraryViewModel::cover_width() const { return state_->prefs.cover_width; }

void LibraryViewModel::setCoverWidth(int width) {
    const int clamped = std::clamp(width, 110, 240);
    if (state_->prefs.cover_width == clamped) {
        return;
    }
    state_->prefs.cover_width = clamped;
    state_->storage.save_library_prefs(state_->prefs);
    emit layoutChanged();
}

QString LibraryViewModel::error() const { return state_->error; }

bool LibraryViewModel::isImportError() const { return state_->import_error; }

QString LibraryViewModel::import_progress() const {
    return import_progress_;
}

void LibraryViewModel::clearError() {
    if (state_->error.isEmpty()) {
        return;
    }
    state_->error.clear();
    state_->import_error = false;
    emit errorChanged();
}

QVariantMap LibraryViewModel::smart_counts() const {
    const auto count = [this](LibraryShelf shelf) {
        return books::book_count(state_->library, LibraryShelfSelection{shelf, {}});
    };
    return {{QStringLiteral("all"), count(LibraryShelf::all)},
            {QStringLiteral("reading"), count(LibraryShelf::reading)},
            {QStringLiteral("unshelved"), count(LibraryShelf::unshelved)}};
}

int LibraryViewModel::book_count() const {
    return books::book_count(state_->library, state_->selection());
}

void LibraryViewModel::refresh() {
    state_->load();

    const std::string resolved =
        selection_text(resolved_selection(state_->library, state_->selection()));
    const bool selection_moved = resolved != state_->selected;
    state_->selected = resolved;

    state_->rebuild_views();
    if (selection_moved) {
        emit selectionChanged();
    }
    emit booksChanged();
    emit shelvesChanged();
}

void LibraryViewModel::importBooks(const QList<QUrl>& files) {
    // BookshelfViewModel.importBooks: one file imports directly; several show
    // the next 1-based file while the batch runs.
    const bool show_progress = files.size() > 1;
    const auto update_progress = [this](const QString& text) {
        if (import_progress_ == text) {
            return;
        }
        import_progress_ = text;
        emit importProgressChanged();
    };
    if (show_progress) {
        update_progress(tr("Importing %1 / %2...").arg(1).arg(files.size()));
    }

    bool imported = false;
    QStringList failures;
    for (qsizetype index = 0; index < files.size(); ++index) {
        const QUrl& file = files.at(index);
        const books::ImportResult result =
            state_->storage.import_book(file.toLocalFile().toStdString());
        if (result.book.has_value()) {
            imported = true;
        } else if (!result.already_exists) {
            failures.append(QFileInfo(file.toLocalFile()).fileName());
        }
        if (show_progress && index + 1 < files.size()) {
            update_progress(tr("Importing %1 / %2...").arg(index + 2).arg(files.size()));
        }
    }
    if (show_progress) {
        update_progress({});
    }
    if (imported) {
        refresh();
    }
    if (!failures.isEmpty()) {
        state_->error = tr("Failed to import: %1. Check that the files are readable EPUBs and try again.")
                            .arg(failures.join(QStringLiteral(", ")));
        state_->import_error = true;
        emit errorChanged();
    }
}

bool LibraryViewModel::acceptsDrop(const QList<QUrl>& files) const {
    return std::ranges::any_of(files, is_epub_file);
}

bool LibraryViewModel::importDroppedEPUBs(const QList<QUrl>& files) {
    QList<QUrl> epubs;
    for (const QUrl& file : files) {
        if (is_epub_file(file)) {
            epubs.append(file);
        }
    }
    if (epubs.isEmpty()) {
        return false;
    }
    importBooks(epubs);
    return true;
}

bool LibraryViewModel::canExportBook(const QString& folder) const {
    const auto index = state_->find_book(folder.toStdString());
    if (!index.has_value()) {
        return false;
    }
    return state_->library.books[*index].metadata.epub.has_value();
}

QString LibraryViewModel::exportBaseName(const QString& folder) const {
    const auto index = state_->find_book(folder.toStdString());
    if (!index.has_value()) {
        return {};
    }
    return QString::fromStdString(
        books::export_base_name(state_->library.books[*index].metadata.display_title()));
}

void LibraryViewModel::exportBook(const QString& folder, const QUrl& destination) {
    const auto index = state_->find_book(folder.toStdString());
    if (!index.has_value()) {
        state_->error = tr("Book not found.");
        state_->import_error = false;
        emit errorChanged();
        return;
    }
    const books::ExportResult result = state_->storage.export_epub(
        state_->library.books[*index].metadata, destination.toLocalFile().toStdString());
    if (result.outcome != books::ExportOutcome::exported) {
        state_->error = QString::fromStdString(result.error);
        state_->import_error = false;
        emit errorChanged();
        return;
    }
    emit bookExported(destination.toLocalFile());
}

void LibraryViewModel::openBook(const QString& folder) {
    if (folder.isEmpty()) {
        return;
    }
    emit bookOpened(folder);
}

void LibraryViewModel::recordBookAccess(const QString& folder) {
    if (folder.isEmpty()) {
        return;
    }
    const std::string folder_name = folder.toStdString();
    auto metadata = state_->storage.load_metadata(folder_name);
    if (!metadata.has_value() || metadata->folder != folder_name) {
        return;
    }
    metadata->last_access = books::reference_now();
    state_->storage.save_metadata(*metadata);
    if (const auto index = state_->find_book(folder_name)) {
        state_->library.books[*index].metadata.last_access = metadata->last_access;
        if (sort_option_of(state_->prefs.sort_option) == SortOption::recent) {
            state_->rebuild_views();
            emit booksChanged();
        }
    }
}

void LibraryViewModel::renameBook(const QString& folder, const QString& title) {
    const auto index = state_->find_book(folder.toStdString());
    if (!index.has_value()) {
        return;
    }
    const std::string trimmed = trim(title.toStdString());
    BookMetadata& book = state_->library.books[*index].metadata;
    book.renamed_title = trimmed.empty() ? std::nullopt : std::optional<std::string>(trimmed);
    state_->storage.save_metadata(book);
    refresh();
}

void LibraryViewModel::deleteBook(const QString& folder) {
    const std::string folder_name = folder.toStdString();
    const auto index = state_->find_book(folder_name);
    if (!index.has_value()) {
        return;
    }
    const std::string id = state_->library.books[*index].metadata.id;
    if (!state_->storage.delete_book(folder_name)) {
        state_->error = tr("Could not delete the book.");
        state_->import_error = false;
        emit errorChanged();
        return;
    }
    // A deleted book is off every shelf, and the model helper is what takes an
    // ID out of all of them.
    move_book_to_shelf(state_->library.shelves, id, std::nullopt);
    state_->storage.save_shelves(state_->library.shelves);
    std::erase(state_->library.manual_order, id);
    state_->storage.save_book_order(state_->library.manual_order);
    refresh();
}

bool LibraryViewModel::isLocalBook(const QString& folder) const {
    const auto index = state_->find_book(folder.toStdString());
    return index && !state_->library.books[*index].metadata.external_source_id;
}

void LibraryViewModel::markRead(const QString& folder) {
    const auto index = state_->find_book(folder.toStdString());
    if (!index.has_value()) {
        return;
    }
    LibraryBook& book = state_->library.books[*index];
    if (book.metadata.external_source_id) {
        return;
    }
    const std::optional<Bookmark> bookmark = read_bookmark(book, books::reference_now());
    if (!bookmark.has_value()) {
        return;
    }
    if (!state_->storage.save_bookmark(book.metadata.folder, *bookmark)) {
        return;
    }
    book.bookmark = bookmark;
    state_->rebuild_views();
    emit booksChanged();
}

QString LibraryViewModel::createShelf(const QString& name) {
    const std::optional<std::string> normalized = normalized_shelf_name(name.toStdString());
    if (!normalized.has_value()) {
        return {};
    }
    if (!shelf_name_available(*normalized, shelf_names(state_->library.shelves))) {
        return {};
    }
    state_->library.shelves.push_back(BookShelf{*normalized, {}});
    state_->storage.save_shelves(state_->library.shelves);
    refresh();
    return QString::fromStdString(*normalized);
}

bool LibraryViewModel::renameShelf(const QString& from, const QString& to) {
    const std::string old_name = from.toStdString();
    const auto shelf = std::find_if(state_->library.shelves.begin(), state_->library.shelves.end(),
                                    [&old_name](const BookShelf& candidate) {
                                        return candidate.name == old_name;
                                    });
    if (shelf == state_->library.shelves.end()) {
        return false;
    }
    const std::optional<std::string> normalized = normalized_shelf_name(to.toStdString());
    if (!normalized.has_value()) {
        return false;
    }
    if (!shelf_name_available(*normalized, shelf_names(state_->library.shelves), old_name)) {
        return false;
    }

    shelf->name = *normalized;
    state_->storage.save_shelves(state_->library.shelves);
    const std::string old_key =
        selection_text(LibraryShelfSelection{LibraryShelf::shelf, old_name});
    if (state_->selected == old_key) {
        state_->selected =
            selection_text(LibraryShelfSelection{LibraryShelf::shelf, *normalized});
        emit selectionChanged();
    }
    refresh();
    return true;
}

void LibraryViewModel::deleteShelf(const QString& name) {
    const std::string target = name.toStdString();
    std::erase_if(state_->library.shelves, [&target](const BookShelf& shelf) {
        return shelf.name == target;
    });
    state_->storage.save_shelves(state_->library.shelves);
    if (state_->selected ==
        selection_text(LibraryShelfSelection{LibraryShelf::shelf, target})) {
        state_->selected = selection_text(LibraryShelfSelection{});
        emit selectionChanged();
    }
    refresh();
}

QString LibraryViewModel::shelfForBook(const QString& folder) const {
    const auto index = state_->find_book(folder.toStdString());
    if (!index.has_value()) {
        return {};
    }
    const std::string& id = state_->library.books[*index].metadata.id;
    for (const BookShelf& shelf : state_->library.shelves) {
        if (std::ranges::find(shelf.book_ids, id) != shelf.book_ids.end()) {
            return QString::fromStdString(shelf.name);
        }
    }
    return {};
}

void LibraryViewModel::moveBookToShelf(const QString& folder, const QString& shelf) {
    moveBooksToShelf({folder}, shelf);
}

void LibraryViewModel::moveBooksToShelf(const QStringList& folders, const QString& shelf) {
    const std::string target = shelf.toStdString();
    // An empty name is the move menu's "None", which Swift spells as a nil
    // destination: the book leaves every shelf and joins none.
    const std::optional<std::string> destination =
        target.empty() ? std::nullopt : std::optional<std::string>(target);
    for (const QString& folder : folders) {
        const auto index = state_->find_book(folder.toStdString());
        if (!index.has_value()) {
            continue;
        }
        move_book_to_shelf(state_->library.shelves, state_->library.books[*index].metadata.id,
                           destination);
    }
    state_->storage.save_shelves(state_->library.shelves);
    refresh();
}

}  // namespace niratan::viewmodels
