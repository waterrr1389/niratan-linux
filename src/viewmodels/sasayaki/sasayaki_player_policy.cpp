// SPDX-License-Identifier: GPL-3.0-or-later

#include "sasayaki_player_policy.h"

#include <algorithm>
#include <cmath>

namespace niratan::viewmodels {

bool tick_lands(double seconds, std::optional<double> pending) {
    return !pending.has_value() || std::abs(seconds - *pending) <= kSeekLandingTolerance;
}

bool stop_reached(std::optional<double> stop_time, double seconds) {
    return stop_time.has_value() && seconds >= *stop_time;
}

std::optional<double> persisted_second(double seconds, int last_second) {
    if (static_cast<int>(std::floor(seconds)) == last_second) {
        return std::nullopt;
    }
    return seconds;
}

std::optional<double> flush_position(std::optional<double> pending, bool playing,
                                     std::optional<double> player_seconds, double current_time) {
    if (pending.has_value()) {
        return pending;
    }
    if (!playing) {
        return std::nullopt;
    }
    if (player_seconds.has_value() && std::isfinite(*player_seconds)) {
        return player_seconds;
    }
    return current_time;
}

std::optional<double> play_cue_stop_time(double end_time, double delay, bool stop) {
    if (!stop) {
        return std::nullopt;
    }
    return end_time + delay;
}

bool follows_playback(bool auto_scroll, bool has_played_once) {
    return auto_scroll && has_played_once;
}

bool pause_for_lookup(bool playing, bool auto_pause) {
    return playing && auto_pause;
}

bool resume_after_lookup(bool was_paused_by_lookup, bool playing) {
    return was_paused_by_lookup && !playing;
}

double clamped_skip_target(double position, double duration, bool forward) {
    if (!forward) {
        return std::max(0.0, position - kSasayakiSkipInterval);
    }
    if (duration > 0.0) {
        return std::min(position + kSasayakiSkipInterval, duration);
    }
    return position + kSasayakiSkipInterval;
}

double clamped_seek_target(double seconds, double duration) {
    if (duration > 0.0) {
        return std::clamp(seconds, 0.0, duration);
    }
    return std::max(0.0, seconds);
}

double clamp_rate(double rate) {
    return std::clamp(rate, kMinimumRate, kMaximumRate);
}

double clamp_delay(double delay) {
    return std::clamp(delay, kMinimumDelay, kMaximumDelay);
}

double cue_reference_time(bool has_current, double current_start, double lookup, bool previous) {
    if (has_current) {
        return current_start;
    }
    return previous ? std::max(0.0, lookup) : lookup;
}

bool cue_needs_chapter_load(int cue_chapter, int current_chapter) {
    return cue_chapter != current_chapter;
}

bool starts_after_restore(bool playing, bool start_playback) {
    return playing || start_playback;
}

bool transition_resumes(bool playing) {
    return playing;
}

RestoreCueDecision restore_cue_decision(bool has_match, bool chapter_transition,
                                        const std::optional<std::string>& pending_id,
                                        int pending_chapter, bool reveal_pending,
                                        bool should_resume, bool auto_scroll, int current_index,
                                        const std::optional<std::string>& timeline_id,
                                        int timeline_chapter) {
    if (!has_match) {
        return {};
    }

    RestoreCueDecision decision;
    decision.handled = true;
    if (chapter_transition && pending_id.has_value() && pending_chapter == current_index) {
        decision.cue_id = pending_id;
        decision.reveal = reveal_pending || (should_resume && auto_scroll);
    } else if (timeline_id.has_value() && timeline_chapter == current_index) {
        decision.cue_id = timeline_id;
        decision.reveal = should_resume && auto_scroll;
    }
    decision.resume = should_resume;
    return decision;
}

CueFollowDecision cue_follow_decision(bool has_audio, bool has_match, bool chapter_transition,
                                      const std::string& cue_id, int cue_chapter,
                                      const std::string& current_id, int current_chapter,
                                      bool auto_scroll, bool has_played_once) {
    if (!has_audio || !has_match || chapter_transition) {
        return {};
    }
    if (cue_id.empty()) {
        return {CueFollowAction::clear, false};
    }
    if (cue_id == current_id) {
        return {};
    }
    if (cue_chapter == current_chapter) {
        return {CueFollowAction::display, auto_scroll && has_played_once};
    }
    if (auto_scroll && has_played_once) {
        return {CueFollowAction::follow, false};
    }
    return {CueFollowAction::clear, false};
}

std::optional<std::size_t> cue_index_at_offset(const std::vector<sasayaki::SasayakiMatch>& matches,
                                               int chapter, int offset) {
    std::size_t low = 0;
    std::size_t high = matches.size();
    while (low < high) {
        const std::size_t mid = low + (high - low) / 2;
        const sasayaki::SasayakiMatch& match = matches[mid];
        if (match.chapter_index < chapter ||
            (match.chapter_index == chapter && match.start + match.length <= offset)) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (low < matches.size() && matches[low].chapter_index == chapter &&
        matches[low].start <= offset) {
        return low;
    }
    return std::nullopt;
}

}  // namespace niratan::viewmodels
