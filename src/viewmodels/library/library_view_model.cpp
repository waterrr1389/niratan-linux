// SPDX-License-Identifier: GPL-3.0-or-later

#include "library_view_model.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "services/books/book_storage.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels {
namespace {

using niratan::books::BookInfo;
using niratan::books::BookMetadata;
using niratan::books::BookShelf;
using niratan::books::BookStorage;
using niratan::books::Bookmark;

constexpr double kFinished = 0.999;

std::string trim(const std::string& text) {
    const QString trimmed = QString::fromStdString(text).trimmed();
    return trimmed.toStdString();
}

bool same_text(const std::string& left, const std::string& right) {
    return QString::fromStdString(left).compare(QString::fromStdString(right),
                                                Qt::CaseInsensitive) == 0;
}

// Code point order, which is stable across locales. Swift uses localized
// standard ordering; the difference only shows for scripts whose collation
// differs from code point order (Kana, case, accents).
bool title_less(const BookMetadata& left, const BookMetadata& right) {
    return left.display_title() < right.display_title();
}

// Drops unknown and duplicate ids, then appends the fallback ids that are
// still missing.
std::vector<std::string> normalized_order(const std::vector<std::string>& order,
                                          const std::set<std::string>& valid,
                                          const std::vector<std::string>& fallback) {
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const std::string& id : order) {
        if (valid.contains(id) && seen.insert(id).second) {
            result.push_back(id);
        }
    }
    for (const std::string& id : fallback) {
        if (valid.contains(id) && seen.insert(id).second) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<std::string> book_ids(const std::vector<BookMetadata>& books) {
    std::vector<std::string> ids;
    ids.reserve(books.size());
    for (const BookMetadata& book : books) {
        ids.push_back(book.id);
    }
    return ids;
}

}  // namespace

struct LibraryViewModel::State {
    explicit State() : storage(storage::data_root()) {}

    BookStorage storage;
    std::vector<BookMetadata> books;
    std::map<std::string, double> progress;
    std::vector<BookShelf> shelves;
    std::vector<std::string> manual_order;
    std::string selected = "all";
    int sort_option = 0;
    int cover_width = 160;
    QString error;
    QVariantList books_view;
    QVariantList shelves_view;

    std::optional<std::size_t> find_book(const std::string& folder) const {
        for (std::size_t index = 0; index < books.size(); ++index) {
            if (books[index].folder == folder) {
                return index;
            }
        }
        return std::nullopt;
    }

    std::optional<std::size_t> find_book_by_id(const std::string& id) const {
        for (std::size_t index = 0; index < books.size(); ++index) {
            if (books[index].id == id) {
                return index;
            }
        }
        return std::nullopt;
    }

    bool in_shelf(const BookShelf& shelf, const std::string& id) const {
        return std::find(shelf.book_ids.begin(), shelf.book_ids.end(), id) != shelf.book_ids.end();
    }

    double book_progress(const BookMetadata& book) const {
        const auto found = progress.find(book.id);
        return found == progress.end() ? 0.0 : found->second;
    }

    void load_progress() {
        progress.clear();
        for (const BookMetadata& book : books) {
            const std::optional<BookInfo> info = storage.load_book_info(book.folder);
            const std::optional<Bookmark> bookmark = storage.load_bookmark(book.folder);
            int total = book.character_count.value_or(0);
            if (info.has_value() && info->character_count > 0) {
                total = info->character_count;
            }
            if (bookmark.has_value() && total > 0) {
                progress[book.id] =
                    std::clamp(static_cast<double>(bookmark->character_count) / total, 0.0, 1.0);
            } else {
                progress[book.id] = 0.0;
            }
        }
    }

    void load_order() {
        manual_order = storage.load_book_order();
        const std::vector<std::string> ids = book_ids(books);
        manual_order = normalized_order(manual_order, std::set<std::string>(ids.begin(), ids.end()),
                                        ids);
        storage.save_book_order(manual_order);
    }

    void load_shelves() {
        shelves = storage.load_shelves();
        const std::vector<std::string> ids = book_ids(books);
        const std::set<std::string> valid(ids.begin(), ids.end());
        for (BookShelf& shelf : shelves) {
            const std::vector<std::string> before = shelf.book_ids;
            shelf.book_ids = normalized_order(shelf.book_ids, valid, shelf.book_ids);
            if (shelf.book_ids != before) {
                storage.save_shelves(shelves);
            }
        }
    }

    bool is_unshelved(const std::string& id) const {
        for (const BookShelf& shelf : shelves) {
            if (in_shelf(shelf, id)) {
                return false;
            }
        }
        return true;
    }

    std::vector<const BookMetadata*> section_books() const {
        std::vector<const BookMetadata*> result;
        if (selected == "all") {
            for (const BookMetadata& book : books) {
                result.push_back(&book);
            }
        } else if (selected == "reading") {
            for (const BookMetadata& book : books) {
                const double value = book_progress(book);
                if (value > 0.0 && value < kFinished) {
                    result.push_back(&book);
                }
            }
        } else if (selected == "unshelved") {
            for (const BookMetadata& book : books) {
                if (is_unshelved(book.id)) {
                    result.push_back(&book);
                }
            }
        } else {
            const std::string name = selected.substr(std::string("shelf:").size());
            const auto shelf = std::find_if(shelves.begin(), shelves.end(),
                                            [&name](const BookShelf& candidate) {
                                                return candidate.name == name;
                                            });
            if (shelf == shelves.end()) {
                for (const BookMetadata& book : books) {
                    result.push_back(&book);
                }
            } else {
                for (const BookMetadata& book : books) {
                    if (in_shelf(*shelf, book.id)) {
                        result.push_back(&book);
                    }
                }
            }
        }
        return result;
    }

    std::vector<std::string> active_manual_order() const {
        if (selected.starts_with("shelf:")) {
            const std::string name = selected.substr(std::string("shelf:").size());
            const auto shelf = std::find_if(shelves.begin(), shelves.end(),
                                            [&name](const BookShelf& candidate) {
                                                return candidate.name == name;
                                            });
            if (shelf != shelves.end()) {
                return shelf->book_ids;
            }
        }
        return manual_order;
    }

    void sort_section(std::vector<const BookMetadata*>& section) const {
        if (sort_option == 0) {
            std::stable_sort(section.begin(), section.end(),
                             [](const BookMetadata* left, const BookMetadata* right) {
                                 return left->last_access > right->last_access;
                             });
        } else if (sort_option == 1) {
            std::stable_sort(section.begin(), section.end(),
                             [](const BookMetadata* left, const BookMetadata* right) {
                                 return title_less(*left, *right);
                             });
        } else {
            const std::vector<std::string> order = active_manual_order();
            std::map<std::string, int> rank;
            for (std::size_t index = 0; index < order.size(); ++index) {
                rank.emplace(order[index], static_cast<int>(index));
            }
            std::stable_sort(section.begin(), section.end(),
                             [&rank](const BookMetadata* left, const BookMetadata* right) {
                                 const auto left_rank = rank.find(left->id);
                                 const auto right_rank = rank.find(right->id);
                                 if (left_rank != rank.end() && right_rank != rank.end()) {
                                     return left_rank->second < right_rank->second;
                                 }
                                 if (left_rank != rank.end()) {
                                     return true;
                                 }
                                 if (right_rank != rank.end()) {
                                     return false;
                                 }
                                 return title_less(*left, *right);
                             });
        }
    }

    void rebuild_views() {
        books_view.clear();
        std::vector<const BookMetadata*> section = section_books();
        sort_section(section);
        for (const BookMetadata* book : section) {
            QVariantMap entry;
            entry.insert(QStringLiteral("id"), QString::fromStdString(book->id));
            entry.insert(QStringLiteral("folder"), QString::fromStdString(book->folder));
            entry.insert(QStringLiteral("title"), QString::fromStdString(book->display_title()));
            const double value = book_progress(*book);
            entry.insert(QStringLiteral("progress"), value);
            entry.insert(QStringLiteral("percent"), QString::number(value * 100.0, 'f', 1));
            entry.insert(QStringLiteral("finished"), value >= kFinished);
            const auto cover = storage.cover_path(*book);
            entry.insert(QStringLiteral("cover"),
                         cover.has_value()
                             ? QUrl::fromLocalFile(QString::fromStdString(cover->string())).toString()
                             : QString());
            books_view.append(entry);
        }

        shelves_view.clear();
        for (const BookShelf& shelf : shelves) {
            const int count = static_cast<int>(std::count_if(
                books.begin(), books.end(),
                [this, &shelf](const BookMetadata& book) { return in_shelf(shelf, book.id); }));
            shelves_view.append(QVariantMap{{QStringLiteral("name"),
                                             QString::fromStdString(shelf.name)},
                                            {QStringLiteral("count"), count}});
        }
    }
};

LibraryViewModel::LibraryViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    refresh();
}

LibraryViewModel::~LibraryViewModel() = default;

QVariantList LibraryViewModel::books() const { return state_->books_view; }

QVariantList LibraryViewModel::shelves() const { return state_->shelves_view; }

QString LibraryViewModel::selected_shelf() const {
    return QString::fromStdString(state_->selected);
}

void LibraryViewModel::setSelectedShelf(const QString& shelf) {
    const std::string value = shelf.toStdString();
    if (state_->selected == value) {
        return;
    }
    state_->selected = value;
    state_->rebuild_views();
    emit booksChanged();
    emit selectionChanged();
}

int LibraryViewModel::sort_option() const { return state_->sort_option; }

void LibraryViewModel::setSortOption(int option) {
    const int clamped = std::clamp(option, 0, 2);
    if (state_->sort_option == clamped) {
        return;
    }
    state_->sort_option = clamped;
    state_->rebuild_views();
    emit booksChanged();
    emit sortChanged();
}

int LibraryViewModel::cover_width() const { return state_->cover_width; }

void LibraryViewModel::setCoverWidth(int width) {
    const int clamped = std::clamp(width, 110, 240);
    if (state_->cover_width == clamped) {
        return;
    }
    state_->cover_width = clamped;
    emit layoutChanged();
}

QString LibraryViewModel::error() const { return state_->error; }

int LibraryViewModel::book_count() const { return state_->books_view.size(); }

void LibraryViewModel::refresh() {
    state_->books = state_->storage.load_all_books();
    state_->load_progress();
    state_->load_order();
    state_->load_shelves();

    if (state_->selected.starts_with("shelf:")) {
        const std::string name = state_->selected.substr(std::string("shelf:").size());
        const bool exists = std::any_of(state_->shelves.begin(), state_->shelves.end(),
                                        [&name](const BookShelf& shelf) {
                                            return shelf.name == name;
                                        });
        if (!exists) {
            state_->selected = "all";
            emit selectionChanged();
        }
    }

    state_->rebuild_views();
    emit booksChanged();
    emit shelvesChanged();
}

void LibraryViewModel::importBooks(const QList<QUrl>& files) {
    bool imported = false;
    QStringList failures;
    for (const QUrl& file : files) {
        const books::ImportResult result =
            state_->storage.import_book(file.toLocalFile().toStdString());
        if (result.book.has_value()) {
            imported = true;
        } else if (!result.already_exists) {
            failures.append(QFileInfo(file.toLocalFile()).fileName());
            state_->error = QString::fromStdString(result.error);
            emit errorChanged();
        }
    }
    if (imported) {
        refresh();
    }
    if (!failures.isEmpty() && state_->error.isEmpty()) {
        state_->error = tr("Failed to import: %1").arg(failures.join(QStringLiteral(", ")));
        emit errorChanged();
    }
}

void LibraryViewModel::openBook(const QString& folder) {
    if (folder.isEmpty()) {
        return;
    }
    emit bookOpened(folder);
}

void LibraryViewModel::renameBook(const QString& folder, const QString& title) {
    const auto index = state_->find_book(folder.toStdString());
    if (!index.has_value()) {
        return;
    }
    const std::string trimmed = trim(title.toStdString());
    state_->books[*index].renamed_title =
        trimmed.empty() ? std::nullopt : std::optional<std::string>(trimmed);
    state_->storage.save_metadata(state_->books[*index]);
    refresh();
}

void LibraryViewModel::deleteBook(const QString& folder) {
    const std::string folder_id = folder.toStdString();
    const auto index = state_->find_book(folder_id);
    if (!index.has_value()) {
        return;
    }
    const std::string id = state_->books[*index].id;
    if (const auto directory = state_->storage.book_directory(folder_id)) {
        std::error_code error;
        std::filesystem::remove_all(*directory, error);
    }
    for (BookShelf& shelf : state_->shelves) {
        shelf.book_ids.erase(std::remove(shelf.book_ids.begin(), shelf.book_ids.end(), id),
                             shelf.book_ids.end());
    }
    state_->storage.save_shelves(state_->shelves);
    state_->manual_order.erase(
        std::remove(state_->manual_order.begin(), state_->manual_order.end(), id),
        state_->manual_order.end());
    state_->storage.save_book_order(state_->manual_order);
    refresh();
}

void LibraryViewModel::markRead(const QString& folder) {
    const std::string folder_id = folder.toStdString();
    const auto info = state_->storage.load_book_info(folder_id);
    if (!info.has_value()) {
        return;
    }
    int chapter = 0;
    for (const auto& [path, chapter_info] : info->chapter_info) {
        (void)path;
        if (chapter_info.spine_index.has_value()) {
            chapter = std::max(chapter, *chapter_info.spine_index);
        }
    }
    Bookmark bookmark;
    bookmark.chapter_index = chapter;
    bookmark.progress = 1.0;
    bookmark.character_count = info->character_count;
    bookmark.last_modified = books::reference_now();
    state_->storage.save_bookmark(folder_id, bookmark);
    refresh();
}

QString LibraryViewModel::createShelf(const QString& name) {
    const std::string trimmed = trim(name.toStdString());
    if (trimmed.empty()) {
        return {};
    }
    for (const BookShelf& shelf : state_->shelves) {
        if (same_text(shelf.name, trimmed)) {
            return {};
        }
    }
    state_->shelves.push_back(BookShelf{trimmed, {}});
    state_->storage.save_shelves(state_->shelves);
    refresh();
    return QString::fromStdString(trimmed);
}

bool LibraryViewModel::renameShelf(const QString& from, const QString& to) {
    const std::string trimmed = trim(to.toStdString());
    if (trimmed.empty()) {
        return false;
    }
    for (const BookShelf& shelf : state_->shelves) {
        if (shelf.name != from.toStdString() && same_text(shelf.name, trimmed)) {
            return false;
        }
    }
    for (BookShelf& shelf : state_->shelves) {
        if (shelf.name == from.toStdString()) {
            shelf.name = trimmed;
            state_->storage.save_shelves(state_->shelves);
            if (state_->selected == "shelf:" + from.toStdString()) {
                state_->selected = "shelf:" + trimmed;
                emit selectionChanged();
            }
            refresh();
            return true;
        }
    }
    return false;
}

void LibraryViewModel::deleteShelf(const QString& name) {
    const std::string target = name.toStdString();
    state_->shelves.erase(std::remove_if(state_->shelves.begin(), state_->shelves.end(),
                                         [&target](const BookShelf& shelf) {
                                             return shelf.name == target;
                                         }),
                          state_->shelves.end());
    state_->storage.save_shelves(state_->shelves);
    if (state_->selected == "shelf:" + target) {
        state_->selected = "all";
        emit selectionChanged();
    }
    refresh();
}

void LibraryViewModel::moveBookToShelf(const QString& folder, const QString& shelf) {
    moveBooksToShelf({folder}, shelf);
}

void LibraryViewModel::moveBooksToShelf(const QStringList& folders, const QString& shelf) {
    const std::string target = shelf.toStdString();
    for (const QString& folder : folders) {
        const auto index = state_->find_book(folder.toStdString());
        if (!index.has_value()) {
            continue;
        }
        const std::string id = state_->books[*index].id;
        for (BookShelf& candidate : state_->shelves) {
            candidate.book_ids.erase(
                std::remove(candidate.book_ids.begin(), candidate.book_ids.end(), id),
                candidate.book_ids.end());
        }
        for (BookShelf& candidate : state_->shelves) {
            if (!target.empty() && candidate.name == target) {
                candidate.book_ids.push_back(id);
            }
        }
    }
    state_->storage.save_shelves(state_->shelves);
    refresh();
}

}  // namespace niratan::viewmodels
