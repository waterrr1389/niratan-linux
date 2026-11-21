// SPDX-License-Identifier: GPL-3.0-or-later
//
// Synthesized Codable conformance for ReaderProfileSettings in
// Models/Profile.swift: every non-optional key is required and a value of the
// wrong JSON type rejects the whole document. Optional keys follow
// decodeIfPresent, so a missing or null value decodes as nil while a wrongly
// typed one still fails. The Linux wheel setting defaults to true when absent.
// Extra keys are ignored. Only the Linux hover delay is clamped on read.

#include "reader_settings.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>

#include <algorithm>
#include <optional>
#include <string>

namespace niratan::profiles {
namespace {

bool read_string(const QJsonObject& object, QLatin1StringView key, std::string& target) {
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        return false;
    }
    target = value.toString().toStdString();
    return true;
}

bool read_bool(const QJsonObject& object, QLatin1StringView key, bool& target) {
    const QJsonValue value = object.value(key);
    if (!value.isBool()) {
        return false;
    }
    target = value.toBool();
    return true;
}

bool read_int(const QJsonObject& object, QLatin1StringView key, int& target) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    target = value.toInt();
    return true;
}

bool read_double(const QJsonObject& object, QLatin1StringView key, double& target) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return false;
    }
    target = value.toDouble();
    return true;
}

bool read_optional_string(const QJsonObject& object,
                          QLatin1StringView key,
                          std::optional<std::string>& target) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isString()) {
        return false;
    }
    target = value.toString().toStdString();
    return true;
}

bool read_optional_bool(const QJsonObject& object,
                        QLatin1StringView key,
                        std::optional<bool>& target) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    target = value.toBool();
    return true;
}

bool read_optional_int(const QJsonObject& object,
                       QLatin1StringView key,
                       std::optional<int>& target) {
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    target = value.toInt();
    return true;
}

QString text(const std::string& value) {
    return QString::fromStdString(value);
}

}  // namespace

ReaderProfileSettings default_reader_settings() {
    // ReaderProfileSettings.defaults fills the optional properties explicitly;
    // an empty optional only means the key was absent from a stored document.
    ReaderProfileSettings settings;
    settings.furigana_mode = "Off";
    settings.two_column_horizontal_pages = false;
    settings.paragraph_mode = false;
    settings.text_animation = false;
    settings.text_speed = 35;
    settings.show_progress = true;
    settings.show_chapter_progress = false;
    settings.progress_count = "Characters";
    return settings;
}

std::optional<ReaderProfileSettings> reader_settings_from_json(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    ReaderProfileSettings settings;
    constexpr QLatin1StringView wheel_key("wheelPageTurnEnabled");
    if (object.contains(wheel_key) && !read_bool(object, wheel_key, settings.wheel_page_turn)) {
        return std::nullopt;
    }
    constexpr QLatin1StringView hover_key("desktopLookupHoverDelayMs");
    settings.desktop_lookup_hover_delay_ms = static_cast<int>(std::clamp(
        object.value(hover_key).toDouble(settings.desktop_lookup_hover_delay_ms), 0.0, 250.0));
    if (!read_string(object, QLatin1StringView("theme"), settings.theme) ||
        !read_string(object, QLatin1StringView("uiTheme"), settings.ui_theme) ||
        !read_bool(object, QLatin1StringView("systemLightSepia"), settings.system_light_sepia) ||
        !read_bool(object, QLatin1StringView("sepiaInvertInDark"), settings.sepia_invert_in_dark) ||
        !read_string(object, QLatin1StringView("customBackgroundColor"),
                     settings.custom_background_color) ||
        !read_string(object, QLatin1StringView("customTextColor"), settings.custom_text_color) ||
        !read_string(object, QLatin1StringView("customInfoColor"), settings.custom_info_color) ||
        !read_bool(object, QLatin1StringView("verticalWriting"), settings.vertical_writing) ||
        !read_string(object, QLatin1StringView("selectedFont"), settings.selected_font) ||
        !read_int(object, QLatin1StringView("fontSize"), settings.font_size) ||
        !read_bool(object, QLatin1StringView("hideFurigana"), settings.hide_furigana) ||
        !read_bool(object, QLatin1StringView("continuousMode"), settings.continuous_mode) ||
        !read_int(object, QLatin1StringView("horizontalPadding"), settings.horizontal_padding) ||
        !read_int(object, QLatin1StringView("verticalPadding"), settings.vertical_padding) ||
        !read_bool(object, QLatin1StringView("avoidPageBreak"), settings.avoid_page_break) ||
        !read_bool(object, QLatin1StringView("justifyText"), settings.justify_text) ||
        !read_bool(object, QLatin1StringView("blurImages"), settings.blur_images) ||
        !read_bool(object, QLatin1StringView("layoutAdvanced"), settings.layout_advanced) ||
        !read_double(object, QLatin1StringView("lineHeight"), settings.line_height) ||
        !read_double(object, QLatin1StringView("characterSpacing"), settings.character_spacing) ||
        !read_double(object, QLatin1StringView("paragraphSpacing"), settings.paragraph_spacing) ||
        !read_bool(object, QLatin1StringView("showTitle"), settings.show_title) ||
        !read_bool(object, QLatin1StringView("showCharacters"), settings.show_characters) ||
        !read_bool(object, QLatin1StringView("showPercentage"), settings.show_percentage) ||
        !read_bool(object, QLatin1StringView("showProgressTop"), settings.show_progress_top) ||
        !read_bool(object, QLatin1StringView("showStatisticsToggle"),
                   settings.show_statistics_toggle) ||
        !read_bool(object, QLatin1StringView("showReadingSpeed"), settings.show_reading_speed) ||
        !read_bool(object, QLatin1StringView("showReadingTime"), settings.show_reading_time) ||
        !read_bool(object, QLatin1StringView("showSasayakiToggle"),
                   settings.show_sasayaki_toggle)) {
        return std::nullopt;
    }

    if (!read_optional_string(object, QLatin1StringView("furiganaMode"), settings.furigana_mode) ||
        !read_optional_bool(object, QLatin1StringView("twoColumnHorizontalPages"),
                            settings.two_column_horizontal_pages) ||
        !read_optional_bool(object, QLatin1StringView("paragraphMode"),
                            settings.paragraph_mode) ||
        !read_optional_bool(object, QLatin1StringView("textAnimation"),
                            settings.text_animation) ||
        !read_optional_int(object, QLatin1StringView("textSpeed"), settings.text_speed) ||
        !read_optional_bool(object, QLatin1StringView("showProgress"), settings.show_progress) ||
        !read_optional_bool(object, QLatin1StringView("showChapterProgress"),
                            settings.show_chapter_progress) ||
        !read_optional_string(object, QLatin1StringView("progressCount"),
                              settings.progress_count)) {
        return std::nullopt;
    }

    return settings;
}

QJsonDocument reader_settings_to_json(const ReaderProfileSettings& settings) {
    QJsonObject object;
    object.insert(QLatin1StringView("theme"), text(settings.theme));
    object.insert(QLatin1StringView("uiTheme"), text(settings.ui_theme));
    object.insert(QLatin1StringView("systemLightSepia"), settings.system_light_sepia);
    object.insert(QLatin1StringView("sepiaInvertInDark"), settings.sepia_invert_in_dark);
    object.insert(QLatin1StringView("customBackgroundColor"),
                  text(settings.custom_background_color));
    object.insert(QLatin1StringView("customTextColor"), text(settings.custom_text_color));
    object.insert(QLatin1StringView("customInfoColor"), text(settings.custom_info_color));
    object.insert(QLatin1StringView("verticalWriting"), settings.vertical_writing);
    object.insert(QLatin1StringView("selectedFont"), text(settings.selected_font));
    object.insert(QLatin1StringView("fontSize"), settings.font_size);
    object.insert(QLatin1StringView("hideFurigana"), settings.hide_furigana);
    if (settings.furigana_mode) {
        object.insert(QLatin1StringView("furiganaMode"), text(*settings.furigana_mode));
    }
    object.insert(QLatin1StringView("continuousMode"), settings.continuous_mode);
    object.insert(QLatin1StringView("wheelPageTurnEnabled"), settings.wheel_page_turn);
    object.insert(QLatin1StringView("desktopLookupHoverDelayMs"),
                  settings.desktop_lookup_hover_delay_ms);
    if (settings.two_column_horizontal_pages) {
        object.insert(QLatin1StringView("twoColumnHorizontalPages"),
                      *settings.two_column_horizontal_pages);
    }
    if (settings.paragraph_mode) {
        object.insert(QLatin1StringView("paragraphMode"), *settings.paragraph_mode);
    }
    if (settings.text_animation) {
        object.insert(QLatin1StringView("textAnimation"), *settings.text_animation);
    }
    if (settings.text_speed) {
        object.insert(QLatin1StringView("textSpeed"), *settings.text_speed);
    }
    object.insert(QLatin1StringView("horizontalPadding"), settings.horizontal_padding);
    object.insert(QLatin1StringView("verticalPadding"), settings.vertical_padding);
    object.insert(QLatin1StringView("avoidPageBreak"), settings.avoid_page_break);
    object.insert(QLatin1StringView("justifyText"), settings.justify_text);
    object.insert(QLatin1StringView("blurImages"), settings.blur_images);
    object.insert(QLatin1StringView("layoutAdvanced"), settings.layout_advanced);
    object.insert(QLatin1StringView("lineHeight"), settings.line_height);
    object.insert(QLatin1StringView("characterSpacing"), settings.character_spacing);
    object.insert(QLatin1StringView("paragraphSpacing"), settings.paragraph_spacing);
    object.insert(QLatin1StringView("showTitle"), settings.show_title);
    object.insert(QLatin1StringView("showCharacters"), settings.show_characters);
    if (settings.show_progress) {
        object.insert(QLatin1StringView("showProgress"), *settings.show_progress);
    }
    if (settings.show_chapter_progress) {
        object.insert(QLatin1StringView("showChapterProgress"), *settings.show_chapter_progress);
    }
    if (settings.progress_count) {
        object.insert(QLatin1StringView("progressCount"), text(*settings.progress_count));
    }
    object.insert(QLatin1StringView("showPercentage"), settings.show_percentage);
    object.insert(QLatin1StringView("showProgressTop"), settings.show_progress_top);
    object.insert(QLatin1StringView("showStatisticsToggle"), settings.show_statistics_toggle);
    object.insert(QLatin1StringView("showReadingSpeed"), settings.show_reading_speed);
    object.insert(QLatin1StringView("showReadingTime"), settings.show_reading_time);
    object.insert(QLatin1StringView("showSasayakiToggle"), settings.show_sasayaki_toggle);
    return QJsonDocument(object);
}

}  // namespace niratan::profiles
