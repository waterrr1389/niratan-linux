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

}  // namespace niratan::viewmodels
