// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Layout and Display card accessors of SettingsViewModel. See the class
// comment in settings_view_model.h for the stored keys. Each setter saves the
// whole reader document; the running reader watches that file and reloads it,
// so an open book follows these values without reopening.

#include "settings_view_model.h"

#include "settings_view_model_state.h"

namespace niratan::viewmodels {

bool SettingsViewModel::wheelPageTurnEnabled() const {
    return state_->reader.wheel_page_turn;
}

void SettingsViewModel::setWheelPageTurnEnabled(bool value) {
    update_reader(state_->reader.wheel_page_turn, value);
}

bool SettingsViewModel::showStatisticsToggle() const {
    return state_->reader.show_statistics_toggle;
}

void SettingsViewModel::setShowStatisticsToggle(bool value) {
    update_reader(state_->reader.show_statistics_toggle, value);
}

bool SettingsViewModel::showReadingSpeed() const {
    return state_->reader.show_reading_speed;
}

void SettingsViewModel::setShowReadingSpeed(bool value) {
    update_reader(state_->reader.show_reading_speed, value);
}

bool SettingsViewModel::showReadingTime() const {
    return state_->reader.show_reading_time;
}

void SettingsViewModel::setShowReadingTime(bool value) {
    update_reader(state_->reader.show_reading_time, value);
}

}  // namespace niratan::viewmodels
