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

#include "viewmodels/settings/settings_view_model.h"

#include "data_root.h"

#include <QtTest>

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

QTEST_APPLESS_MAIN(SettingsViewModelTest)
#include "tst_settings_view_model.moc"
