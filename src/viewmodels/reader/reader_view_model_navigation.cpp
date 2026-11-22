// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reader chapter navigation, following NativeMac/NativeReaderView.swift
// (nextChapter, previousChapter, jumpToCharacter, jumpToChapter,
// loadChapterForSasayaki, establishProgrammaticDestination, persistBookmark and
// resetTrackingBaseline) and SasayakiPlayer.navigateToCue's chapter load.
//
// The navigation functions here split from reader_view_model.cpp along the
// chapter-load seam: page turns, contents entries and jumps that the reader
// starts emit chapterTransitionStarted so the Sasayaki player pauses, while
// goToChapterAt loads a cue's destination without that signal because its
// caller has already paused. Every programmatic load flushes statistics at the
// old position, writes the destination bookmark before the restore and resets
// the statistics baseline at the destination: the reset happens at the restore
// through jump_pending, as the other jump paths do.

#include "reader_view_model.h"

#include <QVariant>

#include <algorithm>

#include "reader_view_model_state.h"

namespace niratan::viewmodels {
namespace {

double clamp_progress(double progress) {
    return std::min(std::max(progress, 0.0), 1.0);
}

}  // namespace

void ReaderViewModel::goToChapter(int index) {
    if (!state_->session.has_value()) {
        return;
    }
    if (index < 0 || index >= state_->session->spine_count() || index == state_->chapter_index) {
        return;
    }
    emit chapterTransitionStarted();
    persist_bookmark();
    save_statistics();
    load_chapter(index, 0.0);
}

void ReaderViewModel::goToContentsEntry(int index, const QString& fragment) {
    if (!state_->session || index < 0 || index >= state_->session->spine_count()) {
        return;
    }
    persist_bookmark();
    save_statistics();
    emit chapterTransitionStarted();
    state_->jump_pending = true;
    load_chapter(index, 0.0, fragment);
}

void ReaderViewModel::goToProgress(double progress) {
    if (!state_->session.has_value()) {
        return;
    }
    jumpToCharacter(
        static_cast<int>(clamp_progress(progress) * state_->session->info().character_count));
}

void ReaderViewModel::jumpToCharacter(int character) {
    if (!state_->session) {
        return;
    }
    if (const auto position = state_->session->position_for_character(character)) {
        persist_bookmark();
        save_statistics();
        emit chapterTransitionStarted();
        state_->jump_pending = true;
        load_chapter(position->first, position->second);
    }
}

void ReaderViewModel::goToChapterAt(int index, double progress) {
    if (!state_->session.has_value()) {
        return;
    }
    if (index < 0 || index >= state_->session->spine_count()) {
        return;
    }
    start_tracking_on_navigation();
    save_statistics();
    state_->chapter_index = index;
    state_->chapter_progress = clamp_progress(progress);
    state_->jump_pending = true;
    persist_bookmark();
    load_chapter(index, state_->chapter_progress);
}

int ReaderViewModel::chapterCharacterCount(int index) const {
    if (!state_->session.has_value()) {
        return 0;
    }
    const int start = state_->session->character_offset(index, 0.0);
    const int end = state_->session->character_offset(index, 1.0);
    return end - start;
}

void ReaderViewModel::handlePageTurnResult(bool forward, const QVariant& result) {
    if (!state_->session.has_value()) {
        return;
    }
    // NativeReaderView's onPageTurn: Page Turn mode starts here, before the
    // result moves the position.
    start_tracking_on_navigation();
    if (result.typeId() == QMetaType::Double || result.typeId() == QMetaType::Int) {
        updateChapterProgress(result.toDouble());
        return;
    }
    if (result.toString() == QStringLiteral("limit")) {
        if (forward) {
            if (state_->chapter_index + 1 < state_->session->spine_count()) {
                emit chapterTransitionStarted();
                // Moving to the next chapter's start is the last page's
                // forward jump, so the characters read up to the boundary are
                // credited before the session is stored.
                if (state_->tracker) {
                    state_->tracker->update_position(
                        state_->session->character_offset(state_->chapter_index + 1, 0.0),
                        now_milliseconds());
                }
                persist_bookmark();
                save_statistics();
                load_chapter(state_->chapter_index + 1, 0.0);
            }
        } else if (state_->chapter_index > 0) {
            emit chapterTransitionStarted();
            if (state_->tracker) {
                state_->tracker->update_position(
                    state_->session->character_offset(state_->chapter_index - 1, 1.0),
                    now_milliseconds());
            }
            persist_bookmark();
            save_statistics();
            load_chapter(state_->chapter_index - 1, 1.0);
        }
    }
}

}  // namespace niratan::viewmodels
