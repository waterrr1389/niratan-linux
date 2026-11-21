// SPDX-License-Identifier: GPL-3.0-or-later
//
// ColorHexCodec.components(hexString:) and hexString(red:green:blue:alpha:).
//
// The codec counts and doubles UTF-16 code units instead of grapheme clusters;
// only ASCII hexadecimal digits can decode, so a non-ASCII value is rejected
// either way. Swift's UInt64(radix: 16) accepts a leading '+' in a six- or
// eight-digit string, while the value is defined as hexadecimal digits only
// and the port rejects it.

#include "sasayaki_settings.h"

#include <QChar>
#include <QString>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace niratan::sasayaki {
namespace {

int hex_nibble(QChar character) {
    const char16_t value = character.unicode();
    if (value >= u'0' && value <= u'9') {
        return value - u'0';
    }
    if (value >= u'a' && value <= u'f') {
        return value - u'a' + 10;
    }
    if (value >= u'A' && value <= u'F') {
        return value - u'A' + 10;
    }
    return -1;
}

}  // namespace

std::optional<std::string> sasayaki_color_hex(std::string_view value) {
    QString hex = QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())).trimmed();
    if (hex.startsWith(QLatin1Char('#'))) {
        hex.removeFirst();
    }
    if (hex.size() == 3 || hex.size() == 4) {
        QString expanded;
        expanded.reserve(hex.size() * 2);
        for (const QChar character : hex) {
            expanded.append(character);
            expanded.append(character);
        }
        hex = expanded;
    } else if (hex.size() != 6 && hex.size() != 8) {
        return std::nullopt;
    }

    std::array<int, 4> bytes{};
    const qsizetype byte_count = hex.size() / 2;
    for (qsizetype index = 0; index < byte_count; ++index) {
        const int high = hex_nibble(hex.at(index * 2));
        const int low = hex_nibble(hex.at(index * 2 + 1));
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        bytes[static_cast<std::size_t>(index)] = high * 16 + low;
    }

    QString encoded = QString::asprintf("#%02X%02X%02X", bytes[0], bytes[1], bytes[2]);
    if (byte_count == 4 && bytes[3] != 255) {
        encoded += QString::asprintf("%02X", bytes[3]);
    }
    return encoded.toStdString();
}

SasayakiColors sasayaki_colors(const SasayakiSettings& settings, bool dark) {
    if (dark) {
        return SasayakiColors{settings.dark_text_color, settings.dark_background_color};
    }
    return SasayakiColors{settings.text_color, settings.background_color};
}

}  // namespace niratan::sasayaki
