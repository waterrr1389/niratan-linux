// SPDX-License-Identifier: GPL-3.0-or-later
//
// ReaderSearchEngine.swift (ReaderSearchIndex, ReaderSearchTextFilter) and
// ReaderSearchEPUBAdapter.swift (ReaderSearchDocument.init): scan in spine
// order, never match across spine boundaries, and keep snippet offsets separate
// from readable-character offsets. As elsewhere in the reader, count scalars
// rather than Swift grapheme clusters. Snippet offsets address UTF-16 for Qt.
//
// Chapter labels deliberately differ from ReaderSearchEPUBAdapter.swift
// (chapterLabels) and ReaderGoToView.swift (groupedSearchResults), which
// label a spine with its top-level contents entry and group per spine. Here
// the deepest entry naming a spine labels it, so a nested book shows its
// chapters rather than only its parts; a spine without an entry inherits the
// label before it, as label(for:) does; and search_group_counts groups
// consecutive hits by label, so a chapter split into several files shows one
// header. Labels are display text only and reach no card, query or file.

#pragma once

#include <QString>

#include <optional>
#include <string_view>
#include <vector>

#include "services/reader/book_session.h"

namespace niratan::reader {

struct SearchChapter {
    int spine_index;
    int character_start;
    // nullopt: no contents entry names this spine, so it inherits the label
    // before it. An empty string is a chapter named with no text.
    std::optional<QString> label;
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
// One label per spine from the contents: the deepest entry naming the spine,
// the first among entries at that depth; nullopt where no entry names it.
std::vector<std::optional<QString>> search_labels(const std::vector<ContentsEntry>& contents,
                                                  int spine_count);
// The label shown for a spine: its own label, else the nearest one before it,
// else empty. Out-of-range spines are empty. The highlights list uses it too.
QString search_label_at(const std::vector<std::optional<QString>>& labels, int spine_index);
// Per hit: the size of the group it starts, or 0 when it continues the group
// before it. A group is a run of consecutive hits with the same label.
std::vector<int> search_group_counts(const std::vector<SearchHit>& hits);
std::optional<std::vector<SearchHit>> search_epub(std::string_view path,
                                                const QString& query);

}  // namespace niratan::reader
