// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Sasayaki preferences in the global settings document, following
// Core/UserConfig.swift (enableSasayaki, sasayakiAutoScroll,
// sasayakiAutoPause, sasayakiSkipControls, the four sasayaki colour
// properties, their didSet observers and the matching UserConfig.init lines),
// Core/ColorHexCodec.swift (hexString(red:green:blue:alpha:) and
// components(hexString:)) and NativeMac/NativeReaderView.swift
// (sasayakiTextColor, sasayakiBackgroundColor, effectiveReaderColorScheme).
//
// A colour is stored as the normalized hex string ColorHexCodec produces from
// the decoded value: trim leading and trailing whitespace and newlines, drop
// one leading '#', expand 3 or 4 digits by doubling each, accept only 3, 4, 6
// or 8 hexadecimal digits (0-9, a-f, A-F), and re-encode as '#' followed by
// the uppercase two-digit hex of each byte. A 6-digit value has alpha 255, and
// an alpha of 255 is omitted; every other alpha is written. Text that does not
// decode has no normalized value, and the caller applies that key's default:
// #000000 for the text colour, #87CFFA66 for both backgrounds and #FFFFFF for
// the dark text colour. Swift resolves a Color through EnvironmentValues and
// may produce values outside sRGB; the port stores hex strings, so
// ColorHexCodec.byte()'s clamp has nothing to act on here.
//
// The reader receives the light colours (#000000, #87CFFA66 by default) or the
// dark ones (#FFFFFF, #87CFFA66) for the resolved appearance, and both strings
// pass through unchanged. The values live in the global settings document, not
// in a reader profile (Decision D3 in notes/specs/sasayaki-gap.md).

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace niratan::sasayaki {

struct SasayakiSettings {
    bool enable = false;
    bool auto_scroll = true;
    bool auto_pause = true;
    bool skip_controls = false;
    std::string text_color = "#000000";
    std::string background_color = "#87CFFA66";
    std::string dark_text_color = "#FFFFFF";
    std::string dark_background_color = "#87CFFA66";

    bool operator==(const SasayakiSettings&) const = default;
};

// The pair the reader highlights cues with for the resolved appearance.
struct SasayakiColors {
    std::string text;
    std::string background;

    bool operator==(const SasayakiColors&) const = default;
};

// ColorHexCodec.components + hexString: the normalized value, or nullopt when
// the text does not decode.
std::optional<std::string> sasayaki_color_hex(std::string_view value);

// The pair for the resolved appearance.
SasayakiColors sasayaki_colors(const SasayakiSettings& settings, bool dark);

}  // namespace niratan::sasayaki
