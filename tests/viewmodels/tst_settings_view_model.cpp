// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reader setting setters, following Features/Settings/AppearanceView.swift
// (nativeAppearanceContent: Font Size Stepper 16...40, Text Speed Slider
// 25...100 step 5, Horizontal Padding Stepper 0...80, Vertical Padding Stepper
// 0...50, Line Height Slider 1.0...2.5 step 0.05, Character Spacing Slider
// -10...10 step 1 as a percent, Paragraph Spacing Slider 0...3 step 0.1) and
// Core/UserConfig.swift (the didSet persists the received value unchanged).
//
// A bounded setter stores what its Appearance control can show: clamp to the
// range, round to the step with std::round, then persist. Values read from an
// existing file are not re-clamped, as with Swift's Codable load, so a second
// model on the same data folder must see the stored value unchanged.
//
// The popup cases follow the Popup card of AppearanceView.nativeAppearanceContent
// (Width Slider 100...1400 step 10, Height Slider 100...800 step 10, Scale
// Slider 0.8...1.5 step 0.05, Swipe Threshold Slider 20...80 step 5 that stays
// visible only while Swipe to Dismiss is on, and the four toggles) and the
// defaults and keys of UserConfig.swift. Each edit saves the whole active
// reader document, so the other reader fields survive a popup edit.
//
// The colour cases follow the Theme card colour rows of
// AppearanceView.nativeAppearanceContent (Background Color, Text Color, Info
// Color while the theme is Custom), the three colour properties of
// Core/UserConfig.swift with the UserConfig.init defaults (opaque white,
// opaque black, opaque #999999), and Core/ColorHexCodec.swift as the stored
// form: uppercase #RRGGBB, or #RRGGBBAA when alpha is not 255.
//
// The display cases follow the Layout card's Mouse Wheel Page Turn toggle
// (AppearanceView 247, UserConfig.readerWheelPageTurnEnabled default true) and
// the Display card's Show Statistics Toggle, Show Reading Speed and Show
// Reading Time toggles (AppearanceView 355-362, defaults false in
// UserConfig.init and ReaderProfileSettings.defaults), stored under
// wheelPageTurnEnabled, showStatisticsToggle, showReadingSpeed and
// showReadingTime in the active profile's reader_settings.json.

#include "viewmodels/settings/settings_view_model.h"

#include "viewmodels/sasayaki/sasayaki_settings_view_model.h"

#include "data_root.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest>

#include <filesystem>
#include <optional>

#include "models/profiles/reader_settings.h"
#include "services/profiles/profile_repository.h"
#include "services/settings/settings_store.h"
#include "services/storage/app_paths.h"

using niratan::profiles::ProfileRepository;
using niratan::test::TempDataRoot;
using niratan::viewmodels::SettingsViewModel;

namespace {

// Sets a value on one model, then reads it from that model and from a fresh
// model on the same data folder.
template <typename Setter, typename Getter, typename Value>
void expect_round_trip(const Setter& set, const Getter& get, Value input, Value expected) {
    SettingsViewModel first;
    set(first, input);
    QCOMPARE(get(first), expected);

    SettingsViewModel second;
    QCOMPARE(get(second), expected);
}

std::filesystem::path reader_settings_file() {
    const ProfileRepository repository(niratan::storage::data_root());
    const auto directory = repository.profile_directory(repository.active_profile().id);
    return directory.has_value() ? *directory / "reader_settings.json" : std::filesystem::path{};
}

std::filesystem::path dictionary_settings_file() {
    const ProfileRepository repository(niratan::storage::data_root());
    const auto directory = repository.profile_directory(repository.active_profile().id);
    return directory.has_value() ? *directory / "dictionary_settings.json"
                                 : std::filesystem::path{};
}

std::optional<QJsonObject> read_object(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

bool write_object(const std::filesystem::path& path, const QJsonObject& object) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(QJsonDocument(object).toJson()) > 0;
}

}  // namespace

class SettingsViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void imported_fonts_import_and_delete();
    void fontSize_keeps_its_stepper_range();
    void textSpeed_rounds_to_the_step_and_clamps();
    void horizontalPadding_keeps_swift_range();
    void verticalPadding_keeps_swift_range();
    void lineHeight_rounds_to_the_step_and_clamps();
    void characterSpacing_is_a_percent();
    void paragraphSpacing_rounds_to_the_step_and_clamps();
    void popup_defaults();
    void popupWidth_rounds_to_the_step_and_clamps();
    void popupHeight_rounds_to_the_step_and_clamps();
    void popupScale_rounds_to_the_step_and_clamps();
    void popup_toggles_round_trip();
    void popupSwipeThreshold_rounds_to_the_step_and_clamps();
    void popup_edits_keep_the_other_reader_settings();
    void custom_colors_default();
    void custom_colors_store_color_hex();
    void custom_colors_keys_and_reader_read();
    void custom_colors_invalid_value_uses_default();
    void custom_colors_edit_keeps_other_reader_settings();
    void reader_display_defaults();
    void reader_display_round_trip();
    void reader_display_edits_keep_the_other_reader_settings();
    void frequency_sort_order_defaults_and_rejects_unknown();
    void frequency_sort_dictionary_round_trips_and_clears();
    void search_text_size_clamps_and_keeps_the_other_dictionary_settings();
    void settings_and_sasayaki_models_share_one_store();
    void interleaved_edits_after_a_profile_switch_target_the_active_profile();
    void reload_reads_the_switched_profile_and_refreshes_sasayaki();
    void dictionary_edit_after_a_profile_switch_keeps_the_other_fields();
};

// B1: the Font Size Stepper is 16...40 with no off-step values.
void SettingsViewModelTest::fontSize_keeps_its_stepper_range() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, int value) { model.setFontSize(value); };
    const auto get = [](const SettingsViewModel& model) { return model.fontSize(); };

    expect_round_trip(set, get, 16, 16);
    expect_round_trip(set, get, 40, 40);
    expect_round_trip(set, get, 15, 16);
    expect_round_trip(set, get, 41, 40);
}

// B2: Text Speed is 25...100, step 5, stored as Int.
void SettingsViewModelTest::textSpeed_rounds_to_the_step_and_clamps() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, int value) { model.setTextSpeed(value); };
    const auto get = [](const SettingsViewModel& model) { return model.textSpeed(); };

    expect_round_trip(set, get, 25, 25);
    expect_round_trip(set, get, 100, 100);
    expect_round_trip(set, get, 24, 25);
    expect_round_trip(set, get, 101, 100);
    expect_round_trip(set, get, 26, 25);
    expect_round_trip(set, get, 99, 100);
}

// B3: Horizontal Padding is 0...80, step 1.
void SettingsViewModelTest::horizontalPadding_keeps_swift_range() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, int value) { model.setHorizontalPadding(value); };
    const auto get = [](const SettingsViewModel& model) { return model.horizontalPadding(); };

    expect_round_trip(set, get, 1, 1);
    expect_round_trip(set, get, 79, 79);
    expect_round_trip(set, get, -1, 0);
    expect_round_trip(set, get, 81, 80);
}

// B4: Vertical Padding is 0...50, step 1.
void SettingsViewModelTest::verticalPadding_keeps_swift_range() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, int value) { model.setVerticalPadding(value); };
    const auto get = [](const SettingsViewModel& model) { return model.verticalPadding(); };

    expect_round_trip(set, get, 1, 1);
    expect_round_trip(set, get, 49, 49);
    expect_round_trip(set, get, -1, 0);
    expect_round_trip(set, get, 51, 50);
}

// B5: Line Height is 1.0...2.5, step 0.05.
void SettingsViewModelTest::lineHeight_rounds_to_the_step_and_clamps() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, double value) { model.setLineHeight(value); };
    const auto get = [](const SettingsViewModel& model) { return model.lineHeight(); };

    expect_round_trip(set, get, 1.05, 1.05);
    expect_round_trip(set, get, 2.45, 2.45);
    expect_round_trip(set, get, 0.95, 1.0);
    expect_round_trip(set, get, 2.55, 2.5);
    expect_round_trip(set, get, 1.02, 1.0);
    expect_round_trip(set, get, 2.47, 2.45);
}

// B6: Character Spacing is -10...10 as a percent, step 1; the stylesheet
// divides it by 100.0.
void SettingsViewModelTest::characterSpacing_is_a_percent() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, double value) {
        model.setCharacterSpacing(value);
    };
    const auto get = [](const SettingsViewModel& model) { return model.characterSpacing(); };

    expect_round_trip(set, get, -9.0, -9.0);
    expect_round_trip(set, get, 9.0, 9.0);
    expect_round_trip(set, get, -11.0, -10.0);
    expect_round_trip(set, get, 11.0, 10.0);
    expect_round_trip(set, get, -9.7, -10.0);
    expect_round_trip(set, get, 9.6, 10.0);
}

// B7: Paragraph Spacing is 0...3, step 0.1.
void SettingsViewModelTest::paragraphSpacing_rounds_to_the_step_and_clamps() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, double value) {
        model.setParagraphSpacing(value);
    };
    const auto get = [](const SettingsViewModel& model) { return model.paragraphSpacing(); };

    expect_round_trip(set, get, 0.1, 0.1);
    expect_round_trip(set, get, 2.9, 2.9);
    expect_round_trip(set, get, -0.1, 0.0);
    expect_round_trip(set, get, 3.1, 3.0);
    expect_round_trip(set, get, 0.16, 0.2);
    expect_round_trip(set, get, 2.96, 3.0);
}

// B1: a fresh data root returns the UserConfig.init defaults.
void SettingsViewModelTest::popup_defaults() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel model;
    QCOMPARE(model.popupWidth(), 320);
    QCOMPARE(model.popupHeight(), 250);
    QCOMPARE(model.popupScale(), 1.0);
    QCOMPARE(model.popupActionBar(), false);
    QCOMPARE(model.popupDisableTransparency(), false);
    QCOMPARE(model.popupFullWidth(), false);
    QCOMPARE(model.popupSwipeToDismiss(), false);
    QCOMPARE(model.popupSwipeThreshold(), 40);
}

// B2: Width is 100...1400, step 10.
void SettingsViewModelTest::popupWidth_rounds_to_the_step_and_clamps() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, int value) { model.setPopupWidth(value); };
    const auto get = [](const SettingsViewModel& model) { return model.popupWidth(); };

    expect_round_trip(set, get, 110, 110);
    expect_round_trip(set, get, 1390, 1390);
    expect_round_trip(set, get, 90, 100);
    expect_round_trip(set, get, 1410, 1400);
    expect_round_trip(set, get, 104, 100);
    expect_round_trip(set, get, 1396, 1400);
}

// B3: Height is 100...800, step 10.
void SettingsViewModelTest::popupHeight_rounds_to_the_step_and_clamps() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, int value) { model.setPopupHeight(value); };
    const auto get = [](const SettingsViewModel& model) { return model.popupHeight(); };

    expect_round_trip(set, get, 110, 110);
    expect_round_trip(set, get, 790, 790);
    expect_round_trip(set, get, 90, 100);
    expect_round_trip(set, get, 810, 800);
    expect_round_trip(set, get, 106, 110);
    expect_round_trip(set, get, 794, 790);
}

// B4: Scale is 0.8...1.5, step 0.05.
void SettingsViewModelTest::popupScale_rounds_to_the_step_and_clamps() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, double value) { model.setPopupScale(value); };
    const auto get = [](const SettingsViewModel& model) { return model.popupScale(); };

    expect_round_trip(set, get, 0.85, 0.85);
    expect_round_trip(set, get, 1.45, 1.45);
    expect_round_trip(set, get, 0.7, 0.8);
    expect_round_trip(set, get, 1.6, 1.5);
    expect_round_trip(set, get, 0.83, 0.85);
    expect_round_trip(set, get, 1.42, 1.4);
}

// B5-B8: the four popup toggles store both values and persist.
void SettingsViewModelTest::popup_toggles_round_trip() {
    TempDataRoot data;
    QVERIFY(data.isValid());

    SettingsViewModel first;
    first.setPopupActionBar(true);
    first.setPopupDisableTransparency(true);
    first.setPopupFullWidth(true);
    first.setPopupSwipeToDismiss(true);
    QCOMPARE(first.popupActionBar(), true);
    QCOMPARE(first.popupDisableTransparency(), true);
    QCOMPARE(first.popupFullWidth(), true);
    QCOMPARE(first.popupSwipeToDismiss(), true);

    SettingsViewModel second;
    QCOMPARE(second.popupActionBar(), true);
    QCOMPARE(second.popupDisableTransparency(), true);
    QCOMPARE(second.popupFullWidth(), true);
    QCOMPARE(second.popupSwipeToDismiss(), true);
    second.setPopupActionBar(false);
    second.setPopupDisableTransparency(false);
    second.setPopupFullWidth(false);
    second.setPopupSwipeToDismiss(false);

    SettingsViewModel third;
    QCOMPARE(third.popupActionBar(), false);
    QCOMPARE(third.popupDisableTransparency(), false);
    QCOMPARE(third.popupFullWidth(), false);
    QCOMPARE(third.popupSwipeToDismiss(), false);
}

// B9: Swipe Threshold is 20...80, step 5, and persists while the toggle is off.
void SettingsViewModelTest::popupSwipeThreshold_rounds_to_the_step_and_clamps() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto set = [](SettingsViewModel& model, int value) {
        model.setPopupSwipeThreshold(value);
    };
    const auto get = [](const SettingsViewModel& model) { return model.popupSwipeThreshold(); };

    expect_round_trip(set, get, 25, 25);
    expect_round_trip(set, get, 75, 75);
    expect_round_trip(set, get, 15, 20);
    expect_round_trip(set, get, 85, 80);
    expect_round_trip(set, get, 22, 20);
    expect_round_trip(set, get, 78, 80);
}

// B10: an edit saves the whole reader document, so other fields survive.
void SettingsViewModelTest::popup_edits_keep_the_other_reader_settings() {
    TempDataRoot data;
    QVERIFY(data.isValid());

    SettingsViewModel first;
    first.setTheme(QStringLiteral("Dark"));
    first.setFontSize(30);
    first.setPopupWidth(110);
    first.setPopupHeight(790);
    first.setPopupScale(1.45);
    first.setPopupSwipeThreshold(75);
    first.setPopupActionBar(true);
    first.setPopupDisableTransparency(true);
    first.setPopupFullWidth(true);
    first.setPopupSwipeToDismiss(true);

    SettingsViewModel second;
    QCOMPARE(second.theme(), QStringLiteral("Dark"));
    QCOMPARE(second.fontSize(), 30);
    QCOMPARE(second.popupWidth(), 110);
    QCOMPARE(second.popupHeight(), 790);
    QCOMPARE(second.popupScale(), 1.45);
    QCOMPARE(second.popupSwipeThreshold(), 75);
    QCOMPARE(second.popupActionBar(), true);
    QCOMPARE(second.popupDisableTransparency(), true);
    QCOMPARE(second.popupFullWidth(), true);
    QCOMPARE(second.popupSwipeToDismiss(), true);
}

// C1: UserConfig.init returns opaque white, black and #999999 from a fresh
// data root.
void SettingsViewModelTest::custom_colors_default() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel model;

    QCOMPARE(model.customBackgroundColor(), QColor(255, 255, 255));
    QCOMPARE(model.customTextColor(), QColor(0, 0, 0));
    QCOMPARE(model.customInfoColor(), QColor(153, 153, 153));
}

// C2: a set colour is stored as ColorHexCodec writes it and a fresh model
// returns it unchanged.
void SettingsViewModelTest::custom_colors_store_color_hex() {
    TempDataRoot data;
    QVERIFY(data.isValid());

    const auto set_background = [](SettingsViewModel& model, QColor value) {
        model.setCustomBackgroundColor(value);
    };
    const auto background = [](const SettingsViewModel& model) {
        return model.customBackgroundColor();
    };
    expect_round_trip(set_background, background, QColor(255, 0, 0), QColor(255, 0, 0));
    auto object = read_object(reader_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value(QStringLiteral("customBackgroundColor")).toString(),
             QStringLiteral("#FF0000"));

    const auto set_text = [](SettingsViewModel& model, QColor value) {
        model.setCustomTextColor(value);
    };
    const auto text = [](const SettingsViewModel& model) { return model.customTextColor(); };
    expect_round_trip(set_text, text, QColor(0, 128, 255, 102), QColor(0, 128, 255, 102));
    object = read_object(reader_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value(QStringLiteral("customTextColor")).toString(),
             QStringLiteral("#0080FF66"));

    const auto set_info = [](SettingsViewModel& model, QColor value) {
        model.setCustomInfoColor(value);
    };
    const auto info = [](const SettingsViewModel& model) { return model.customInfoColor(); };
    expect_round_trip(set_info, info, QColor(0, 0, 0, 0), QColor(0, 0, 0, 0));
    object = read_object(reader_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value(QStringLiteral("customInfoColor")).toString(),
             QStringLiteral("#00000000"));
}

// C3: the three values are written under the Swift keys of the active
// profile's reader_settings.json, the document the reader loads.
void SettingsViewModelTest::custom_colors_keys_and_reader_read() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel model;
    model.setCustomBackgroundColor(QColor(255, 0, 0));
    model.setCustomTextColor(QColor(0, 128, 255, 102));
    model.setCustomInfoColor(QColor(0, 0, 0, 0));

    const auto object = read_object(reader_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value(QStringLiteral("customBackgroundColor")).toString(),
             QStringLiteral("#FF0000"));
    QCOMPARE(object->value(QStringLiteral("customTextColor")).toString(),
             QStringLiteral("#0080FF66"));
    QCOMPARE(object->value(QStringLiteral("customInfoColor")).toString(),
             QStringLiteral("#00000000"));

    const auto decoded = niratan::profiles::reader_settings_from_json(QJsonDocument(*object));
    QVERIFY(decoded.has_value());
    QCOMPARE(QString::fromStdString(decoded->custom_background_color), QStringLiteral("#FF0000"));
    QCOMPARE(QString::fromStdString(decoded->custom_text_color), QStringLiteral("#0080FF66"));
    QCOMPARE(QString::fromStdString(decoded->custom_info_color), QStringLiteral("#00000000"));
}

// C4: a value ColorHexCodec cannot parse reads as that key's UserConfig.init
// default, and the other keys keep their stored values.
void SettingsViewModelTest::custom_colors_invalid_value_uses_default() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel first;
    first.setCustomBackgroundColor(QColor(255, 0, 0));
    first.setCustomTextColor(QColor(0, 128, 255));
    first.setCustomInfoColor(QColor(0, 0, 0, 0));

    const auto path = reader_settings_file();
    auto object = read_object(path);
    QVERIFY(object.has_value());
    object->insert(QStringLiteral("customTextColor"), QStringLiteral("oops"));
    QVERIFY(write_object(path, *object));

    SettingsViewModel second;
    QCOMPARE(second.customTextColor(), QColor(0, 0, 0));
    QCOMPARE(second.customBackgroundColor(), QColor(255, 0, 0));
    QCOMPARE(second.customInfoColor(), QColor(0, 0, 0, 0));
}

// C5: an edit saves the whole reader document, so a stored theme and font
// size survive a colour edit.
void SettingsViewModelTest::custom_colors_edit_keeps_other_reader_settings() {
    TempDataRoot data;
    QVERIFY(data.isValid());

    SettingsViewModel first;
    first.setTheme(QStringLiteral("Dark"));
    first.setFontSize(30);
    first.setCustomBackgroundColor(QColor(255, 0, 0));
    first.setCustomTextColor(QColor(0, 128, 255, 102));
    first.setCustomInfoColor(QColor(0, 0, 0, 0));

    SettingsViewModel second;
    QCOMPARE(second.theme(), QStringLiteral("Dark"));
    QCOMPARE(second.fontSize(), 30);
    QCOMPARE(second.customBackgroundColor(), QColor(255, 0, 0));
    QCOMPARE(second.customTextColor(), QColor(0, 128, 255, 102));
    QCOMPARE(second.customInfoColor(), QColor(0, 0, 0, 0));
}

// R1: UserConfig.init and ReaderProfileSettings.defaults: the wheel flag is on,
// the three statistics rows are off.
void SettingsViewModelTest::reader_display_defaults() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel model;

    QCOMPARE(model.wheelPageTurnEnabled(), true);
    QCOMPARE(model.showStatisticsToggle(), false);
    QCOMPARE(model.showReadingSpeed(), false);
    QCOMPARE(model.showReadingTime(), false);
}

// R2: each value round-trips through a fresh model and is stored under its
// Swift key.
void SettingsViewModelTest::reader_display_round_trip() {
    TempDataRoot data;
    QVERIFY(data.isValid());

    SettingsViewModel first;
    first.setWheelPageTurnEnabled(false);
    first.setShowStatisticsToggle(true);
    first.setShowReadingSpeed(true);
    first.setShowReadingTime(true);

    SettingsViewModel second;
    QCOMPARE(second.wheelPageTurnEnabled(), false);
    QCOMPARE(second.showStatisticsToggle(), true);
    QCOMPARE(second.showReadingSpeed(), true);
    QCOMPARE(second.showReadingTime(), true);

    const auto object = read_object(reader_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value(QStringLiteral("wheelPageTurnEnabled")).toBool(), false);
    QCOMPARE(object->value(QStringLiteral("showStatisticsToggle")).toBool(), true);
    QCOMPARE(object->value(QStringLiteral("showReadingSpeed")).toBool(), true);
    QCOMPARE(object->value(QStringLiteral("showReadingTime")).toBool(), true);
}

// R2: an edit saves the whole reader document, so other reader fields survive.
void SettingsViewModelTest::reader_display_edits_keep_the_other_reader_settings() {
    TempDataRoot data;
    QVERIFY(data.isValid());

    SettingsViewModel first;
    first.setTheme(QStringLiteral("Dark"));
    first.setFontSize(30);
    first.setWheelPageTurnEnabled(false);
    first.setShowStatisticsToggle(true);
    first.setShowReadingSpeed(true);
    first.setShowReadingTime(true);

    SettingsViewModel second;
    QCOMPARE(second.theme(), QStringLiteral("Dark"));
    QCOMPARE(second.fontSize(), 30);
    QCOMPARE(second.wheelPageTurnEnabled(), false);
    QCOMPARE(second.showStatisticsToggle(), true);
    QCOMPARE(second.showReadingSpeed(), true);
    QCOMPARE(second.showReadingTime(), true);
}

// B1: UserConfig.init and LookupFrequencySortOrder: a missing or unknown stored
// order reads Auto; the setter accepts only the three raw strings, stores the
// accepted one under frequencySortOrder and leaves an unknown string unchanged.
void SettingsViewModelTest::frequency_sort_order_defaults_and_rejects_unknown() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel model;
    QCOMPARE(model.frequencySortOrder(), QStringLiteral("Auto"));
    model.setMaxResults(20);
    model.setFrequencySortOrder(QStringLiteral("Descending"));
    QCOMPARE(model.frequencySortOrder(), QStringLiteral("Descending"));
    auto object = read_object(dictionary_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value("frequencySortOrder").toString(), QStringLiteral("Descending"));
    QCOMPARE(object->value("maxResults").toInt(), 20);
    model.setFrequencySortOrder(QStringLiteral("Ascending"));
    QCOMPARE(model.frequencySortOrder(), QStringLiteral("Ascending"));
    object = read_object(dictionary_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value("frequencySortOrder").toString(), QStringLiteral("Ascending"));
    QCOMPARE(object->value("maxResults").toInt(), 20);
    model.setFrequencySortOrder(QStringLiteral("Auto"));
    QCOMPARE(model.frequencySortOrder(), QStringLiteral("Auto"));
    object = read_object(dictionary_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value("frequencySortOrder").toString(), QStringLiteral("Auto"));
    QCOMPARE(object->value("maxResults").toInt(), 20);
    SettingsViewModel second;
    QCOMPARE(second.frequencySortOrder(), QStringLiteral("Auto"));
    second.setFrequencySortOrder(QStringLiteral("sideways"));
    QCOMPARE(second.frequencySortOrder(), QStringLiteral("Auto"));
    object = read_object(dictionary_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value("frequencySortOrder").toString(), QStringLiteral("Auto"));
    const auto path = dictionary_settings_file();
    object->insert(QStringLiteral("frequencySortOrder"), QStringLiteral("sideways"));
    QVERIFY(write_object(path, *object));
    SettingsViewModel unknown;
    QCOMPARE(unknown.frequencySortOrder(), QStringLiteral("Auto"));
    object = read_object(path);
    QVERIFY(object.has_value());
    object->remove(QStringLiteral("frequencySortOrder"));
    QVERIFY(write_object(path, *object));
    SettingsViewModel absent;
    QCOMPARE(absent.frequencySortOrder(), QStringLiteral("Auto"));
}

// B2: UserConfig.frequencySortDictionary: absent reads empty, any received
// string is stored unchanged under frequencySortDictionary, and the empty
// string clears the choice.
void SettingsViewModelTest::frequency_sort_dictionary_round_trips_and_clears() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel first;
    first.setMaxResults(20);
    first.setFrequencySortDictionary(QStringLiteral("Jiten"));
    QCOMPARE(first.frequencySortDictionary(), QStringLiteral("Jiten"));
    auto object = read_object(dictionary_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value(QStringLiteral("frequencySortDictionary")).toString(),
             QStringLiteral("Jiten"));
    QCOMPARE(object->value("maxResults").toInt(), 20);

    SettingsViewModel second;
    QCOMPARE(second.frequencySortDictionary(), QStringLiteral("Jiten"));
    second.setFrequencySortDictionary(QString());
    QCOMPARE(second.frequencySortDictionary(), QString());
    object = read_object(dictionary_settings_file());
    QVERIFY(object.has_value());
    QCOMPARE(object->value(QStringLiteral("frequencySortDictionary")).toString(), QString());
    QCOMPARE(object->value("maxResults").toInt(), 20);

    SettingsViewModel cleared;
    QCOMPARE(cleared.frequencySortDictionary(), QString());

    cleared.setFrequencySortDictionary(QStringLiteral("No Such Dictionary"));
    SettingsViewModel unknown;
    QCOMPARE(unknown.frequencySortDictionary(), QStringLiteral("No Such Dictionary"));
}

// B3: the Search Text Stepper is 12...48 with no off-step values; the getter
// returns a stored value unchanged (UserConfig.init) and the setter keeps the
// other dictionary key.
void SettingsViewModelTest::search_text_size_clamps_and_keeps_the_other_dictionary_settings() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel defaults;
    QCOMPARE(defaults.searchTextSize(), 26);
    const auto set = [](SettingsViewModel& model, int value) { model.setSearchTextSize(value); };
    const auto get = [](const SettingsViewModel& model) { return model.searchTextSize(); };

    expect_round_trip(set, get, 12, 12);
    expect_round_trip(set, get, 48, 48);
    expect_round_trip(set, get, 11, 12);
    expect_round_trip(set, get, 49, 48);

    const auto path = dictionary_settings_file();
    auto object = read_object(path);
    QVERIFY(object.has_value());
    object->insert(QStringLiteral("searchTextSize"), 100);
    QVERIFY(write_object(path, *object));
    SettingsViewModel hand_edited;
    QCOMPARE(hand_edited.searchTextSize(), 100);

    hand_edited.setMaxResults(20);
    SettingsViewModel edited;
    edited.setSearchTextSize(30);
    object = read_object(path);
    QVERIFY(object.has_value());
    QCOMPARE(object->value(QStringLiteral("searchTextSize")).toInt(), 30);
    QCOMPARE(object->value(QStringLiteral("maxResults")).toInt(), 20);
}

// ProfileSettingsStore.shared: the Appearance form's settings model and its
// Sasayaki model use one store.
void SettingsViewModelTest::settings_and_sasayaki_models_share_one_store() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel settings;
    niratan::viewmodels::SasayakiSettingsViewModel sasayaki;
    QVERIFY(settings.store() != nullptr);
    QCOMPARE(settings.store(), sasayaki.store());
}

namespace {

// Two profiles with distinct reader settings; the second is made active after
// the models are built, as ProfileRepository.setGlobalActiveProfile does
// while the Settings page is kept.
struct SwitchedProfiles {
    std::string first;
    std::string second;
};

std::optional<SwitchedProfiles> seed_two_profiles() {
    ProfileRepository repository(niratan::storage::data_root());
    const auto created = repository.create_profile(
        "English", niratan::profiles::ContentLanguage::english, std::nullopt);
    if (!created.profile.has_value()) {
        return std::nullopt;
    }
    niratan::settings::SettingsStore store(niratan::storage::data_root());
    auto first = niratan::profiles::default_reader_settings();
    first.font_size = 20;
    first.horizontal_padding = 10;
    first.show_sasayaki_toggle = false;
    auto second = niratan::profiles::default_reader_settings();
    second.font_size = 24;
    second.horizontal_padding = 40;
    second.show_sasayaki_toggle = false;
    if (store.save_reader("default-ja", first) != niratan::settings::StoreError::none ||
        store.save_reader(created.profile->id, second) != niratan::settings::StoreError::none) {
        return std::nullopt;
    }
    return SwitchedProfiles{"default-ja", created.profile->id};
}

bool activate(const std::string& id) {
    ProfileRepository repository(niratan::storage::data_root());
    return !repository.set_active_profile(id).has_value();
}

}  // namespace

// ProfileSettingsStore.activate and persistReaderSettings: after a switch,
// a Sasayaki toggle edit and an Appearance edit both land in the active
// profile, neither restores the other's field or the old profile's values,
// and the previous profile's file is unchanged.
void SettingsViewModelTest::interleaved_edits_after_a_profile_switch_target_the_active_profile() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto profiles = seed_two_profiles();
    QVERIFY(profiles.has_value());
    SettingsViewModel settings;
    niratan::viewmodels::SasayakiSettingsViewModel sasayaki;
    QCOMPARE(settings.fontSize(), 20);
    QVERIFY(activate(profiles->second));

    sasayaki.setShowSasayakiToggle(true);
    settings.setFontSize(30);
    sasayaki.setShowSasayakiToggle(false);
    sasayaki.setShowSasayakiToggle(true);

    niratan::settings::SettingsStore store(niratan::storage::data_root());
    const auto second = store.load_reader(profiles->second);
    QVERIFY(second.has_value());
    QCOMPARE(second->font_size, 30);
    QCOMPARE(second->horizontal_padding, 40);
    QCOMPARE(second->show_sasayaki_toggle, true);
    const auto first = store.load_reader(profiles->first);
    QVERIFY(first.has_value());
    QCOMPARE(first->font_size, 20);
    QCOMPARE(first->horizontal_padding, 10);
    QCOMPARE(first->show_sasayaki_toggle, false);
}

// A kept Appearance form shows the newly active profile after reload(), and
// the Sasayaki model sharing the store re-reads with it.
void SettingsViewModelTest::reload_reads_the_switched_profile_and_refreshes_sasayaki() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto profiles = seed_two_profiles();
    QVERIFY(profiles.has_value());
    SettingsViewModel settings;
    niratan::viewmodels::SasayakiSettingsViewModel sasayaki;
    QCOMPARE(sasayaki.show_sasayaki_toggle(), false);

    niratan::settings::SettingsStore store(niratan::storage::data_root());
    auto second = *store.load_reader(profiles->second);
    second.show_sasayaki_toggle = true;
    QCOMPARE(store.save_reader(profiles->second, second), niratan::settings::StoreError::none);
    QVERIFY(activate(profiles->second));

    QSignalSpy reader_spy(&settings, &SettingsViewModel::readerChanged);
    QSignalSpy toggle_spy(
        &sasayaki, &niratan::viewmodels::SasayakiSettingsViewModel::showSasayakiToggleChanged);
    settings.reload();

    QVERIFY(reader_spy.count() >= 1);
    QCOMPARE(settings.fontSize(), 24);
    QCOMPARE(settings.horizontalPadding(), 40);
    QCOMPARE(toggle_spy.count(), 1);
    QCOMPARE(sasayaki.show_sasayaki_toggle(), true);
}

// ProfileSettingsStore.activate loads the new profile's settings and
// UserConfig keeps each dictionary field independent: after a switch without
// reload, a Max Results edit (DictionaryView) changes only maxResults in the
// active profile and leaves the previous profile's file alone.
void SettingsViewModelTest::dictionary_edit_after_a_profile_switch_keeps_the_other_fields() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto profiles = seed_two_profiles();
    QVERIFY(profiles.has_value());
    niratan::settings::SettingsStore store(niratan::storage::data_root());
    auto first = niratan::profiles::default_dictionary_settings();
    first.max_results = 10;
    first.scan_length = 12;
    auto second = niratan::profiles::default_dictionary_settings();
    second.max_results = 20;
    second.scan_length = 24;
    QCOMPARE(store.save_dictionary(profiles->first, first), niratan::settings::StoreError::none);
    QCOMPARE(store.save_dictionary(profiles->second, second), niratan::settings::StoreError::none);
    QVERIFY(activate(profiles->first));

    SettingsViewModel settings;
    QVERIFY(activate(profiles->second));
    settings.setMaxResults(30);

    const auto stored_first = store.load_dictionary(profiles->first);
    const auto stored_second = store.load_dictionary(profiles->second);
    QVERIFY(stored_first.has_value() && stored_second.has_value());
    QCOMPARE(stored_first->max_results, 10);
    QCOMPARE(stored_first->scan_length, 12);
    QCOMPARE(stored_second->max_results, 30);
    QCOMPARE(stored_second->scan_length, 24);
}

// FontManager.storedFonts and AppearanceView's Font row: imported fonts are
// listed by name, an import copies the file into Fonts, deleting the selected
// imported font resets the selection to the default, and a failure sets the
// page's error line while the list stays unchanged.
void SettingsViewModelTest::imported_fonts_import_and_delete() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    const auto app = niratan::storage::data_root();
    std::error_code error;
    std::filesystem::create_directories(app / "Fonts", error);
    QFile existing(QString::fromStdString((app / "Fonts/Existing.ttf").string()));
    QVERIFY(existing.open(QIODevice::WriteOnly));
    QCOMPARE(existing.write("existing"), qint64(8));
    existing.close();
    QFile source(QString::fromStdString((app / "Imported Font.otf").string()));
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("imported"), qint64(8));
    source.close();

    SettingsViewModel model;
    QCOMPARE(model.importedFonts(), QStringList{QStringLiteral("Existing")});
    QSignalSpy fontsChanged(&model, &SettingsViewModel::importedFontsChanged);
    QVERIFY(model.importFont(
        QUrl::fromLocalFile(QString::fromStdString((app / "Imported Font.otf").string()))));
    QVERIFY(!fontsChanged.isEmpty());
    QCOMPARE(model.importedFonts(),
             (QStringList{QStringLiteral("Existing"), QStringLiteral("Imported Font")}));
    QVERIFY(std::filesystem::exists(app / "Fonts/Imported Font.otf"));

    model.setSelectedFont(QStringLiteral("Imported Font"));
    QCOMPARE(model.selectedFont(), QStringLiteral("Imported Font"));
    QVERIFY(model.deleteFont(QStringLiteral("Imported Font")));
    QVERIFY(!std::filesystem::exists(app / "Fonts/Imported Font.otf"));
    QCOMPARE(model.selectedFont(), QStringLiteral("Hiragino Mincho ProN"));
    QCOMPARE(model.importedFonts(), QStringList{QStringLiteral("Existing")});

    QVERIFY(model.deleteFont(QStringLiteral("Missing")));
    QVERIFY(model.deleteFont(QStringLiteral("Hiragino Mincho ProN")));
    QCOMPARE(model.importedFonts(), QStringList{QStringLiteral("Existing")});
    QVERIFY(model.error().isEmpty());

    QVERIFY(!model.importFont(
        QUrl::fromLocalFile(QString::fromStdString((app / "missing.ttf").string()))));
    QVERIFY(!model.error().isEmpty());
    QCOMPARE(model.importedFonts(), QStringList{QStringLiteral("Existing")});
}

QTEST_APPLESS_MAIN(SettingsViewModelTest)
#include "tst_settings_view_model.moc"
