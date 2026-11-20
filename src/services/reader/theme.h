// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reader theme resolution. Values and the decision order come from
// NativeMac/NativeReaderView.swift (readerBackgroundColor, readerTextColor)
// and Core/UserConfig.swift (Themes, usesDarkSepia, preferredColorScheme).
//
// Sepia follows the system appearance: dark sepia whenever the system is
// dark. UserConfig.sepiaInvertInDark is a legacy toggle kept only for stored
// settings, and usesDarkSepia ignores it, so it is not part of this input.
// System and Custom colors that come from the platform (window background,
// primary, secondary) are passed in already resolved; this function only
// decides which source wins.

#pragma once

#include <optional>
#include <string>

namespace niratan::reader {

enum class Theme { system, light, dark, sepia, custom };

struct ThemeInput {
    Theme theme = Theme::system;
    bool system_dark = false;
    // Themes.system with systemLightSepia uses the light sepia palette while
    // the system is light, and the system palette otherwise.
    bool system_light_sepia = false;
    // Platform colors, already resolved for the effective appearance. Custom
    // uses its own stored colors instead.
    std::string system_background;
    std::string system_text;
    std::string custom_background;
    std::string custom_text;
};

struct ResolvedTheme {
    std::string background;
    std::string text;

    bool operator==(const ResolvedTheme&) const = default;
};

// readerBackgroundColor checks sepia inversion before the theme, so Sepia in
// a dark system appearance wins even when system_light_sepia is set.
ResolvedTheme resolve_theme(const ThemeInput& input);

}  // namespace niratan::reader
