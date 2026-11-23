// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reader zoom gestures use the profile-backed Font Size setting from
// AppearanceView.nativeAppearanceContent instead of a separate page zoom.

#include "viewmodels/settings/settings_view_model.h"

#include "data_root.h"

#include <QSignalSpy>
#include <QtTest>

using niratan::test::TempDataRoot;
using niratan::viewmodels::SettingsViewModel;

class ReaderFontSizeTest : public QObject {
    Q_OBJECT

   private slots:
    void wheel_accumulates_signed_notches();
    void shortcuts_step_without_consuming_wheel_remainder();
    void gestures_use_stepper_limits_and_save_to_the_profile();
};

void ReaderFontSizeTest::wheel_accumulates_signed_notches() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel model;
    model.setFontSize(24);
    QSignalSpy changes(&model, &SettingsViewModel::readerChanged);

    model.adjustFontSizeFromWheel(90);
    model.adjustFontSizeFromWheel(-30);
    QCOMPARE(model.fontSize(), 24);
    QCOMPARE(changes.size(), 0);

    model.adjustFontSizeFromWheel(60);
    QCOMPARE(model.fontSize(), 25);
    QCOMPARE(changes.size(), 1);

    model.adjustFontSizeFromWheel(-300);
    QCOMPARE(model.fontSize(), 23);
    model.adjustFontSizeFromWheel(-60);
    QCOMPARE(model.fontSize(), 22);
}

void ReaderFontSizeTest::shortcuts_step_without_consuming_wheel_remainder() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel model;
    model.setFontSize(24);

    model.adjustFontSizeFromWheel(-60);
    model.stepFontSize(true);
    QCOMPARE(model.fontSize(), 25);
    model.stepFontSize(false);
    QCOMPARE(model.fontSize(), 24);
    model.adjustFontSizeFromWheel(-60);
    QCOMPARE(model.fontSize(), 23);
}

void ReaderFontSizeTest::gestures_use_stepper_limits_and_save_to_the_profile() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    SettingsViewModel model;
    model.setFontSize(39);

    model.adjustFontSizeFromWheel(360);
    QCOMPARE(model.fontSize(), 40);
    model.stepFontSize(true);
    QCOMPARE(model.fontSize(), 40);
    model.adjustFontSizeFromWheel(-120);
    QCOMPARE(model.fontSize(), 39);
    QCOMPARE(SettingsViewModel().fontSize(), 39);

    model.setFontSize(17);
    model.adjustFontSizeFromWheel(-360);
    QCOMPARE(model.fontSize(), 16);
    model.stepFontSize(false);
    QCOMPARE(model.fontSize(), 16);
    QCOMPARE(SettingsViewModel().fontSize(), 16);
}

QTEST_APPLESS_MAIN(ReaderFontSizeTest)

#include "tst_reader_font_size.moc"
