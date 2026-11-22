// SPDX-License-Identifier: GPL-3.0-or-later
//
// Transport, cue display, cue following and position persistence of
// SasayakiViewModel, following Features/Sasayaki/SasayakiPlayer.swift
// (playCue(from:stop:), tick, seek, startPlayback, pausePlayback,
// persistPlaybackPosition, flushPlayback, togglePlayback, skip(forward:),
// seekRelative, nextCue, prevCue, navigateToCue, prepareTransition,
// handleRestoreCompleted, updateCue, displayCue, clearDisplayedCue and the rate
// and delay didSet observers) and the pure decisions in
// sasayaki_player_policy.h.
//
// A position tick applies the landing options of the seek in flight, then the
// stop time, the whole-second write and the cue lookup, in that order. A seek
// persists its target before moving the player; the timer only drops a target
// that never landed. The active cue is the displayed cue, so cue navigation
// uses its start rather than a new lookup, as Swift does.
//
// A cue in another chapter is remembered as the pending cue, the reader loads
// that chapter at the cue's progress and restoreCompleted() displays it, with
// the reveal and resume rules of handleRestoreCompleted. The displayed cue is
// only replaced when the cue belongs to the open chapter or the following
// rules allow a chapter load.

#include "sasayaki_view_model.h"

#include <QMediaPlayer>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "sasayaki_player_policy.h"
#include "sasayaki_view_model_state.h"
#include "services/books/book_storage.h"
#include "services/sasayaki/matcher.h"
#include "services/storage/app_paths.h"
#include "viewmodels/reader/reader_view_model.h"

namespace niratan::viewmodels {
namespace {

using niratan::sasayaki::SasayakiCue;
using niratan::sasayaki::SasayakiMatch;
using niratan::sasayaki::SasayakiMatchData;
using niratan::sasayaki::SasayakiPlaybackData;

const SasayakiCue* find_cue(const std::vector<SasayakiCue>& cues, const QString& id) {
    if (id.isEmpty()) {
        return nullptr;
    }
    for (const SasayakiCue& cue : cues) {
        if (QString::fromStdString(cue.id) == id) {
            return &cue;
        }
    }
    return nullptr;
}

niratan::books::BookStorage book_storage() {
    return niratan::books::BookStorage(niratan::storage::data_root());
}

}  // namespace

void SasayakiViewModel::play() {
    if (!state_->has_audio) {
        set_error(tr("Load an audio file first."));
        return;
    }
    state_->player->setPlaybackRate(state_->rate);
    state_->player->play();
    state_->has_played_once = true;
}

void SasayakiViewModel::pause() {
    if (!state_->has_audio) {
        return;
    }
    state_->player->pause();
    persist_position(static_cast<double>(state_->player->position()) / 1000.0);
}

void SasayakiViewModel::togglePlayback() {
    if (state_->playing) {
        pause();
        return;
    }
    play();
    if (state_->auto_scroll && !state_->current_cue_id.isEmpty()) {
        emit activeCueChanged(state_->current_cue_id, true);
    }
}

void SasayakiViewModel::seek(double seconds) {
    if (!std::isfinite(seconds)) {
        return;
    }
    seek(clamped_seek_target(seconds, state_->duration), false, true, std::nullopt);
}

void SasayakiViewModel::seek(double seconds, bool start_playback, bool update_cue,
                             std::optional<double> stop_playback_time) {
    if (!state_->has_audio || !std::isfinite(seconds)) {
        return;
    }
    state_->pending_seek = seconds;
    state_->seek_start_playback = start_playback;
    state_->seek_update_cue = update_cue;
    state_->seek_stop_time = stop_playback_time;
    state_->seek_timer->start(1500);
    persist_position(seconds);
    state_->player->setPosition(static_cast<qint64>(std::llround(seconds * 1000.0)));
}

void SasayakiViewModel::skipForward(bool forward) {
    if (!state_->has_audio) {
        return;
    }
    state_->stop_playback_time.reset();
    seek(clamped_skip_target(state_->position, state_->duration, forward), false, true,
         std::nullopt);
}

void SasayakiViewModel::previousCue() {
    if (!state_->has_audio || state_->timeline.empty()) {
        return;
    }
    state_->stop_playback_time.reset();
    const SasayakiCue* current = find_cue(state_->timeline.cues(), state_->current_cue_id);
    const double reference =
        cue_reference_time(current != nullptr, current != nullptr ? current->start_time : 0.0,
                           state_->position - state_->delay, true);
    const SasayakiCue* previous = state_->timeline.previous_cue_before(reference);
    if (previous == nullptr) {
        seek(0.0);
        return;
    }
    navigate_to_cue(*previous);
}

void SasayakiViewModel::nextCue() {
    if (!state_->has_audio || state_->timeline.empty()) {
        return;
    }
    state_->stop_playback_time.reset();
    const SasayakiCue* current = find_cue(state_->timeline.cues(), state_->current_cue_id);
    const double reference =
        cue_reference_time(current != nullptr, current != nullptr ? current->start_time : 0.0,
                           state_->position - state_->delay, false);
    const SasayakiCue* next = state_->timeline.next_cue_after(reference);
    if (next == nullptr) {
        return;
    }
    navigate_to_cue(*next);
}

void SasayakiViewModel::replayCue() {
    if (!state_->has_audio || state_->timeline.empty()) {
        return;
    }
    const SasayakiCue* current = state_->timeline.cue_at(state_->position - state_->delay);
    if (current == nullptr) {
        seek(0.0);
        return;
    }
    navigate_to_cue(*current);
}

void SasayakiViewModel::jumpCue(const QString& cueId) {
    if (!state_->has_audio || state_->timeline.empty()) {
        return;
    }
    if (const SasayakiCue* cue = find_cue(state_->timeline.cues(), cueId)) {
        navigate_to_cue(*cue);
    }
}

void SasayakiViewModel::playCue(const QString& cueId, bool stop) {
    if (!state_->has_audio) {
        return;
    }
    const SasayakiCue* cue = find_cue(state_->timeline.cues(), cueId);
    if (cue == nullptr) {
        return;
    }
    state_->stop_playback_time.reset();
    if (state_->playing) {
        pause();
    }
    seek(cue->start_time + state_->delay, true, false,
         play_cue_stop_time(cue->end_time, state_->delay, stop));
}

bool SasayakiViewModel::pauseForLookup() {
    if (!pause_for_lookup(state_->playing, state_->auto_pause)) {
        return false;
    }
    pause();
    return true;
}

void SasayakiViewModel::resumeAfterLookup(bool was_paused_by_lookup) {
    if (resume_after_lookup(was_paused_by_lookup, state_->playing)) {
        togglePlayback();
    }
}

void SasayakiViewModel::flushPlayback() {
    std::optional<double> player_seconds;
    if (state_->has_audio && state_->player != nullptr) {
        const double seconds = static_cast<double>(state_->player->position()) / 1000.0;
        if (std::isfinite(seconds)) {
            player_seconds = seconds;
        }
    }
    const std::optional<double> target =
        flush_position(state_->pending_seek, state_->playing, player_seconds, state_->position);
    if (!target.has_value()) {
        return;
    }
    if (state_->pending_seek.has_value()) {
        persist_position(*target);
        return;
    }
    state_->position = std::max(0.0, *target);
    emit positionChanged();
    update_audiobook_chapter();
    save_playback(state_->position);
}

void SasayakiViewModel::setRate(double rate) {
    if (!std::isfinite(rate)) {
        return;
    }
    const double clamped = clamp_rate(rate);
    if (clamped == state_->rate) {
        return;
    }
    state_->rate = clamped;
    state_->player->setPlaybackRate(clamped);
    emit rateChanged();
    save_playback(state_->position);
}

void SasayakiViewModel::setDelay(double delay) {
    if (!std::isfinite(delay)) {
        return;
    }
    const double clamped = clamp_delay(delay);
    if (clamped == state_->delay) {
        return;
    }
    state_->delay = clamped;
    emit delayChanged();
    save_playback(state_->position);
    update_cue();
}

void SasayakiViewModel::update_cue(bool refresh_highlight) {
    const SasayakiCue* cue = nullptr;
    if (state_->has_audio && !state_->timeline.empty()) {
        cue = state_->timeline.cue_at(state_->position - state_->delay);
    }
    const SasayakiMatch* match =
        cue != nullptr ? find_match_by_id(state_->match, cue->id) : nullptr;
    ReaderViewModel* reader = ReaderViewModel::instance();
    const CueFollowDecision decision =
        cue_follow_decision(state_->has_audio, state_->has_match, state_->chapter_transition,
                            cue != nullptr ? cue->id : std::string{},
                            match != nullptr ? match->chapter_index : reader->chapter_index(),
                            state_->current_cue_id.toStdString(), reader->chapter_index(),
                            state_->auto_scroll, state_->has_played_once);

    switch (decision.action) {
        case CueFollowAction::none:
            break;
        case CueFollowAction::clear:
            clear_displayed_cue();
            break;
        case CueFollowAction::display:
            display_cue(*cue, decision.reveal);
            break;
        case CueFollowAction::follow:
            // The restore displays the pending cue with the reveal and resume
            // rules of handleRestoreCompleted.
            state_->pending_cue = *match;
            prepareTransition();
            reader->goToChapterAt(match->chapter_index,
                                  niratan::sasayaki::reader_progress(
                                      *match, reader->chapterCharacterCount(match->chapter_index)));
            break;
    }

    if (refresh_highlight) {
        // Install new ranges even if the playback cue still has the same ID.
        emit matchApplied();
        emit activeCueChanged(state_->current_cue_id,
                              follows_playback(state_->auto_scroll, state_->has_played_once));
    }
}

void SasayakiViewModel::prepareTransition() {
    state_->should_resume = transition_resumes(state_->playing);
    state_->chapter_transition = true;
    state_->stop_playback_time.reset();
    clear_displayed_cue();
    if (state_->playing) {
        pause();
    }
}

void SasayakiViewModel::restoreCompleted(int currentIndex) {
    std::optional<std::string> pending_id;
    int pending_chapter = 0;
    if (state_->pending_cue.has_value()) {
        pending_id = state_->pending_cue->id;
        pending_chapter = state_->pending_cue->chapter_index;
    }

    std::optional<std::string> timeline_id;
    int timeline_chapter = 0;
    if (const SasayakiCue* active = state_->timeline.cue_at(state_->position - state_->delay)) {
        if (const SasayakiMatch* match = find_match_by_id(state_->match, active->id)) {
            timeline_id = active->id;
            timeline_chapter = match->chapter_index;
        }
    }

    const RestoreCueDecision decision = restore_cue_decision(
        state_->has_match, state_->chapter_transition, pending_id, pending_chapter,
        state_->reveal_pending_cue_on_restore, state_->should_resume, state_->auto_scroll,
        currentIndex, timeline_id, timeline_chapter);
    if (!decision.handled) {
        return;
    }

    state_->chapter_transition = false;
    state_->should_resume = false;
    state_->reveal_pending_cue_on_restore = false;
    state_->pending_cue.reset();

    const SasayakiCue* cue = decision.cue_id.has_value()
        ? find_cue(state_->timeline.cues(), QString::fromStdString(*decision.cue_id))
        : nullptr;
    if (cue != nullptr) {
        display_cue(*cue, decision.reveal);
    } else {
        clear_displayed_cue();
    }

    if (decision.resume) {
        play();
    }
}

void SasayakiViewModel::display_cue(const SasayakiCue& cue, bool reveal) {
    state_->current_cue_id = QString::fromStdString(cue.id);
    state_->current_cue_text = QString::fromStdString(cue.text);
    emit currentCueChanged();
    emit activeCueChanged(state_->current_cue_id, reveal);
}

void SasayakiViewModel::clear_displayed_cue() {
    if (state_->current_cue_id.isEmpty() && state_->current_cue_text.isEmpty()) {
        return;
    }
    state_->current_cue_id.clear();
    state_->current_cue_text.clear();
    emit currentCueChanged();
    emit activeCueChanged(QString(), false);
}

void SasayakiViewModel::position_tick(double seconds) {
    if (state_->pending_seek.has_value()) {
        if (!tick_lands(seconds, state_->pending_seek)) {
            return;
        }
        state_->pending_seek.reset();
        state_->seek_timer->stop();
        state_->stop_playback_time = std::exchange(state_->seek_stop_time, std::nullopt);
        if (!state_->seek_update_cue) {
            state_->position = std::max(0.0, seconds);
            emit positionChanged();
            update_audiobook_chapter();
            if (std::exchange(state_->seek_start_playback, false)) {
                play();
            }
            return;
        }
    }
    apply_position(seconds);
    if (std::exchange(state_->seek_start_playback, false)) {
        play();
    }
}

void SasayakiViewModel::apply_position(double seconds) {
    if (!std::isfinite(seconds)) {
        return;
    }
    state_->position = std::max(0.0, seconds);
    emit positionChanged();
    update_audiobook_chapter();

    if (stop_reached(state_->stop_playback_time, state_->position)) {
        state_->stop_playback_time.reset();
        if (state_->playing) {
            pause();
        }
    }

    const std::optional<double> written = persisted_second(state_->position, state_->last_second);
    if (written.has_value()) {
        state_->last_second = static_cast<int>(std::floor(state_->position));
        save_playback(*written);
    }
    update_cue();
}

void SasayakiViewModel::persist_position(double seconds) {
    if (!std::isfinite(seconds)) {
        return;
    }
    state_->position = std::max(0.0, seconds);
    state_->last_second = static_cast<int>(std::floor(state_->position));
    emit positionChanged();
    update_audiobook_chapter();
    save_playback(state_->position);
}

void SasayakiViewModel::save_playback(double position) {
    if (!state_->has_root) {
        return;
    }
    SasayakiPlaybackData data;
    data.last_position = position;
    data.delay = state_->delay;
    data.rate = static_cast<float>(state_->rate);
    data.audio_bookmark = state_->playback.audio_bookmark;
    book_storage().save_sasayaki_playback(state_->folder, data);
}

void SasayakiViewModel::navigate_to_cue(const SasayakiCue& cue) {
    navigate_to_cue(cue, false);
}

void SasayakiViewModel::navigate_to_cue(const SasayakiCue& cue, bool start_playback) {
    if (!state_->has_audio) {
        return;
    }
    state_->stop_playback_time.reset();
    const double target = cue.start_time + state_->delay;
    const SasayakiMatch* match = find_match_by_id(state_->match, cue.id);
    ReaderViewModel* reader = ReaderViewModel::instance();
    const int cue_chapter = match != nullptr ? match->chapter_index : reader->chapter_index();
    if (!cue_needs_chapter_load(cue_chapter, reader->chapter_index())) {
        seek(target, start_playback, true, std::nullopt);
        return;
    }

    const bool should_start_after_restore = starts_after_restore(state_->playing, start_playback);
    state_->pending_cue = *match;
    state_->reveal_pending_cue_on_restore = true;
    prepareTransition();
    reader->goToChapterAt(match->chapter_index,
                          niratan::sasayaki::reader_progress(
                              *match, reader->chapterCharacterCount(match->chapter_index)));
    state_->current_cue_id = QString::fromStdString(cue.id);
    state_->current_cue_text = QString::fromStdString(cue.text);
    emit currentCueChanged();
    seek(target, false, false, std::nullopt);
    if (should_start_after_restore) {
        state_->should_resume = true;
    }
}

}  // namespace niratan::viewmodels
