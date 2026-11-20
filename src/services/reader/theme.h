// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reader theme resolution. Values and the decision order come from
// NativeMac/NativeReaderView.swift (readerBackgroundColor, readerTextColor,
// readerBackgroundHex, effectiveReaderColorScheme) and Core/UserConfig.swift
// (Themes, preferredColorScheme, usesDarkSepia).
//
// The reader window first gets an appearance: Light and Dark force theirs,
// Custom forces the one its ui_theme names (Light or Dark, otherwise the
// system's), System and Sepia follow the system. Every color below is then
// chosen for that appearance, so Light in a dark system still gets the light
// window background.
//
// Sepia uses its dark palette whenever that appearance is dark.
// UserConfig.sepiaInvertInDark is a legacy toggle that usesDarkSepia ignores,
// so it is not an input.
//
// Colors are "#RRGGBB" strings and pass through unchanged; this function does
// not validate or normalize them.

#pragma once

#include <optional>
#include <string>

namespace niratan::reader {

enum class Theme { system, light, dark, sepia, custom };

struct ThemeInput {
    Theme theme = Theme::system;
    // Only read when theme is custom. It decides the appearance and nothing
    // else: custom colors are stored, not derived from the appearance.
    Theme ui_theme = Theme::system;
    bool system_dark = false;
    // Themes.system with systemLightSepia uses the light sepia palette while
    // the appearance is light, and the window background otherwise.
    bool system_light_sepia = false;
    // The platform window background in each appearance. The one matching
    // the resolved appearance is used for System, Light and Dark.
    std::string light_window_background;
    std::string dark_window_background;
    std::string custom_background;
    std::string custom_text;
};

struct ResolvedTheme {
    // The appearance the reader window renders in. Callers apply it to the
    // window and to the web view's prefers-color-scheme.
    bool dark = false;
    std::string background;
    // nullopt leaves the text color to the page stylesheet, which uses #000
    // in a light appearance and #fff in a dark one (injectReader's
    // --hoshi-text-color defaults). System, Light and Dark return nullopt.
    std::optional<std::string> text;

    bool operator==(const ResolvedTheme&) const = default;
};

// readerBackgroundColor checks dark sepia before the theme, then light sepia,
// then Custom, then the window background.
ResolvedTheme resolve_theme(const ThemeInput& input);

}  // namespace niratan::reader
