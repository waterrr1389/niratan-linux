// SPDX-License-Identifier: GPL-3.0-or-later
//
// The three Theme card colour accessors of SettingsViewModel. See the class
// comment in settings_view_model.h for the stored keys and the ColorHexCodec
// rule. A getter parses the stored string and falls back to the key's
// UserConfig.init default when it does not decode; the stored text is not
// rewritten by a read.

#include "settings_view_model.h"

#include <optional>
#include <string>

#include "settings_view_model_state.h"

#include "models/color/color_hex.h"
#include "models/profiles/reader_settings.h"

namespace niratan::viewmodels {
namespace {

QColor to_color(const std::string& value, const QColor& fallback) {
    const std::optional<color::Rgba> parsed = color::parse_hex(value);
    if (!parsed) {
        return fallback;
    }
    return QColor(parsed->red, parsed->green, parsed->blue, parsed->alpha);
}

std::string to_hex(const QColor& value) {
    return color::hex_string(color::Rgba{value.red(), value.green(), value.blue(), value.alpha()});
}

}  // namespace

QColor SettingsViewModel::customBackgroundColor() const {
    return to_color(state_->reader.custom_background_color, QColor(255, 255, 255));
}

void SettingsViewModel::setCustomBackgroundColor(const QColor& value) {
    update_reader(state_->reader.custom_background_color, to_hex(value));
}

QColor SettingsViewModel::customTextColor() const {
    return to_color(state_->reader.custom_text_color, QColor(0, 0, 0));
}

void SettingsViewModel::setCustomTextColor(const QColor& value) {
    update_reader(state_->reader.custom_text_color, to_hex(value));
}

QColor SettingsViewModel::customInfoColor() const {
    return to_color(state_->reader.custom_info_color, QColor(153, 153, 153));
}

void SettingsViewModel::setCustomInfoColor(const QColor& value) {
    update_reader(state_->reader.custom_info_color, to_hex(value));
}

}  // namespace niratan::viewmodels
