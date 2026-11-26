// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/player/subtitle_gap_playback.h"

#include "services/player/playback_controls.h"
#include "services/player/subtitle_document.h"

#include <algorithm>
#include <cmath>

namespace niratan::player {
namespace {

constexpr double kDifferenceEpsilon = 0.001;
constexpr double kMinimumSpeed = 1.1;
constexpr double kMaximumSpeed = 5.0;

// normalizedSubtitleGapFastForwardSpeed: finite values clamp to the configured
// range, then take the playback increment; non-finite values fall back to 2.7.
double normalized_configured_speed(double speed) {
    if (!std::isfinite(speed)) {
        return SubtitleGapPlayback::kDefaultSpeed;
    }
    return normalized_speed(std::clamp(speed, kMinimumSpeed, kMaximumSpeed));
}

}  // namespace

bool SubtitleGapPlayback::enabled() const {
    return enabled_;
}

double SubtitleGapPlayback::configured_speed() const {
    return configured_speed_;
}

bool SubtitleGapPlayback::accelerating() const {
    return base_speed_.has_value();
}

std::optional<double> SubtitleGapPlayback::set_enabled(bool enabled, double engine_speed) {
    if (enabled_ == enabled) {
        return std::nullopt;
    }
    enabled_ = enabled;
    return enabled ? std::nullopt : restore(engine_speed);
}

std::optional<double> SubtitleGapPlayback::set_configured_speed(double speed, double engine_speed) {
    configured_speed_ = normalized_configured_speed(speed);
    if (base_speed_ && std::abs(engine_speed - configured_speed_) >= kDifferenceEpsilon) {
        return configured_speed_;
    }
    return std::nullopt;
}

std::optional<double> SubtitleGapPlayback::update(const SubtitleCueStore& store,
                                                  double playback_time, double subtitle_delay,
                                                  bool playing, double engine_speed) {
    if (!enabled_ || !playing || !in_eligible_gap(store, playback_time, subtitle_delay)) {
        return restore(engine_speed);
    }
    if (!base_speed_) {
        base_speed_ = engine_speed;
    }
    if (std::abs(engine_speed - configured_speed_) >= kDifferenceEpsilon) {
        return configured_speed_;
    }
    return std::nullopt;
}

std::optional<double> SubtitleGapPlayback::set_user_speed(double speed, double /*engine_speed*/) {
    const double normalized = normalized_speed(speed);
    if (base_speed_) {
        base_speed_ = normalized;
        return std::nullopt;
    }
    return normalized;
}

std::optional<double> SubtitleGapPlayback::adjust_user_speed(double delta, double engine_speed) {
    return set_user_speed(user_speed(engine_speed) + delta, engine_speed);
}

std::optional<double> SubtitleGapPlayback::restore(double engine_speed) {
    if (!base_speed_) {
        return std::nullopt;
    }
    const double normalized = normalized_speed(*base_speed_);
    base_speed_.reset();
    if (std::abs(engine_speed - normalized) >= kDifferenceEpsilon) {
        return normalized;
    }
    return std::nullopt;
}

double SubtitleGapPlayback::user_speed(double engine_speed) const {
    return base_speed_.value_or(engine_speed);
}

double SubtitleGapPlayback::persistence_speed(double engine_speed) const {
    return base_speed_.value_or(engine_speed);
}

bool SubtitleGapPlayback::in_eligible_gap(const SubtitleCueStore& store, double playback_time,
                                          double subtitle_delay) const {
    const double adjusted = playback_time - subtitle_delay;
    const SubtitleCueSlice slice = store.slice_at(adjusted);
    if (!slice.showing.isEmpty() || slice.last_shown.isEmpty() || slice.next_to_show.isEmpty()) {
        return false;
    }
    double previous_end = slice.last_shown.first().end_time;
    for (const SubtitleCue& cue : slice.last_shown) {
        previous_end = std::max(previous_end, cue.end_time);
    }
    double next_start = slice.next_to_show.first().start_time;
    for (const SubtitleCue& cue : slice.next_to_show) {
        next_start = std::min(next_start, cue.start_time);
    }
    return adjusted - previous_end > kEdgeGuard && next_start - adjusted > kEdgeGuard;
}

}  // namespace niratan::player
