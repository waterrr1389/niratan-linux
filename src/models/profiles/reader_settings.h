// SPDX-License-Identifier: GPL-3.0-or-later
//
// The reader appearance settings stored per profile, following
// ReaderProfileSettings in Models/Profile.swift and the JSON persistence in
// Core/ProfileSettingsStore.swift (reader_settings.json).
//
// The struct mirrors the Swift property names and defaults exactly. Properties
// that are optional in Swift stay optional here: a document written by an
// older build may omit them, and the applied value then falls back to the
// default only where UserConfig does so. The codec requires the non-optional
// keys and accepts a missing optional key as empty, which is what synthesized
// Codable does.
//
// The default values are ReaderProfileSettings.defaults: vertical Japanese
// writing, 22pt, continuous scrolling off, two column horizontal pages off,
// line height 1.65, no letter or paragraph spacing, the title and character
// count shown, top progress with a percentage. Colours are "#RRGGBBAA"
// strings and pass through unvalidated. defaults also fills the optional
// properties that Swift's defaults set explicitly (furigana mode Off, text
// speed 35, progress shown as a character count), so an empty optional here
// means the key was absent from a stored document, not that the default is
// empty.

#pragma once

#include <QJsonDocument>

#include <optional>
#include <string>

namespace niratan::profiles {

struct ReaderProfileSettings {
    std::string theme = "System";
    std::string ui_theme = "System";
    bool system_light_sepia = false;
    bool sepia_invert_in_dark = false;
    std::string custom_background_color = "#FFFFFFFF";
    std::string custom_text_color = "#000000FF";
    std::string custom_info_color = "#999999FF";
    bool vertical_writing = true;
    std::string selected_font = "Hiragino Mincho ProN";
    int font_size = 22;
    bool hide_furigana = false;
    std::optional<std::string> furigana_mode;
    bool continuous_mode = false;
    // Linux stores UserConfig.readerWheelPageTurnEnabled with reader appearance.
    bool wheel_page_turn = true;
    std::optional<bool> two_column_horizontal_pages;
    std::optional<bool> paragraph_mode;
    std::optional<bool> text_animation;
    std::optional<int> text_speed;
    int horizontal_padding = 5;
    int vertical_padding = 0;
    bool avoid_page_break = false;
    bool justify_text = false;
    bool blur_images = false;
    bool layout_advanced = false;
    double line_height = 1.65;
    double character_spacing = 0.0;
    double paragraph_spacing = 0.0;
    bool show_title = true;
    bool show_characters = true;
    std::optional<bool> show_progress;
    std::optional<bool> show_chapter_progress;
    std::optional<std::string> progress_count;
    bool show_percentage = true;
    bool show_progress_top = true;
    bool show_statistics_toggle = false;
    bool show_reading_speed = false;
    bool show_reading_time = false;
    bool show_sasayaki_toggle = false;

    bool operator==(const ReaderProfileSettings&) const = default;
};

// ReaderProfileSettings.defaults, which the store writes when a profile has no
// reader_settings.json yet.
ReaderProfileSettings default_reader_settings();

// nullopt when a required key is missing or wrongly typed, or when the
// document is not an object. Missing optional keys stay empty.
std::optional<ReaderProfileSettings> reader_settings_from_json(const QJsonDocument& document);

QJsonDocument reader_settings_to_json(const ReaderProfileSettings& settings);

}  // namespace niratan::profiles
