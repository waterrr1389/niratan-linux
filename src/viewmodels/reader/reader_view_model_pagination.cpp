// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_view_model_state.h"

#include <numeric>

namespace niratan::viewmodels {

int ReaderViewModel::page_count() const {
    const auto& counts = state_->spine_page_counts;
    return std::accumulate(counts.begin(), counts.end(), 0);
}

int ReaderViewModel::page_index() const { return state_->spine_page_offset + state_->page_index; }

int ReaderViewModel::chapter_page_count() const { return state_->page_count; }

void ReaderViewModel::set_pagination(int index, int count) {
    auto& counts = state_->spine_page_counts;
    const auto spine = counts.begin() + (counts.empty() ? 0 : state_->chapter_index);
    if (count > 0) {
        if (spine == counts.end() || (index == state_->page_index && count == state_->page_count)) {
            return;
        }
        *spine = count;
    }

    // Reloads reset the visible page without discarding other measured spines.
    state_->spine_page_offset = std::accumulate(counts.begin(), spine, 0);
    state_->page_index = index;
    state_->page_count = count;
    emit paginationChanged();
    if (count > 0) {
        emit pageChanged(index - 1);
    }
}

}  // namespace niratan::viewmodels
