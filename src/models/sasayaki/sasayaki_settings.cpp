// SPDX-License-Identifier: GPL-3.0-or-later
//
// ColorHexCodec.components(hexString:) and hexString(red:green:blue:alpha:),
// through the shared codec in models/color/color_hex.h. sasayaki_color_hex
// keeps this module's signature; the normalization and the two documented
// Swift differences live with the shared codec.

#include "sasayaki_settings.h"

#include <optional>
#include <string>
#include <string_view>

#include "models/color/color_hex.h"

namespace niratan::sasayaki {

std::optional<std::string> sasayaki_color_hex(std::string_view value) {
    const std::optional<color::Rgba> parsed = color::parse_hex(value);
    if (!parsed) {
        return std::nullopt;
    }
    return color::hex_string(*parsed);
}

SasayakiColors sasayaki_colors(const SasayakiSettings& settings, bool dark) {
    if (dark) {
        return SasayakiColors{settings.dark_text_color, settings.dark_background_color};
    }
    return SasayakiColors{settings.text_color, settings.background_color};
}

}  // namespace niratan::sasayaki
