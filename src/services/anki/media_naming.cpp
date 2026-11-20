// SPDX-License-Identifier: GPL-3.0-or-later

#include "media_naming.h"

#include <QByteArray>
#include <QString>

#include <cstddef>
#include <string>
#include <string_view>

namespace niratan::anki {
namespace {

// Returned when the sanitized name has nothing left but separators.
constexpr std::string_view kDefaultMediaName = "hoshi_media";

// The characters of the Swift allowed set are all ASCII, so any byte at or
// above 0x80 is disallowed and consumes its whole UTF-8 sequence.
bool is_allowed_ascii(char byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
           (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.';
}

// Length of the UTF-8 sequence a lead byte starts. The input is valid UTF-8,
// so only well-formed lead bytes reach here.
std::size_t sequence_length(unsigned char lead) {
    if (lead < 0x80) {
        return 1;
    }
    if (lead < 0xE0) {
        return 2;
    }
    if (lead < 0xF0) {
        return 3;
    }
    return 4;
}

// URL(fileURLWithPath:).lastPathComponent ignores trailing '/' and keeps the
// text after the last remaining '/', or the whole string when the path has no
// separator. A path of only separators ends up empty here and gets the default
// name, as the "/" component does after replacement.
std::string_view last_path_component(std::string_view path) {
    while (!path.empty() && path.back() == '/') {
        path.remove_suffix(1);
    }
    const std::size_t separator = path.rfind('/');
    return separator == std::string_view::npos ? path : path.substr(separator + 1);
}

// One disallowed Unicode scalar becomes exactly one '_', never one per byte.
std::string replace_disallowed_scalars(std::string_view name) {
    std::string sanitized;
    sanitized.reserve(name.size());
    std::size_t pos = 0;
    while (pos < name.size()) {
        const unsigned char lead = static_cast<unsigned char>(name[pos]);
        if (lead < 0x80 && is_allowed_ascii(static_cast<char>(lead))) {
            sanitized.push_back(static_cast<char>(lead));
            ++pos;
            continue;
        }
        sanitized.push_back('_');
        const std::size_t rest = name.size() - pos;
        const std::size_t length = sequence_length(lead);
        pos += length < rest ? length : rest;
    }
    return sanitized;
}

bool is_trimmed_byte(char byte) { return byte == '.' || byte == '_'; }

// Only decides emptiness; the separators stay in the returned name.
bool is_empty_after_trimming(std::string_view name) {
    std::size_t begin = 0;
    std::size_t end = name.size();
    while (begin < end && is_trimmed_byte(name[begin])) {
        ++begin;
    }
    while (end > begin && is_trimmed_byte(name[end - 1])) {
        --end;
    }
    return begin == end;
}

}  // namespace

std::string safe_media_filename(std::string_view filename) {
    const std::string sanitized = replace_disallowed_scalars(last_path_component(filename));
    if (is_empty_after_trimming(sanitized)) {
        return std::string(kDefaultMediaName);
    }
    return sanitized;
}

std::string safe_media_extension(std::string_view extension, std::string_view fallback) {
    if (extension.empty()) {
        return std::string(fallback);
    }
    // Swift lowers the extension before sanitizing it, so non-ASCII letters
    // fold to their own characters first and are then replaced one by one.
    const QString lowered = QString::fromUtf8(extension.data(), static_cast<qsizetype>(extension.size()))
                                .toLower();
    const QByteArray lowered_utf8 = lowered.toUtf8();
    const std::string sanitized = safe_media_filename(
        std::string_view(lowered_utf8.constData(), static_cast<std::size_t>(lowered_utf8.size())));
    if (sanitized.empty() || sanitized.find('.') != std::string::npos) {
        return std::string(fallback);
    }
    return sanitized;
}

std::string direct_audio_markup(std::string_view filename) {
    return "[sound:" + safe_media_filename(filename) + "]";
}

std::string direct_image_markup(std::string_view filename) {
    return "<img src=\"" + safe_media_filename(filename) + "\">";
}

}  // namespace niratan::anki
