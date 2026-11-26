// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/books/library_model.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace niratan::books {

bool move_shelves(std::vector<BookShelf>& shelves, const std::vector<int>& source,
                  int destination) {
    if (source.empty() || destination < 0 || std::cmp_greater(destination, shelves.size())) {
        return false;
    }
    std::vector<bool> moved(shelves.size());
    for (int index : source) {
        if (index < 0 || std::cmp_greater_equal(index, shelves.size())) {
            return false;
        }
        moved[static_cast<std::size_t>(index)] = true;
    }

    std::vector<BookShelf> remaining;
    std::vector<BookShelf> selected;
    remaining.reserve(shelves.size());
    auto insertion = static_cast<std::size_t>(destination);
    for (std::size_t index = 0; index < shelves.size(); ++index) {
        if (moved[index]) {
            selected.push_back(std::move(shelves[index]));
            if (std::cmp_less(index, destination)) {
                --insertion;
            }
        } else {
            remaining.push_back(std::move(shelves[index]));
        }
    }
    remaining.insert(remaining.begin() + static_cast<std::ptrdiff_t>(insertion),
                     std::make_move_iterator(selected.begin()),
                     std::make_move_iterator(selected.end()));
    shelves = std::move(remaining);
    return true;
}

}  // namespace niratan::books
