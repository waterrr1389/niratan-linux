// SPDX-License-Identifier: GPL-3.0-or-later

#include "player_study_view_model_state.h"

#include <algorithm>

namespace niratan::viewmodels {

bool PlayerStudyViewModel::has_transcript_rows() const {
    return !state_->cues.isEmpty();
}

bool PlayerStudyViewModel::can_align_previous_subtitle() const {
    return state_->can_align_previous;
}

bool PlayerStudyViewModel::can_align_next_subtitle() const {
    return state_->can_align_next;
}

bool PlayerStudyViewModel::seekPreviousSubtitleCue() {
    const int target = relative_row(-1);
    if (target < 0) {
        return false;
    }
    emit seekRequested(std::max(0.0, state_->cues[target].start + state_->delay));
    return true;
}

bool PlayerStudyViewModel::seekNextSubtitleCue() {
    const int target = relative_row(1);
    if (target < 0) {
        return false;
    }
    emit seekRequested(std::max(0.0, state_->cues[target].start + state_->delay));
    return true;
}

bool PlayerStudyViewModel::alignPreviousSubtitleToCurrentTime() {
    return align_subtitle(player::SubtitleOffsetDirection::previous);
}

bool PlayerStudyViewModel::alignNextSubtitleToCurrentTime() {
    return align_subtitle(player::SubtitleOffsetDirection::next);
}

int PlayerStudyViewModel::relative_row(int offset) const {
    const double time = state_->position - state_->delay;
    int current = state_->active_row(time);
    if (current < 0) {
        if (state_->cues.isEmpty()) {
            return -1;
        }
        const auto next =
            std::lower_bound(state_->cues.cbegin(), state_->cues.cend(), time,
                             [](const Cue& cue, double value) { return cue.start < value; });
        current = int(std::min<qsizetype>(next - state_->cues.cbegin(), state_->cues.size() - 1));
    }
    const int target = current + offset;
    return target >= 0 && target < state_->cues.size() ? target : -1;
}

bool PlayerStudyViewModel::align_subtitle(player::SubtitleOffsetDirection direction) {
    if (!state_->alignment_store) {
        return false;
    }
    const auto delay = state_->alignment_store->delay_aligning_adjacent_cue(
        state_->position, state_->delay, direction);
    if (!delay) {
        return false;
    }
    emit subtitleAlignmentRequested(*delay);
    return true;
}

void PlayerStudyViewModel::update_alignment_availability() {
    const auto available = [this](player::SubtitleOffsetDirection direction) {
        return state_->alignment_store &&
            state_->alignment_store
                ->delay_aligning_adjacent_cue(state_->position, state_->delay, direction)
                .has_value();
    };
    const bool previous = available(player::SubtitleOffsetDirection::previous);
    const bool next = available(player::SubtitleOffsetDirection::next);
    if (state_->can_align_previous == previous && state_->can_align_next == next) {
        return;
    }
    state_->can_align_previous = previous;
    state_->can_align_next = next;
    emit alignmentAvailabilityChanged();
}

}  // namespace niratan::viewmodels
