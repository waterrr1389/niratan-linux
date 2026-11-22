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

#include "viewmodels/settings/settings_view_model.h"

#include "data_root.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <filesystem>
#include <optional>

#include "models/profiles/reader_settings.h"
#include "services/profiles/profile_repository.h"
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

QTEST_APPLESS_MAIN(SettingsViewModelTest)
#include "tst_settings_view_model.moc"
