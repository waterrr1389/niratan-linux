// SPDX-License-Identifier: GPL-3.0-or-later
//
// ColorHexCodec.swift: hexString(red:green:blue:alpha:), components(hexString:)
// and byte(_:), shared by every stored colour. Components are bytes 0-255,
// which is what the stored strings and QColor both carry; Swift works on
// CGFloat 0...1 and this is the byte equivalent.
//
// hex_string clamps each byte to 0...255 and writes '#' followed by the
// uppercase two-digit hex of red, green and blue, plus alpha only when it is
// not 255. parse_hex trims whitespace, drops one leading '#', accepts 3, 4, 6
// or 8 hexadecimal digits, doubles 3- and 4-digit values, and returns nil
// otherwise; a 6-digit value has alpha 255. The reader of a stored value
// applies the key's default when parse fails (UserConfig.loadColor(key:) ??
// default).
//
// Two Swift runtime details are not reproduced because only ASCII hex digits
// can decode: Swift counts the input in UTF-16 code units rather than
// grapheme clusters, and UInt64(radix: 16) accepts a leading '+' in a six- or
// eight-digit string, which this rejects as non-hexadecimal.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace niratan::color {

struct Rgba {
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 255;

    bool operator==(const Rgba&) const = default;
};

// The decoded bytes, or nullopt when the text is not a colour string.
std::optional<Rgba> parse_hex(std::string_view value);

// The stored form: uppercase #RRGGBB, or #RRGGBBAA when alpha is not 255.
std::string hex_string(const Rgba& color);

}  // namespace niratan::color
