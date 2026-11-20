// SPDX-License-Identifier: GPL-3.0-or-later

#include "text_selection.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace niratan::dictionary {
namespace {

// Mirrors Core/SelectionLookup/TextSelectionResolver.swift.
constexpr std::u16string_view kEnglishScanDelimiters =
    u"\"“”„‟'‘’‚‛«»‹›!?—–-‐‑‒/\\|@#$%^&*_+=~`<>";
constexpr std::u16string_view kEnglishWordInternalDelimiters = u"'’`-‐‑";
constexpr std::u16string_view kSharedScanDelimiters =
    u"。、！？…‥「」『』（）()【】〈〉《》〔〕｛｝{}［］[]・：；:;，,.─\n\r";

bool is_whitespace_or_newline(char16_t unit) {
    // CharacterSet.whitespacesAndNewlines: tab, newlines, and Unicode Zs.
    return (unit >= 0x0009 && unit <= 0x000D) || unit == 0x0020 || unit == 0x0085 ||
           unit == 0x00A0 || unit == 0x1680 || (unit >= 0x2000 && unit <= 0x200A) ||
           unit == 0x2028 || unit == 0x2029 || unit == 0x202F || unit == 0x205F ||
           unit == 0x3000;
}

bool is_ascii_alphanumeric(char16_t unit) {
    return (unit >= u'0' && unit <= u'9') || (unit >= u'A' && unit <= u'Z') ||
           (unit >= u'a' && unit <= u'z');
}

bool is_internal_word_delimiter(const std::u16string& units, int offset) {
    if (kEnglishWordInternalDelimiters.find(units[static_cast<std::size_t>(offset)]) ==
        std::u16string_view::npos) {
        return false;
    }
    if (offset <= 0 || offset + 1 >= static_cast<int>(units.size())) return false;
    return is_ascii_alphanumeric(units[static_cast<std::size_t>(offset - 1)]) &&
           is_ascii_alphanumeric(units[static_cast<std::size_t>(offset + 1)]);
}

bool is_english_hit_boundary(const std::u16string& units, int offset) {
    const char16_t unit = units[static_cast<std::size_t>(offset)];
    if (is_whitespace_or_newline(unit)) return true;
    if (kSharedScanDelimiters.find(unit) != std::u16string_view::npos) return true;
    if (kEnglishScanDelimiters.find(unit) == std::u16string_view::npos) return false;
    return !is_internal_word_delimiter(units, offset);
}

int english_word_start(const std::u16string& units, int utf16_offset) {
    int offset = utf16_offset;
    while (offset > 0 && !is_english_hit_boundary(units, offset - 1)) --offset;
    return offset;
}

void append_utf8_as_utf16(std::u16string& out, std::string_view input) {
    std::size_t index = 0;
    while (index < input.size()) {
        const auto lead = static_cast<unsigned char>(input[index]);
        char32_t code_point = 0;
        std::size_t width = 0;
        if (lead < 0x80) {
            code_point = lead;
            width = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            code_point = lead & 0x1F;
            width = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            code_point = lead & 0x0F;
            width = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            code_point = lead & 0x07;
            width = 4;
        } else {
            out.push_back(u'\uFFFD');
            ++index;
            continue;
        }

        if (index + width > input.size()) {
            out.push_back(u'\uFFFD');
            ++index;
            continue;
        }

        bool valid = true;
        for (std::size_t offset = 1; offset < width; ++offset) {
            const auto continuation = static_cast<unsigned char>(input[index + offset]);
            if ((continuation & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            code_point = (code_point << 6) | (continuation & 0x3F);
        }

        static constexpr char32_t kMinimumCodePoint[] = {0, 0, 0x80, 0x800, 0x10000};
        if (!valid || code_point < kMinimumCodePoint[width] || code_point > 0x10FFFF ||
            (code_point >= 0xD800 && code_point <= 0xDFFF)) {
            out.push_back(u'\uFFFD');
            ++index;
            continue;
        }

        if (code_point < 0x10000) {
            out.push_back(static_cast<char16_t>(code_point));
        } else {
            const char32_t supplementary = code_point - 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (supplementary >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (supplementary & 0x3FF)));
        }
        index += width;
    }
}

std::u16string utf8_to_utf16(std::string_view input) {
    std::u16string result;
    result.reserve(input.size());
    append_utf8_as_utf16(result, input);
    return result;
}

void append_utf16_as_utf8(std::string& out, std::u16string_view input) {
    std::size_t index = 0;
    while (index < input.size()) {
        char32_t code_point = input[index];
        ++index;
        if (code_point >= 0xD800 && code_point <= 0xDBFF && index < input.size()) {
            const char16_t low = input[index];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
                ++index;
            } else {
                code_point = 0xFFFD;
            }
        } else if (code_point >= 0xD800 && code_point <= 0xDFFF) {
            code_point = 0xFFFD;
        }

        if (code_point < 0x80) {
            out.push_back(static_cast<char>(code_point));
        } else if (code_point < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
            out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
        } else if (code_point < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
        }
    }
}

std::string utf16_to_utf8(std::u16string_view input) {
    std::string result;
    result.reserve(input.size());
    append_utf16_as_utf8(result, input);
    return result;
}

}  // namespace

std::optional<TextLookupCandidate> lookup_candidate(std::string_view sentence, int utf16_offset,
                                                    int scan_length, std::string_view language_id) {
    const std::u16string units = utf8_to_utf16(sentence);
    const auto unit_count = static_cast<int>(units.size());
    if (utf16_offset < 0 || utf16_offset >= unit_count || scan_length <= 0) {
        return std::nullopt;
    }

    const bool english = language_id == "en";
    const int candidate_start = english ? english_word_start(units, utf16_offset) : utf16_offset;
    const int length = std::min(scan_length, unit_count - candidate_start);
    const std::u16string_view raw(units.data() + candidate_start,
                                  static_cast<std::size_t>(length));

    std::size_t first = 0;
    std::size_t last = raw.size();
    while (first < last && is_whitespace_or_newline(raw[first])) ++first;
    while (last > first && is_whitespace_or_newline(raw[last - 1])) --last;
    if (first == last) return std::nullopt;

    TextLookupCandidate candidate;
    candidate.text = utf16_to_utf8(raw.substr(first, last - first));
    candidate.utf16_start = candidate_start + static_cast<int>(first);
    return candidate;
}

std::optional<HighlightRange> highlight_range(const TextLookupCandidate& candidate,
                                              std::string_view matched_text) {
    const int candidate_length = static_cast<int>(utf8_to_utf16(candidate.text).size());
    const int matched_length = static_cast<int>(utf8_to_utf16(matched_text).size());
    const int length = std::min(candidate_length, matched_length);
    if (length <= 0) return std::nullopt;
    return HighlightRange{candidate.utf16_start, length};
}

}  // namespace niratan::dictionary
