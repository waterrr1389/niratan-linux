// SPDX-License-Identifier: GPL-3.0-or-later
//
// ColorHexCodec.swift. See color_hex.h for the accepted input and the stored
// form.

#include "color_hex.h"

#include <QChar>
#include <QString>

#include <algorithm>
#include <array>
#include <cstddef>

namespace niratan::color {
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

int byte(int value) {
    return std::clamp(value, 0, 255);
}

}  // namespace

std::optional<Rgba> parse_hex(std::string_view value) {
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

    std::array<int, 4> bytes{0, 0, 0, 255};
    const qsizetype byte_count = hex.size() / 2;
    for (qsizetype index = 0; index < byte_count; ++index) {
        const int high = hex_nibble(hex.at(index * 2));
        const int low = hex_nibble(hex.at(index * 2 + 1));
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        bytes[static_cast<std::size_t>(index)] = high * 16 + low;
    }
    return Rgba{bytes[0], bytes[1], bytes[2], bytes[3]};
}

std::string hex_string(const Rgba& color) {
    const QString encoded =
        QString::asprintf("#%02X%02X%02X", byte(color.red), byte(color.green), byte(color.blue)) +
        (byte(color.alpha) == 255 ? QString() : QString::asprintf("%02X", byte(color.alpha)));
    return encoded.toStdString();
}

}  // namespace niratan::color
