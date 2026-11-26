// SPDX-License-Identifier: GPL-3.0-or-later
//
// Transcript and history search, following VideoStudyListCard.swift
// `VideoStudySearch` (`options`, `normalizedQuery`, `matches`, `highlighted`).
//
// `normalized_study_query` trims whitespace and newlines, as
// `normalizedQuery` does; the raw field text keeps whatever the user typed.
// A query is active when that trimmed text is not empty.
//
// Matching and highlighting share one normalization: NFKD, case folding and
// removing combining marks. It stands in for Swift's `.caseInsensitive`,
// `.diacriticInsensitive` and `.widthInsensitive` options, so both sides of a
// comparison use the same form and widths fold through the decomposition.
// `study_search_matches` is false for an empty trimmed query or when the
// normalized query is empty, and otherwise asks whether the text contains it.
//
// `study_search_ranges` returns every non-overlapping occurrence from left to
// right, advancing past each match like `highlighted` does, with positions in
// the original text: a match inside a decomposed character expands to cover
// that whole character, so callers can highlight the text without changing
// its characters or order. An inactive or unmatchable query yields no ranges.

#pragma once

#include <QList>
#include <QString>
#include <QStringView>

namespace niratan::player {

struct StudySearchRange {
    qsizetype start = 0;
    qsizetype length = 0;
    bool operator==(const StudySearchRange&) const = default;
};

QString normalized_study_query(QStringView query);

bool study_search_matches(QStringView text, QStringView query);

QList<StudySearchRange> study_search_ranges(QStringView text, QStringView query);

}  // namespace niratan::player
