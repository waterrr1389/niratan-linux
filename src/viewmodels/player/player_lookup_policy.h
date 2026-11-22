// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shift-hover lookup state for the video subtitle overlay, following
// Features/Video/Subtitles/VideoShiftHoverLookupState.swift and the timer gate
// in InteractiveSubtitleTextView.swift (scheduleShiftHoverLookup,
// mouseMoved, mouseExited, handleModifierFlagsChanged): a hover lookup is
// scheduled only while Shift is pressed over a hover point, and the delayed
// call rechecks pointerMoved() before looking up.
//
// normalized_delay_milliseconds follows VideoShiftHoverLookupState
// .normalizedDelayMilliseconds: UserConfig.desktopLookupHoverDelayMs is
// clamped to 0..1000 ms.

#pragma once

namespace niratan::viewmodels {

struct ShiftHoverLookupState {
    // Stores the value and returns whether a hover lookup should be scheduled
    // now: Shift pressed and a hover point available.
    bool set_shift_pressed(bool pressed);
    bool set_hover_point_available(bool available);
    bool pointer_moved() const;
    // Clears both flags, so a scheduled lookup must not run.
    void cancel();
    bool shift_pressed() const;
    bool hover_point_available() const;

   private:
    bool shift_pressed_ = false;
    bool hover_point_available_ = false;
};

int normalized_delay_milliseconds(int value);

}  // namespace niratan::viewmodels
