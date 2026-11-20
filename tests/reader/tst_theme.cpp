// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/theme.h"

#include <QtTest>

using niratan::reader::ResolvedTheme;
using niratan::reader::Theme;
using niratan::reader::ThemeInput;
using niratan::reader::resolve_theme;

// Cases follow NativeReaderView.readerBackgroundColor and readerTextColor,
// with the decision order from UserConfig.usesDarkSepia.
class ThemeTest : public QObject {
    Q_OBJECT

   private slots:
    void system_follows_the_platform();
    void light_and_dark_force_the_platform_palette();
    void sepia_follows_the_system_appearance();
    void system_light_sepia_only_applies_while_light();
    void dark_sepia_wins_over_system_light_sepia();
    void custom_uses_its_own_colors();
};

ThemeInput base() {
    ThemeInput input;
    input.system_background = "#system-bg";
    input.system_text = "#system-text";
    input.custom_background = "#custom-bg";
    input.custom_text = "#custom-text";
    return input;
}

void ThemeTest::system_follows_the_platform() {
    auto input = base();
    input.theme = Theme::system;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#system-bg", "#system-text"}));

    input.system_dark = true;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#system-bg", "#system-text"}));
}

void ThemeTest::light_and_dark_force_the_platform_palette() {
    auto input = base();
    input.theme = Theme::light;
    input.system_dark = true;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#system-bg", "#system-text"}));

    input.theme = Theme::dark;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#system-bg", "#system-text"}));
}

void ThemeTest::sepia_follows_the_system_appearance() {
    auto input = base();
    input.theme = Theme::sepia;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#F2E2C9", "#332A1B"}));

    input.system_dark = true;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#18150C", "#F2E2C9"}));
}

void ThemeTest::system_light_sepia_only_applies_while_light() {
    auto input = base();
    input.theme = Theme::system;
    input.system_light_sepia = true;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#F2E2C9", "#332A1B"}));

    input.system_dark = true;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#system-bg", "#system-text"}));
}

void ThemeTest::dark_sepia_wins_over_system_light_sepia() {
    auto input = base();
    input.theme = Theme::sepia;
    input.system_dark = true;
    input.system_light_sepia = true;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#18150C", "#F2E2C9"}));
}

void ThemeTest::custom_uses_its_own_colors() {
    auto input = base();
    input.theme = Theme::custom;
    input.system_dark = true;
    QCOMPARE(resolve_theme(input), (ResolvedTheme{"#custom-bg", "#custom-text"}));
}

QTEST_APPLESS_MAIN(ThemeTest)
#include "tst_theme.moc"
