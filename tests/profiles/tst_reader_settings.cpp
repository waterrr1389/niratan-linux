// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/profiles/reader_settings.h"

#include <QJsonObject>
#include <QtTest>

#include <optional>
#include <string>

#include "models/profiles/profile_index.h"
#include "services/storage/json_file.h"

using niratan::profiles::ReaderProfileSettings;
using niratan::profiles::default_reader_settings;
using niratan::profiles::reader_settings_from_json;
using niratan::profiles::reader_settings_to_json;

// Models/Profile.swift (ReaderProfileSettings, defaults) and
// script/test_reader_profile_settings_persistence.swift define the values;
// Core/ProfileSettingsStore.swift defines when the defaults are written.
namespace {

std::optional<ReaderProfileSettings> from_text(const char* text) {
    return reader_settings_from_json(QJsonDocument::fromJson(QByteArray(text)));
}

}  // namespace

class ReaderSettingsTest : public QObject {
    Q_OBJECT

   private slots:
    void the_defaults_match_the_swift_defaults();
    void the_defaults_round_trip();
    void every_field_round_trips();
    void the_document_uses_the_swift_keys();
    void missing_optional_keys_stay_empty();
    void a_missing_or_wrong_required_key_is_rejected();
    void extra_keys_are_ignored();
};

void ReaderSettingsTest::the_defaults_match_the_swift_defaults() {
    const ReaderProfileSettings defaults = default_reader_settings();

    QCOMPARE(defaults.theme, std::string("System"));
    QCOMPARE(defaults.ui_theme, std::string("System"));
    QVERIFY(!defaults.system_light_sepia);
    QVERIFY(!defaults.sepia_invert_in_dark);
    QCOMPARE(defaults.custom_background_color, std::string("#FFFFFFFF"));
    QCOMPARE(defaults.custom_text_color, std::string("#000000FF"));
    QCOMPARE(defaults.custom_info_color, std::string("#999999FF"));
    QVERIFY(defaults.vertical_writing);
    QCOMPARE(defaults.selected_font, std::string("Hiragino Mincho ProN"));
    QCOMPARE(defaults.font_size, 22);
    QVERIFY(!defaults.hide_furigana);
    QCOMPARE(defaults.furigana_mode, std::optional<std::string>{std::string("Off")});
    QVERIFY(!defaults.continuous_mode);
    QCOMPARE(defaults.two_column_horizontal_pages, std::optional<bool>{false});
    QCOMPARE(defaults.paragraph_mode, std::optional<bool>{false});
    QCOMPARE(defaults.text_animation, std::optional<bool>{false});
    QCOMPARE(defaults.text_speed, std::optional<int>{35});
    QCOMPARE(defaults.horizontal_padding, 5);
    QCOMPARE(defaults.vertical_padding, 0);
    QVERIFY(!defaults.avoid_page_break);
    QVERIFY(!defaults.justify_text);
    QVERIFY(!defaults.blur_images);
    QVERIFY(!defaults.layout_advanced);
    QCOMPARE(defaults.line_height, 1.65);
    QCOMPARE(defaults.character_spacing, 0.0);
    QCOMPARE(defaults.paragraph_spacing, 0.0);
    QVERIFY(defaults.show_title);
    QVERIFY(defaults.show_characters);
    QCOMPARE(defaults.show_progress, std::optional<bool>{true});
    QCOMPARE(defaults.show_chapter_progress, std::optional<bool>{false});
    QCOMPARE(defaults.progress_count, std::optional<std::string>{std::string("Characters")});
    QVERIFY(defaults.show_percentage);
    QVERIFY(defaults.show_progress_top);
    QVERIFY(!defaults.show_statistics_toggle);
    QVERIFY(!defaults.show_reading_speed);
    QVERIFY(!defaults.show_reading_time);
    QVERIFY(!defaults.show_sasayaki_toggle);
}

void ReaderSettingsTest::the_defaults_round_trip() {
    const ReaderProfileSettings defaults = default_reader_settings();
    const auto decoded = reader_settings_from_json(reader_settings_to_json(defaults));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, defaults);
}

void ReaderSettingsTest::every_field_round_trips() {
    ReaderProfileSettings settings = default_reader_settings();
    settings.theme = "Sepia";
    settings.ui_theme = "Dark";
    settings.system_light_sepia = true;
    settings.sepia_invert_in_dark = true;
    settings.custom_background_color = "#11223344";
    settings.custom_text_color = "#55667788";
    settings.custom_info_color = "#99AABBCC";
    settings.vertical_writing = false;
    settings.selected_font = "Noto Serif JP";
    settings.font_size = 28;
    settings.hide_furigana = true;
    settings.furigana_mode = "All";
    settings.continuous_mode = true;
    settings.two_column_horizontal_pages = true;
    settings.paragraph_mode = false;
    settings.text_animation = true;
    settings.text_speed = 60;
    settings.horizontal_padding = 12;
    settings.vertical_padding = 4;
    settings.avoid_page_break = true;
    settings.justify_text = true;
    settings.blur_images = true;
    settings.layout_advanced = true;
    settings.line_height = 2.0;
    settings.character_spacing = 0.25;
    settings.paragraph_spacing = 1.5;
    settings.show_title = false;
    settings.show_characters = false;
    settings.show_progress = false;
    settings.show_chapter_progress = true;
    settings.progress_count = "Percentage";
    settings.show_percentage = false;
    settings.show_progress_top = false;
    settings.show_statistics_toggle = true;
    settings.show_reading_speed = true;
    settings.show_reading_time = true;
    settings.show_sasayaki_toggle = true;

    const auto decoded = reader_settings_from_json(reader_settings_to_json(settings));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, settings);
}

void ReaderSettingsTest::the_document_uses_the_swift_keys() {
    const QJsonObject object = reader_settings_to_json(default_reader_settings()).object();

    QVERIFY(object.contains("theme"));
    QVERIFY(object.contains("uiTheme"));
    QVERIFY(object.contains("systemLightSepia"));
    QVERIFY(object.contains("sepiaInvertInDark"));
    QVERIFY(object.contains("customBackgroundColor"));
    QVERIFY(object.contains("customTextColor"));
    QVERIFY(object.contains("customInfoColor"));
    QVERIFY(object.contains("verticalWriting"));
    QVERIFY(object.contains("selectedFont"));
    QVERIFY(object.contains("fontSize"));
    QVERIFY(object.contains("hideFurigana"));
    QVERIFY(object.contains("continuousMode"));
    QVERIFY(object.contains("horizontalPadding"));
    QVERIFY(object.contains("verticalPadding"));
    QVERIFY(object.contains("avoidPageBreak"));
    QVERIFY(object.contains("justifyText"));
    QVERIFY(object.contains("blurImages"));
    QVERIFY(object.contains("layoutAdvanced"));
    QVERIFY(object.contains("lineHeight"));
    QVERIFY(object.contains("characterSpacing"));
    QVERIFY(object.contains("paragraphSpacing"));
    QVERIFY(object.contains("showTitle"));
    QVERIFY(object.contains("showCharacters"));
    QVERIFY(object.contains("showPercentage"));
    QVERIFY(object.contains("showProgressTop"));
    QVERIFY(object.contains("showStatisticsToggle"));
    QVERIFY(object.contains("showReadingSpeed"));
    QVERIFY(object.contains("showReadingTime"));
    QVERIFY(object.contains("showSasayakiToggle"));

    QCOMPARE(object.value("fontSize").toInt(), 22);
    QCOMPARE(object.value("lineHeight").toDouble(), 1.65);
}

void ReaderSettingsTest::missing_optional_keys_stay_empty() {
    const auto settings = from_text(
        "{\"theme\":\"Light\",\"uiTheme\":\"Light\",\"systemLightSepia\":false,"
        "\"sepiaInvertInDark\":false,\"customBackgroundColor\":\"#FFFFFFFF\","
        "\"customTextColor\":\"#000000FF\",\"customInfoColor\":\"#999999FF\","
        "\"verticalWriting\":false,\"selectedFont\":\"Font\",\"fontSize\":18,"
        "\"hideFurigana\":false,\"continuousMode\":false,\"horizontalPadding\":5,"
        "\"verticalPadding\":0,\"avoidPageBreak\":false,\"justifyText\":false,"
        "\"blurImages\":false,\"layoutAdvanced\":false,\"lineHeight\":1.5,"
        "\"characterSpacing\":0,\"paragraphSpacing\":0,\"showTitle\":true,"
        "\"showCharacters\":true,\"showPercentage\":true,\"showProgressTop\":true,"
        "\"showStatisticsToggle\":false,\"showReadingSpeed\":false,"
        "\"showReadingTime\":false,\"showSasayakiToggle\":false}");

    QVERIFY(settings.has_value());
    QVERIFY(!settings->furigana_mode.has_value());
    QVERIFY(!settings->two_column_horizontal_pages.has_value());
    QVERIFY(!settings->paragraph_mode.has_value());
    QVERIFY(!settings->text_animation.has_value());
    QVERIFY(!settings->text_speed.has_value());
    QVERIFY(!settings->show_progress.has_value());
    QVERIFY(!settings->show_chapter_progress.has_value());
    QVERIFY(!settings->progress_count.has_value());
    QCOMPARE(settings->font_size, 18);
}

void ReaderSettingsTest::a_missing_or_wrong_required_key_is_rejected() {
    QVERIFY(!from_text("{}").has_value());
    QVERIFY(!from_text("[]").has_value());

    QJsonObject object = reader_settings_to_json(default_reader_settings()).object();
    object.remove("fontSize");
    QVERIFY(!reader_settings_from_json(QJsonDocument(object)).has_value());

    object = reader_settings_to_json(default_reader_settings()).object();
    object.insert("verticalWriting", "yes");
    QVERIFY(!reader_settings_from_json(QJsonDocument(object)).has_value());

    object = reader_settings_to_json(default_reader_settings()).object();
    object.insert("lineHeight", "tall");
    QVERIFY(!reader_settings_from_json(QJsonDocument(object)).has_value());

    object = reader_settings_to_json(default_reader_settings()).object();
    object.insert("textSpeed", "fast");
    QVERIFY(!reader_settings_from_json(QJsonDocument(object)).has_value());
}

void ReaderSettingsTest::extra_keys_are_ignored() {
    QJsonObject object = reader_settings_to_json(default_reader_settings()).object();
    object.insert("unexpected", true);

    const auto settings = reader_settings_from_json(QJsonDocument(object));
    QVERIFY(settings.has_value());
    QCOMPARE(*settings, default_reader_settings());
}

QTEST_APPLESS_MAIN(ReaderSettingsTest)
#include "tst_reader_settings.moc"
