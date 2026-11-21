// SPDX-License-Identifier: GPL-3.0-or-later

#include "cue_timeline.h"

#include <algorithm>
#include <utility>

namespace niratan::sasayaki {
namespace {

// SasayakiPlayer.CueTimeline.cue(at:) accepts a start within this many seconds
// of the lookup time in either direction.
constexpr double start_tolerance = 0.01;

}  // namespace

CueTimeline::CueTimeline(std::vector<SasayakiCue> cues) : cues_(std::move(cues)) {
    std::stable_sort(cues_.begin(), cues_.end(),
                     [](const SasayakiCue& left, const SasayakiCue& right) {
                         return left.start_time < right.start_time;
                     });
}

const std::vector<SasayakiCue>& CueTimeline::cues() const { return cues_; }

bool CueTimeline::empty() const { return cues_.empty(); }

std::size_t CueTimeline::find(double time) const {
    std::size_t low = 0;
    std::size_t high = cues_.size();
    while (low < high) {
        const std::size_t mid = low + (high - low) / 2;
        if (cues_[mid].start_time < time) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low;
}

const SasayakiCue* CueTimeline::cue_at(double time) const {
    const std::size_t index = find(time);
    if (index < cues_.size()) {
        const double delta = cues_[index].start_time - time;
        if (delta >= -start_tolerance && delta <= start_tolerance) {
            return &cues_[index];
        }
    }
    if (index == 0) {
        return nullptr;
    }
    const SasayakiCue& previous = cues_[index - 1];
    return time <= previous.end_time ? &previous : nullptr;
}

const SasayakiCue* CueTimeline::next_cue_after(double time) const {
    std::size_t index = find(time);
    if (index < cues_.size() && cues_[index].start_time == time) {
        ++index;
    }
    return index < cues_.size() ? &cues_[index] : nullptr;
}

const SasayakiCue* CueTimeline::previous_cue_before(double time) const {
    const std::size_t index = find(time);
    return index > 0 ? &cues_[index - 1] : nullptr;
}

double seek_target(const SasayakiCue& cue, double delay) { return cue.start_time + delay; }

}  // namespace niratan::sasayaki
