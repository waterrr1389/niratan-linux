// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cue lookup for the audiobook player, following Features/Sasayaki/
// SasayakiPlayer.swift (CueTimeline: cue(at:), nextCueMatch(after:),
// prevCueMatch(before:) and findCue(_:)).
//
// Cues are kept in ascending start-time order. The constructor sorts them
// stably, which preserves the input order of equal starts; sasayaki_match.json
// is written in start order, so the sort is a no-op for match data but keeps a
// subtitle file that is not ordered behaving like the Swift timeline.
//
// Times are audio seconds. The player applies the playback delay before the
// lookup: the lookup time is position - delay, and a seek target is a cue's
// start + delay.
//
// cue_at(time) returns the first cue whose start lies within 0.01 seconds of
// time, preferring the cue at or after time. Otherwise it returns the cue
// immediately before the insertion point when time has not passed that cue's
// end time, and null when there is no such cue or when it has ended. An empty
// timeline returns null from every lookup. The insertion point never selects a
// cue that starts before time, so only the upper bound of the window can pick
// a cue ahead; a cue that started earlier is reached through the previous-cue
// fallback.
//
// next_cue_after(time) returns the first cue starting strictly after time, so
// an exact start match is skipped. previous_cue_before(time) returns the cue
// before the insertion point of time, so an exact start match yields the
// previous cue rather than the match itself.
//
// Non-finite times are not special-cased. Swift's comparisons are false for
// NaN, which makes every lookup fail; the ordered comparisons here behave the
// same way.

#pragma once

#include <cstddef>
#include <vector>

#include "models/sasayaki/sasayaki.h"

namespace niratan::sasayaki {

class CueTimeline {
   public:
    CueTimeline() = default;
    explicit CueTimeline(std::vector<SasayakiCue> cues);

    const std::vector<SasayakiCue>& cues() const;
    bool empty() const;

    // The cue playing at time, or null.
    const SasayakiCue* cue_at(double time) const;
    // The first cue that starts after time, including one that starts at time
    // only when a later start exists; an exact start is skipped.
    const SasayakiCue* next_cue_after(double time) const;
    // The cue before time's insertion point, which may already have ended.
    const SasayakiCue* previous_cue_before(double time) const;

   private:
    // The first index whose start is not less than time.
    std::size_t find(double time) const;

    std::vector<SasayakiCue> cues_;
};

// The audio position for a cue: its start shifted by the playback delay.
double seek_target(const SasayakiCue& cue, double delay);

}  // namespace niratan::sasayaki
