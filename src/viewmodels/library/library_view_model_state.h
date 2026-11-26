// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "models/books/library_model.h"
#include "services/books/book_storage.h"
#include "viewmodels/library/library_view_model.h"

namespace niratan::viewmodels {

struct LibraryViewModel::State {
    State();

    books::BookStorage storage;
    books::Library library;
    std::string selected = "all";
    books::LibraryPrefs prefs;
    QString error;
    bool import_error = false;
    QVariantList books_view;
    QVariantList shelves_view;

    books::LibraryShelfSelection selection() const;
    std::optional<std::size_t> find_book(const std::string& folder) const;
    books::SortOption sort_option() const;
    void load();
    void rebuild_views();
};

}  // namespace niratan::viewmodels
