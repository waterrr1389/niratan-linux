// SPDX-License-Identifier: GPL-3.0-or-later

#include "player_lookup_policy.h"

#include <algorithm>

namespace niratan::viewmodels {

bool ShiftHoverLookupState::set_shift_pressed(bool pressed) {
    shift_pressed_ = pressed;
    return pointer_moved();
}

bool ShiftHoverLookupState::set_hover_point_available(bool available) {
    hover_point_available_ = available;
    return pointer_moved();
}

bool ShiftHoverLookupState::pointer_moved() const {
    return shift_pressed_ && hover_point_available_;
}

void ShiftHoverLookupState::cancel() {
    shift_pressed_ = false;
    hover_point_available_ = false;
}

bool ShiftHoverLookupState::shift_pressed() const {
    return shift_pressed_;
}

bool ShiftHoverLookupState::hover_point_available() const {
    return hover_point_available_;
}

int normalized_delay_milliseconds(int value) {
    return std::clamp(value, 0, 1000);
}

}  // namespace niratan::viewmodels
