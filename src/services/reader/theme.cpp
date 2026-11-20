// SPDX-License-Identifier: GPL-3.0-or-later

#include "theme.h"

namespace niratan::reader {
namespace {

// NativeReaderView.readerBackgroundColor / readerTextColor, dark variant.
constexpr const char* kSepiaDarkBackground = "#18150C";
constexpr const char* kSepiaDarkText = "#F2E2C9";

// Same symbols, light variant.
constexpr const char* kSepiaLightBackground = "#F2E2C9";
constexpr const char* kSepiaLightText = "#332A1B";

bool uses_light_sepia(const ThemeInput& input) {
    return input.theme == Theme::sepia ||
           (input.theme == Theme::system && input.system_light_sepia && !input.system_dark);
}

}  // namespace

ResolvedTheme resolve_theme(const ThemeInput& input) {
    // usesDarkSepia: only the Sepia theme inverts, and only in a dark
    // appearance. The legacy sepiaInvertInDark setting does not apply.
    if (input.theme == Theme::sepia && input.system_dark) {
        return {kSepiaDarkBackground, kSepiaDarkText};
    }
    if (uses_light_sepia(input)) {
        return {kSepiaLightBackground, kSepiaLightText};
    }
    if (input.theme == Theme::custom) {
        return {input.custom_background, input.custom_text};
    }
    return {input.system_background, input.system_text};
}

}  // namespace niratan::reader
