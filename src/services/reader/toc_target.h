// SPDX-License-Identifier: GPL-3.0-or-later
//
// TOC href parsing and fragment-offset scanning for BookSession::open,
// following Features/Reader/ReaderChapterIndex.swift: target(from:),
// normalizedChapterPath(_:), fragmentOffsets(chapterMarkup:fragments:),
// attributeValues(_:), and decodedFragmentIdentifier(_:).
//
// split_target normalizes and percent-decodes the path, strips the query
// string, and removes leading "./" prefixes. The fragment is kept raw so the
// call site can decode it once into the form ContentsEntry stores.
//
// fragment_offsets maps each decoded fragment to its readable character offset
// inside one chapter's markup. Every fragment not found in the markup gets
// offset 0, as ReaderChapterIndex does.

#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace niratan::reader {

struct TocTarget {
    std::string path;
    std::optional<std::string> fragment;
};

// Splits href at the first '#', strips the query string from the path, removes
// leading "./" prefixes, and percent-decodes the path.
TocTarget split_target(const std::string& href);

// Percent-decodes a UTF-8 string following URL decoding rules.
std::string percent_decoded(std::string_view text);

// Returns each fragment's readable character offset inside markup.
std::map<std::string, int> fragment_offsets(std::string_view markup,
                                            const std::set<std::string>& fragments);

}  // namespace niratan::reader
