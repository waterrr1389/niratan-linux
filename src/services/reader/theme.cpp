// SPDX-License-Identifier: GPL-3.0-or-later

#include "theme.h"

namespace niratan::reader {
namespace {

// The sepia palettes that readerBackgroundColor and readerTextColor return.
constexpr char kSepiaDarkBackground[] = "#18150C";
constexpr char kSepiaDarkText[] = "#F2E2C9";
constexpr char kSepiaLightBackground[] = "#F2E2C9";
constexpr char kSepiaLightText[] = "#332A1B";

// Themes.colorScheme: only Light and Dark name an appearance, so every other
// ui_theme leaves Custom following the system.
bool custom_dark(const ThemeInput& input) {
    switch (input.ui_theme) {
        case Theme::light:
            return false;
        case Theme::dark:
            return true;
        default:
            return input.system_dark;
    }
}

// UserConfig.preferredColorScheme, then the system: Light and Dark force
// theirs, Custom takes its ui_theme's, and System and Sepia follow the system.
bool resolved_dark(const ThemeInput& input) {
    switch (input.theme) {
        case Theme::light:
            return false;
        case Theme::dark:
            return true;
        case Theme::custom:
            return custom_dark(input);
        default:
            return input.system_dark;
    }
}

}  // namespace

ResolvedTheme resolve_theme(const ThemeInput& input) {
    const bool dark = resolved_dark(input);

    // Dark sepia comes first, as in readerBackgroundColor; the light palette
    // is only reachable in a light appearance.
    if (input.theme == Theme::sepia && dark) {
        return {true, kSepiaDarkBackground, kSepiaDarkText};
    }
    if (input.theme == Theme::sepia ||
        (input.theme == Theme::system && input.system_light_sepia && !dark)) {
        return {false, kSepiaLightBackground, kSepiaLightText};
    }
    // Custom colors are stored, not derived from the appearance.
    if (input.theme == Theme::custom) {
        return {dark, input.custom_background, input.custom_text};
    }
    // The window background resolves against the reader's own appearance, so
    // Light in a dark system still gets the light one.
    return {dark, dark ? input.dark_window_background : input.light_window_background,
            std::nullopt};
}

}  // namespace niratan::reader
