// SPDX-License-Identifier: GPL-3.0-or-later

#include "viewmodels/library/library_view_model.h"
#include "viewmodels/library/library_view_model_state.h"

#include <algorithm>

namespace niratan::viewmodels {

bool LibraryViewModel::reorderBook(const QString& source, const QString& target) {
    const auto selection = state_->selection();
    if (source == target || selection.shelf == books::LibraryShelf::reading) {
        return false;
    }
    const auto source_index = state_->find_book(source.toStdString());
    const auto target_index = state_->find_book(target.toStdString());
    if (!source_index || !target_index) {
        return false;
    }
    const auto& source_id = state_->library.books[*source_index].metadata.id;
    const auto& target_id = state_->library.books[*target_index].metadata.id;
    const auto displayed = [this](const QString& folder) {
        return std::ranges::any_of(state_->books_view, [&folder](const QVariant& row) {
            return row.toMap().value(QStringLiteral("folder")).toString() == folder;
        });
    };
    if (source_id == target_id || !displayed(source) || !displayed(target)) {
        return false;
    }

    // ShelfView captures the displayed section before setting Manual. Its
    // order supplies any missing IDs, not the basis for the actual move.
    const auto previous_sort = state_->sort_option();
    const bool sort_changed = previous_sort != books::SortOption::manual;
    if (sort_changed) {
        state_->prefs.sort_option = static_cast<int>(books::SortOption::manual);
        state_->storage.save_library_prefs(state_->prefs);
    }
    books::move_book_before(state_->library, selection, previous_sort, source_id, target_id);
    const bool named = selection.shelf == books::LibraryShelf::shelf;
    const bool saved = named ? state_->storage.save_shelves(state_->library.shelves)
                             : state_->storage.save_book_order(state_->library.manual_order);
    state_->rebuild_views();
    emit booksChanged();
    if (sort_changed) {
        emit sortChanged();
    }
    if (!saved) {
        state_->import_error = false;
        state_->error = tr("Could not save the book order.");
        emit errorChanged();
    }
    return true;
}

bool LibraryViewModel::moveShelves(const QList<int>& source, int destination) {
    const std::vector<int> offsets(source.begin(), source.end());
    if (!books::move_shelves(state_->library.shelves, offsets, destination)) {
        return false;
    }
    const bool saved = state_->storage.save_shelves(state_->library.shelves);
    state_->rebuild_views();
    emit shelvesChanged();
    if (!saved) {
        state_->import_error = false;
        state_->error = tr("Could not save the shelf order.");
        emit errorChanged();
    }
    return true;
}

}  // namespace niratan::viewmodels
