// SPDX-License-Identifier: GPL-3.0-or-later

#include "ttu_naming.h"

#include <QByteArrayView>

#include <algorithm>
#include <charconv>
#include <cmath>

namespace niratan::sync {
namespace {

constexpr std::string_view star = "~ttu-star~";
constexpr std::string_view dot = "~ttu-dend~";
constexpr std::string_view space = "~ttu-spc~";

int hex_digit(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    return -1;
}

std::string percent_decode(std::string_view title) {
    std::string result;
    for (size_t i = 0; i < title.size(); ++i) {
        if (title[i] != '%') {
            result += title[i];
            continue;
        }
        if (title.size() - i < 3) {
            return std::string(title);
        }
        const int high = hex_digit(title[i + 1]);
        const int low = hex_digit(title[i + 2]);
        if (high < 0 || low < 0) {
            return std::string(title);
        }
        result += static_cast<char>(high * 16 + low);
        i += 2;
    }
    return QByteArrayView(result.data(), result.size()).isValidUtf8() ? result : std::string(title);
}

std::string describe_double(double value) {
    char buffer[64];
    const auto magnitude = std::abs(value);
    const auto format = magnitude >= 1e16 || (magnitude != 0 && magnitude < 1e-4)
        ? std::chars_format::scientific
        : std::chars_format::fixed;
    const auto number = std::to_chars(std::begin(buffer), std::end(buffer), value, format);
    std::string result(buffer, number.ptr);
    if (std::isfinite(value) && result.find_first_of(".e") == std::string::npos) {
        result += ".0";
    }
    return result;
}

}  // namespace

std::string sanitize_ttu_title(std::string_view title) {
    const auto suffix = title.ends_with(' ') ? space
        : title.ends_with('.')               ? dot
                                             : std::string_view{};
    if (!suffix.empty()) {
        title.remove_suffix(1);
    }
    std::string result;
    for (const char byte : title) {
        if (byte == '*') {
            result += star;
        } else if (std::string_view("/?<>\\:|%\"").find(byte) != std::string_view::npos) {
            constexpr char hex[] = "0123456789ABCDEF";
            result += '%';
            result += hex[byte >> 4];
            result += hex[byte & 15];
        } else {
            result += byte;
        }
    }
    return result.append(suffix);
}

std::string desanitize_ttu_title(std::string_view title) {
    auto result = percent_decode(title);
    for (const auto& [marker, value] : {std::pair{star, '*'}, {dot, '.'}, {space, ' '}}) {
        size_t index = 0;
        while ((index = result.find(marker, index)) != std::string::npos) {
            result.replace(index, marker.size(), 1, value);
            ++index;
        }
    }
    return result;
}

std::string ttu_statistics_file_name(std::span<const statistics::Statistics> rows) {
    double time = 0;
    std::int64_t characters = 0;
    std::int64_t weighted = 0;
    std::int64_t modified = 0;
    int min_speed = 0;
    int alt_min_speed = 0;
    int max_speed = 0;
    std::int64_t days = 0;
    for (const auto& row : rows) {
        time += row.reading_time;
        characters += row.characters_read;
        min_speed =
            min_speed > 0 ? std::min(min_speed, row.min_reading_speed) : row.min_reading_speed;
        alt_min_speed = alt_min_speed > 0 ? std::min(alt_min_speed, row.alt_min_reading_speed)
                                          : row.alt_min_reading_speed;
        max_speed = std::max(max_speed, row.last_reading_speed);
        weighted += static_cast<std::int64_t>(row.reading_time) * row.characters_read;
        modified = std::max(modified, row.last_statistic_modified);
        if (row.reading_time > 0) {
            ++days;
        }
    }
    const auto average = [](double numerator, double denominator) {
        return denominator > 0 ? std::ceil(numerator / denominator) : 0.0;
    };
    const double avg_time = average(time, days);
    const double avg_weighted_time = average(weighted, characters);
    const double avg_characters = average(characters, days);
    const double avg_weighted_characters = average(weighted, time);
    const double speed = average(3600.0 * characters, time);
    const double avg_speed = average(3600.0 * avg_characters, avg_time);
    const double avg_weighted_speed = average(3600.0 * avg_weighted_characters, avg_weighted_time);
    return "statistics_1_6_" + std::to_string(modified) + "_" + std::to_string(characters) + "_" +
        describe_double(time) + "_" + std::to_string(min_speed) + "_" +
        std::to_string(alt_min_speed) + "_" + describe_double(speed) + "_" +
        std::to_string(max_speed) + "_" + describe_double(avg_time) + "_" +
        describe_double(avg_weighted_time) + "_" + describe_double(avg_characters) + "_" +
        describe_double(avg_weighted_characters) + "_" + describe_double(avg_speed) + "_" +
        describe_double(avg_weighted_speed) + "_na.json";
}

}  // namespace niratan::sync
