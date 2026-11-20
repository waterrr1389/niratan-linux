// SPDX-License-Identifier: GPL-3.0-or-later

#include "glossary_markup.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace niratan::anki {
namespace {

// <li data-dictionary=", including the opening quote of the name.
constexpr std::string_view kHeaderOpen = "<li data-dictionary=\"";
// Closes the name and the tag; the bare header ends here.
constexpr std::string_view kNameClose = "\">";
constexpr std::string_view kLabelOpen = "<i>";
// The space after </i> is part of the header, so a header without it stays.
constexpr std::string_view kLabelClose = "</i> ";

struct Header {
    std::string_view name;
    std::string_view label;
    std::size_t end = 0;
};

// Parses the header that starts at `start`, the way the Swift expressions do:
// the name runs up to the first '"' and the label up to the first '<'.
// `require_name` selects the non-empty name of stripDictionaryName's [^"]+
// over the possibly empty name of stripGlossaryHeaders' [^"]*.
std::optional<Header> parse_header(std::string_view html, std::size_t start, bool require_name) {
    if (html.substr(start, kHeaderOpen.size()) != kHeaderOpen) {
        return std::nullopt;
    }
    const std::size_t name_begin = start + kHeaderOpen.size();
    const std::size_t quote = html.find('"', name_begin);
    if (quote == std::string_view::npos || (require_name && quote == name_begin)) {
        return std::nullopt;
    }
    if (html.substr(quote, kNameClose.size()) != kNameClose ||
        html.substr(quote + kNameClose.size(), kLabelOpen.size()) != kLabelOpen) {
        return std::nullopt;
    }
    const std::size_t label_begin = quote + kNameClose.size() + kLabelOpen.size();
    const std::size_t label_end = html.find('<', label_begin);
    if (label_end == std::string_view::npos ||
        html.substr(label_end, kLabelClose.size()) != kLabelClose) {
        return std::nullopt;
    }
    return Header{html.substr(name_begin, quote - name_begin),
                  html.substr(label_begin, label_end - label_begin),
                  label_end + kLabelClose.size()};
}

// Replaces literal occurrences left to right, non-overlapping, like
// replacingOccurrences(of:with:).
void replace_all(std::string& text, std::string_view needle, std::string_view replacement) {
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        text.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
}

}  // namespace

std::string strip_glossary_headers(std::string_view html) {
    std::string stripped;
    stripped.reserve(html.size());
    std::size_t pos = 0;
    while (pos < html.size()) {
        const std::optional<Header> header = parse_header(html, pos, false);
        if (!header) {
            stripped.push_back(html[pos]);
            ++pos;
            continue;
        }
        stripped.append(kHeaderOpen);
        stripped.append(header->name);
        stripped.append(kNameClose);
        pos = header->end;
    }
    return stripped;
}

std::string strip_dictionary_name(std::string_view html) {
    std::string stripped;
    stripped.reserve(html.size());
    std::size_t pos = 0;
    while (pos < html.size()) {
        const std::optional<Header> header = parse_header(html, pos, true);
        if (!header) {
            stripped.push_back(html[pos]);
            ++pos;
            continue;
        }
        std::string needle;
        needle.reserve(header->name.size() + 3);
        needle.append(", ");
        needle.append(header->name);
        needle.push_back(')');

        std::string label(header->label);
        replace_all(label, needle, ")");

        std::string only_name;
        only_name.reserve(header->name.size() + 2);
        only_name.push_back('(');
        only_name.append(header->name);
        only_name.push_back(')');

        stripped.append(kHeaderOpen);
        stripped.append(header->name);
        stripped.append(kNameClose);
        if (label != only_name) {
            stripped.append(kLabelOpen);
            stripped.append(label);
            stripped.append(kLabelClose);
        }
        pos = header->end;
    }
    return stripped;
}

}  // namespace niratan::anki
