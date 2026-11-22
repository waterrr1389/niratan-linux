// SPDX-License-Identifier: GPL-3.0-or-later
//
// The decisions the Sasayaki player makes, kept pure so they can be tested
// without QMediaPlayer. Follows Features/Sasayaki/SasayakiPlayer.swift
// (skipInterval, seekLandingTolerance, tick, seek, playCue(from:stop:),
// flushPlayback, persistPlaybackPosition, updateCue, displayCue,
// togglePlayback, nextCue, prevCue, skip(forward:), seekRelative,
// navigateToCue, prepareTransition and handleRestoreCompleted) and
// NativeMac/NativeReaderView.swift (pushPopups and
// closePopup(resumePausedPlayback:)).
//
// Paths that touch the player or the web view stay in the view model; these
// functions hold only the arithmetic and conditions, so the caller applies
// them in the Swift order: a tick checks the landing before writing anything,
// a flush writes a pending target before it samples the player, and a seek
// persists its target before moving the player.
//
// Cue following: navigateToCue seeks inside the open chapter and loads another
// chapter otherwise; prepareTransition resumes only when playback was running;
// a restore chooses the pending cue of the restored chapter before the
// timeline cue, and updateCue follows a cue into another chapter only with
// auto-scroll on and playback started. The restore and follow decisions name
// the current reader chapter, not the chapter of the cue on display.
//
// Constants: skipInterval is 15 s and seekLandingTolerance 0.75 s from
// SasayakiPlayer; the speed slider range is 0.5...2.5
// (SasayakiPlaybackLimits.speedRange) and the delay slider range -2...2, both
// with a 0.05 step (Features/Sasayaki/SasayakiSheet.swift).
//
// A zero or negative duration means the duration is not known yet; the
// player keeps duration 0 until a finite positive value arrives.

#pragma once

#include <optional>
#include <string>

namespace niratan::viewmodels {

constexpr double kSasayakiSkipInterval = 15.0;
constexpr double kSeekLandingTolerance = 0.75;
constexpr double kMinimumRate = 0.5;
constexpr double kMaximumRate = 2.5;
constexpr double kMinimumDelay = -2.0;
constexpr double kMaximumDelay = 2.0;

// B3: true when no target is pending, otherwise |seconds - pending| <= 0.75.
bool tick_lands(double seconds, std::optional<double> pending);

// B2: seconds >= stop_time, false when no stop time is set.
bool stop_reached(std::optional<double> stop_time, double seconds);

// B4: the value written this tick, or nullopt between whole seconds.
std::optional<double> persisted_second(double seconds, int last_second);

// B5: the position a flush writes, or nullopt when it writes nothing.
std::optional<double> flush_position(std::optional<double> pending, bool playing,
                                     std::optional<double> player_seconds, double current_time);

// B1: the landing stop time, nullopt when stop is false.
std::optional<double> play_cue_stop_time(double end_time, double delay, bool stop);

// B11/B12: auto-scroll on and playback started.
bool follows_playback(bool auto_scroll, bool has_played_once);

// B13.
bool pause_for_lookup(bool playing, bool auto_pause);
bool resume_after_lookup(bool was_paused_by_lookup, bool playing);

// B8: the reference time for cue navigation: the active cue's start wins;
// without one, a previous-cue lookup clamps a negative lookup to zero.
double cue_reference_time(bool has_current, double current_start, double lookup, bool previous);

// B7/B9: clamping.
double clamped_skip_target(double position, double duration, bool forward);
double clamped_seek_target(double seconds, double duration);
double clamp_rate(double rate);
double clamp_delay(double delay);

// B2: navigateToCue loads a chapter only when the cue is outside the open one.
bool cue_needs_chapter_load(int cue_chapter, int current_chapter);

// B3: a cross-chapter cue jump starts playback after the restore when it was
// playing or asked to start.
bool starts_after_restore(bool playing, bool start_playback);

// B4: prepareTransition resumes playback only when it was playing.
bool transition_resumes(bool playing);

// B6: handleRestoreCompleted(currentIndex:). handled is false without match
// data, where Swift leaves every flag untouched. cue_id has no value when no
// cue is displayed; reveal and resume are the display and start decisions.
struct RestoreCueDecision {
    bool handled = false;
    std::optional<std::string> cue_id;
    bool reveal = false;
    bool resume = false;

    bool operator==(const RestoreCueDecision&) const = default;
};

RestoreCueDecision restore_cue_decision(bool has_match, bool chapter_transition,
                                        const std::optional<std::string>& pending_id,
                                        int pending_chapter, bool reveal_pending,
                                        bool should_resume, bool auto_scroll, int current_index,
                                        const std::optional<std::string>& timeline_id,
                                        int timeline_chapter);

// B7: updateCue(for:).
enum class CueFollowAction {
    none,
    clear,
    display,
    follow,
};

struct CueFollowDecision {
    CueFollowAction action = CueFollowAction::none;
    bool reveal = false;

    bool operator==(const CueFollowDecision&) const = default;
};

// current_chapter is the reader's chapter index.
CueFollowDecision cue_follow_decision(bool has_audio, bool has_match, bool chapter_transition,
                                      const std::string& cue_id, int cue_chapter,
                                      const std::string& current_id, int current_chapter,
                                      bool auto_scroll, bool has_played_once);

}  // namespace niratan::viewmodels
