// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/theme.h"

#include <QtTest>

#include <optional>
#include <string>

using niratan::reader::ResolvedTheme;
using niratan::reader::Theme;
using niratan::reader::ThemeInput;
using niratan::reader::profile_color;
using niratan::reader::resolve_theme;
using niratan::reader::theme_from_id;

// Cases follow NativeReaderView.readerBackgroundColor, readerTextColor and
// effectiveReaderColorScheme, with the forced appearance from
// UserConfig.preferredColorScheme and Themes.colorScheme.
class ThemeTest : public QObject {
    Q_OBJECT

   private slots:
    void system_follows_the_system_appearance();
    void light_and_dark_force_the_appearance();
    void sepia_follows_the_system_appearance();
    void system_light_sepia_only_applies_while_light();
    void system_light_sepia_does_not_touch_forced_themes();
    void dark_sepia_wins_over_system_light_sepia();
    void custom_takes_the_appearance_from_ui_theme();
    void custom_colors_win_over_system_light_sepia();
    void stored_custom_palette_applies_css_colors();
};

namespace {

const std::string kLight = "#ECECEC";
const std::string kDark = "#1E1E1E";
const std::string kCustomBackground = "#123456";
const std::string kCustomText = "#ABCDEF";

ThemeInput base(Theme theme, bool system_dark) {
    ThemeInput input;
    input.theme = theme;
    input.system_dark = system_dark;
    input.light_window_background = kLight;
    input.dark_window_background = kDark;
    input.custom_background = kCustomBackground;
    input.custom_text = kCustomText;
    return input;
}

ResolvedTheme window(bool dark) { return {dark, dark ? kDark : kLight, std::nullopt}; }

ResolvedTheme custom(bool dark) { return {dark, kCustomBackground, kCustomText}; }

const ResolvedTheme kSepiaLight{false, "#F2E2C9", "#332A1B"};
const ResolvedTheme kSepiaDark{true, "#18150C", "#F2E2C9"};

}  // namespace

void ThemeTest::system_follows_the_system_appearance() {
    QCOMPARE(resolve_theme(base(Theme::system, false)), window(false));
    QCOMPARE(resolve_theme(base(Theme::system, true)), window(true));
}

// UserConfig.swift (theme, customBackgroundColor, customTextColor).
void ThemeTest::stored_custom_palette_applies_css_colors() {
    ThemeInput input;
    input.theme = theme_from_id("Custom");
    input.custom_background = profile_color("#123456ab");
    input.custom_text = profile_color("#abcdef");
    const auto palette = resolve_theme(input);
    QCOMPARE(palette.background, std::string("#123456ab"));
    QCOMPARE(palette.text, std::optional<std::string>("#abcdef"));
    QVERIFY(profile_color("#123456;").empty());
    QCOMPARE(theme_from_id("unknown"), Theme::system);
}

void ThemeTest::light_and_dark_force_the_appearance() {
    QCOMPARE(resolve_theme(base(Theme::light, true)), window(false));
    QCOMPARE(resolve_theme(base(Theme::light, false)), window(false));
    QCOMPARE(resolve_theme(base(Theme::dark, false)), window(true));
    QCOMPARE(resolve_theme(base(Theme::dark, true)), window(true));
}

void ThemeTest::sepia_follows_the_system_appearance() {
    QCOMPARE(resolve_theme(base(Theme::sepia, false)), kSepiaLight);
    QCOMPARE(resolve_theme(base(Theme::sepia, true)), kSepiaDark);
}

void ThemeTest::system_light_sepia_only_applies_while_light() {
    auto input = base(Theme::system, false);
    input.system_light_sepia = true;
    QCOMPARE(resolve_theme(input), kSepiaLight);

    input.system_dark = true;
    QCOMPARE(resolve_theme(input), window(true));
}

void ThemeTest::system_light_sepia_does_not_touch_forced_themes() {
    auto light = base(Theme::light, false);
    light.system_light_sepia = true;
    QCOMPARE(resolve_theme(light), window(false));

    auto dark = base(Theme::dark, false);
    dark.system_light_sepia = true;
    QCOMPARE(resolve_theme(dark), window(true));
}

void ThemeTest::dark_sepia_wins_over_system_light_sepia() {
    auto input = base(Theme::sepia, true);
    input.system_light_sepia = true;
    QCOMPARE(resolve_theme(input), kSepiaDark);
}

void ThemeTest::custom_takes_the_appearance_from_ui_theme() {
    auto input = base(Theme::custom, true);
    input.ui_theme = Theme::light;
    QCOMPARE(resolve_theme(input), custom(false));

    input = base(Theme::custom, false);
    input.ui_theme = Theme::dark;
    QCOMPARE(resolve_theme(input), custom(true));

    // Themes.colorScheme is nil for anything but Light and Dark.
    for (const auto ui : {Theme::system, Theme::sepia, Theme::custom}) {
        input = base(Theme::custom, true);
        input.ui_theme = ui;
        QCOMPARE(resolve_theme(input), custom(true));
        input.system_dark = false;
        QCOMPARE(resolve_theme(input), custom(false));
    }
}

void ThemeTest::custom_colors_win_over_system_light_sepia() {
    auto input = base(Theme::custom, false);
    input.system_light_sepia = true;
    QCOMPARE(resolve_theme(input), custom(false));
}

QTEST_APPLESS_MAIN(ThemeTest)
#include "tst_theme.moc"
