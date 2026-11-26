// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fast-forward over subtitle gaps, following VideoPlayerViewModel.swift
// (`updateSubtitleGapPlayback`, `shouldFastForwardSubtitleGap`,
// `restoreSubtitleGapFastForwardSpeedIfNeeded`, `setSpeed`, `adjustSpeed`,
// `currentUserSpeed`, `snapshotForPersistence`,
// `setSubtitleGapFastForwardEnabled`, `setSubtitleGapFastForwardSpeed`,
// `normalizedSubtitleGapFastForwardSpeed`, `seek`, `openPlaylistItem`,
// `shutdown`), VideoPlayerScreen.swift (`updateSubtitleGapPlayback`),
// SubtitleCueStore.swift (`slice`), PlaybackEngine.swift
// (`VideoPlaybackSpeed.normalized`) and Core/UserConfig.swift
// (`videoSubtitleGapFastForwardEnabled`, `videoSubtitleGapFastForwardSpeed`).
//
// The policy holds the durable preference values and the speed that was in use
// when acceleration began. It never calls an engine: every method that may
// change the effective speed returns the value to apply, or nothing when the
// engine already runs it.
//
// A gap qualifies only with the feature on, playback running, no showing cue,
// one cue ending before and one starting after, and adjusted time (playback
// minus subtitle delay) strictly more than 0.6 s from both edges. The first
// qualifying update captures the engine's current speed as the base, even when
// it already equals or exceeds the configured speed; the base survives while
// the gap continues. Leaving the gap, pausing, disabling, seeking, opening
// media and shutdown restore the normalized base and clear it. Manual speed
// changes during acceleration move the base without changing the effective
// speed, so relative steps start from the base; persistence reports the base.
//
// The configured playback speed is clamped to [1.1, 5.0], falls back to 2.7
// when non-finite, and is then snapped to the existing 0.1 speed increment.
// Changing it while the accelerating gap continues applies the new speed when
// it differs from the engine's by at least 0.001, without replacing the base.

#pragma once

#include <optional>

namespace niratan::player {

class SubtitleCueStore;

class SubtitleGapPlayback {
   public:
    static constexpr double kDefaultSpeed = 2.7;
    static constexpr double kEdgeGuard = 0.6;

    bool enabled() const;
    double configured_speed() const;
    // Whether a base speed is captured, i.e. the effective speed is temporary.
    bool accelerating() const;

    // setSubtitleGapFastForwardEnabled: an unchanged flag does nothing, and
    // disabling restores the captured base.
    std::optional<double> set_enabled(bool enabled, double engine_speed);
    // setSubtitleGapFastForwardSpeed.
    std::optional<double> set_configured_speed(double speed, double engine_speed);

    // VideoPlayerViewModel.updateSubtitleGapPlayback.
    std::optional<double> update(const SubtitleCueStore& store, double playback_time,
                                 double subtitle_delay, bool playing, double engine_speed);
    // setSpeed and adjustSpeed; relative steps use the base, not the temporary
    // effective speed.
    std::optional<double> set_user_speed(double speed, double engine_speed);
    std::optional<double> adjust_user_speed(double delta, double engine_speed);
    // restoreSubtitleGapFastForwardSpeedIfNeeded before seek, opening media and
    // shutdown.
    std::optional<double> restore(double engine_speed);

    // currentUserSpeed and the speed snapshotForPersistence stores.
    double user_speed(double engine_speed) const;
    double persistence_speed(double engine_speed) const;

    // SubtitleCueStore.slice plus shouldFastForwardSubtitleGap.
    bool in_eligible_gap(const SubtitleCueStore& store, double playback_time,
                         double subtitle_delay) const;

   private:
    bool enabled_ = false;
    double configured_speed_ = kDefaultSpeed;
    std::optional<double> base_speed_;
};

}  // namespace niratan::player
