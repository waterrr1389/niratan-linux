// SPDX-License-Identifier: GPL-3.0-or-later
//
// ReaderSearchEngine.swift (ReaderSearchIndex, ReaderSearchTextFilter) and
// ReaderSearchEPUBAdapter.swift (ReaderSearchDocument.init): scan in spine
// order, never match across spine boundaries, and keep snippet offsets separate
// from readable-character offsets. As elsewhere in the reader, count scalars
// rather than Swift grapheme clusters. Snippet offsets address UTF-16 for Qt.

#pragma once

#include <QString>

#include <filesystem>
#include <optional>
#include <vector>

namespace niratan::reader {

struct SearchChapter {
    int spine_index;
    int character_start;
    QString label;
    QString markup;
};

struct SearchHit {
    int spine_index;
    int character;
    QString label;
    QString snippet;
    int match_start;
    int match_end;
    bool operator==(const SearchHit&) const = default;
};

bool has_searchable_text(const QString& query);
std::vector<SearchHit> search_book(const std::vector<SearchChapter>& chapters, const QString& query,
                                   int limit = 1000);
std::optional<std::vector<SearchHit>> search_epub(const std::filesystem::path& path,
                                                const QString& query);

}  // namespace niratan::reader
