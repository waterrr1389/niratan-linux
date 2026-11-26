// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_view_model_state.h"

#include <algorithm>
#include <cmath>

namespace niratan::viewmodels {

bool ReaderViewModel::bookmark_edited() const {
    return state_->bookmark_edited;
}

// NativeReaderView.reloadBookmark: the stored chapter and progress replace the
// open position, the statistics history is re-read, and the restored position
// anchors the tracking baseline. Nothing is written and the jump credits no
// reading.
void ReaderViewModel::reloadBookmark() {
    if (!state_->session || !state_->has_book) {
        return;
    }
    if (const auto bookmark = state_->storage.load_bookmark(state_->book.folder)) {
        const int last = std::max(state_->session->spine_count() - 1, 0);
        state_->chapter_index = std::min(std::max(bookmark->chapter_index, 0), last);
        state_->chapter_progress =
            std::isfinite(bookmark->progress) ? std::clamp(bookmark->progress, 0.0, 1.0) : 0.0;
    }
    if (state_->tracker) {
        const std::int64_t now = now_milliseconds();
        state_->tracker->reload(now);
        // resetTrackingBaseline at the restored position: the jump credits no
        // characters and the interval before the reload no time.
        state_->tracker->reset_position(character_offset(), now);
    }
    state_->fragment.clear();
    state_->reveal_cue_after_restore = true;
    reload_document(true);
    emit chapterChanged();
    emit progressChanged();
    refresh_statistics(true);
}

void ReaderViewModel::readerSyncFinished(const QString& folder, bool imported) {
    if (!state_->has_book || !imported || folder != book_folder()) {
        return;
    }
    reloadBookmark();
    emit audiobookReloadRequested();
}

// NativeReaderView.prepareForReaderLifecycleClose and runAutoExport's pending
// flag: the close persists the bookmark and statistics, then consumes the
// pending export so a later close without another bookmark write sends
// nothing. The book stays open for a reopened window.
void ReaderViewModel::prepareForClose() {
    if (!state_->has_book) {
        return;
    }
    persist_bookmark();
    save_statistics();
    if (state_->bookmark_edited) {
        state_->bookmark_edited = false;
        emit bookmarkEditedChanged();
    }
}

}  // namespace niratan::viewmodels
