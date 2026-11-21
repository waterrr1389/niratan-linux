// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the Sasayaki colour codec and defaults, following
// Core/ColorHexCodec.swift (components(hexString:), hexString(red:green:blue:
// alpha:), byte(_:)), the sasayaki colour lines of UserConfig.init in
// Core/UserConfig.swift and NativeMac/NativeReaderView.swift
// (sasayakiTextColor, sasayakiBackgroundColor, effectiveReaderColorScheme).

#include "models/sasayaki/sasayaki_settings.h"

#include <QTest>

#include <optional>
#include <string>

using niratan::sasayaki::sasayaki_color_hex;
using niratan::sasayaki::sasayaki_colors;
using niratan::sasayaki::SasayakiColors;
using niratan::sasayaki::SasayakiSettings;

class SasayakiSettingsTest : public QObject {
    Q_OBJECT

   private slots:
    void colorHex_data();
    void colorHex();
    void defaultsMatchUserConfig();
    void colorsChooseTheResolvedAppearance();
};

// B3: ColorHexCodec.components + hexString round trip.
void SasayakiSettingsTest::colorHex_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<bool>("decodes");

    QTest::newRow("six digits") << "#87CFFA"
                                << "#87CFFA" << true;
    QTest::newRow("six digits lower case") << "#87cffa"
                                           << "#87CFFA" << true;
    QTest::newRow("six digits without hash") << "87CFFA"
                                             << "#87CFFA" << true;
    QTest::newRow("eight digits keep alpha") << "#87CFFA66"
                                             << "#87CFFA66" << true;
    QTest::newRow("opaque alpha is omitted") << "#112233FF"
                                             << "#112233" << true;
    QTest::newRow("lower case opaque alpha is omitted") << "#112233ff" << "#112233" << true;
    QTest::newRow("zero alpha is kept") << "#00000000"
                                        << "#00000000" << true;
    QTest::newRow("three digits expand") << "#ABC"
                                         << "#AABBCC" << true;
    QTest::newRow("four digits expand") << "#a1b2"
                                        << "#AA11BB22" << true;
    QTest::newRow("whitespace is trimmed") << "  #AbCdEf\n\t"
                                           << "#ABCDEF" << true;
    QTest::newRow("eight digits without hash") << "11223344"
                                               << "#11223344" << true;
    QTest::newRow("empty") << ""
                           << "" << false;
    QTest::newRow("hash only") << "#"
                               << "" << false;
    QTest::newRow("spaces only") << "   "
                                 << "" << false;
    QTest::newRow("non-hex digit") << "#12G456"
                                   << "" << false;
    QTest::newRow("five digits") << "#12345"
                                 << "" << false;
    QTest::newRow("seven digits") << "#1234567"
                                  << "" << false;
    QTest::newRow("nine digits") << "#123456789"
                                 << "" << false;
    QTest::newRow("two hashes") << "##123456"
                                << "" << false;
    QTest::newRow("word") << "red"
                          << "" << false;
    QTest::newRow("full width digits") << "#１２３"
                                       << "" << false;
}

void SasayakiSettingsTest::colorHex() {
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QFETCH(bool, decodes);

    const std::optional<std::string> normalized = sasayaki_color_hex(input.toStdString());
    if (!decodes) {
        QVERIFY(!normalized.has_value());
        return;
    }
    QVERIFY(normalized.has_value());
    QCOMPARE(QString::fromStdString(*normalized), expected);
}

// B4: the defaults of the sasayaki colour lines in UserConfig.init.
void SasayakiSettingsTest::defaultsMatchUserConfig() {
    const SasayakiSettings settings;
    QCOMPARE(settings.enable, false);
    QCOMPARE(settings.auto_scroll, true);
    QCOMPARE(settings.auto_pause, true);
    QCOMPARE(settings.skip_controls, false);
    QCOMPARE(QString::fromStdString(settings.text_color), QStringLiteral("#000000"));
    QCOMPARE(QString::fromStdString(settings.background_color), QStringLiteral("#87CFFA66"));
    QCOMPARE(QString::fromStdString(settings.dark_text_color), QStringLiteral("#FFFFFF"));
    QCOMPARE(QString::fromStdString(settings.dark_background_color), QStringLiteral("#87CFFA66"));
    QCOMPARE(settings, SasayakiSettings{});
}

// B5: the pair follows effectiveReaderColorScheme and passes the stored
// strings through unchanged.
void SasayakiSettingsTest::colorsChooseTheResolvedAppearance() {
    QCOMPARE(sasayaki_colors(SasayakiSettings{}, false), (SasayakiColors{"#000000", "#87CFFA66"}));
    QCOMPARE(sasayaki_colors(SasayakiSettings{}, true), (SasayakiColors{"#FFFFFF", "#87CFFA66"}));

    SasayakiSettings settings;
    settings.text_color = "#101010";
    settings.background_color = "#20202080";
    settings.dark_text_color = "#abcdef";
    settings.dark_background_color = "#00000040";
    QCOMPARE(sasayaki_colors(settings, false), (SasayakiColors{"#101010", "#20202080"}));
    QCOMPARE(sasayaki_colors(settings, true), (SasayakiColors{"#abcdef", "#00000040"}));
}

QTEST_APPLESS_MAIN(SasayakiSettingsTest)
#include "tst_sasayaki_settings.moc"
