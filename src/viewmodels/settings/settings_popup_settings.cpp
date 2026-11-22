// SPDX-License-Identifier: GPL-3.0-or-later
//
// Popup card accessors of SettingsViewModel, following the Popup card of
// AppearanceView.nativeAppearanceContent (Width Slider 100...1400 step 10,
// Height Slider 100...800 step 10, Scale Slider 0.8...1.5 step 0.05, Swipe
// Threshold Slider 20...80 step 5 while Swipe to Dismiss is on, and the four
// toggles) and the popup fields of Core/UserConfig.swift. Values stay in the
// active profile's reader_settings.json under the Swift keys.

#include "settings_view_model.h"

#include "settings_view_model_state.h"

#include <algorithm>
#include <cmath>

namespace niratan::viewmodels {
namespace {

double snap(double value, double minimum, double maximum, int increments_per_unit) {
    return std::round(std::clamp(value, minimum, maximum) * increments_per_unit) /
        increments_per_unit;
}

}  // namespace

int SettingsViewModel::popupWidth() const {
    return state_->reader.popup_width;
}

void SettingsViewModel::setPopupWidth(int value) {
    update_reader(state_->reader.popup_width,
                  std::lround(std::clamp(value, 100, 1400) / 10.0) * 10);
}

int SettingsViewModel::popupHeight() const {
    return state_->reader.popup_height;
}

void SettingsViewModel::setPopupHeight(int value) {
    update_reader(state_->reader.popup_height,
                  std::lround(std::clamp(value, 100, 800) / 10.0) * 10);
}

double SettingsViewModel::popupScale() const {
    return state_->reader.popup_scale;
}

void SettingsViewModel::setPopupScale(double value) {
    update_reader(state_->reader.popup_scale, snap(value, 0.8, 1.5, 20));
}

bool SettingsViewModel::popupActionBar() const {
    return state_->reader.popup_action_bar;
}

void SettingsViewModel::setPopupActionBar(bool value) {
    update_reader(state_->reader.popup_action_bar, value);
}

bool SettingsViewModel::popupDisableTransparency() const {
    return state_->reader.popup_disable_transparency;
}

void SettingsViewModel::setPopupDisableTransparency(bool value) {
    update_reader(state_->reader.popup_disable_transparency, value);
}

bool SettingsViewModel::popupFullWidth() const {
    return state_->reader.popup_full_width;
}

void SettingsViewModel::setPopupFullWidth(bool value) {
    update_reader(state_->reader.popup_full_width, value);
}

bool SettingsViewModel::popupSwipeToDismiss() const {
    return state_->reader.popup_swipe_to_dismiss;
}

void SettingsViewModel::setPopupSwipeToDismiss(bool value) {
    update_reader(state_->reader.popup_swipe_to_dismiss, value);
}

int SettingsViewModel::popupSwipeThreshold() const {
    return state_->reader.popup_swipe_threshold;
}

void SettingsViewModel::setPopupSwipeThreshold(int value) {
    update_reader(state_->reader.popup_swipe_threshold,
                  std::lround(std::clamp(value, 20, 80) / 5.0) * 5);
}

}  // namespace niratan::viewmodels
