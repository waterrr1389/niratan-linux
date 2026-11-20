// SPDX-License-Identifier: GPL-3.0-or-later
//
// The lookup entries the dictionary popup script reads, following
// Features/Popup/PopupView.swift (buildLookupEntries) and the fields consumed
// by Features/Popup/popup.js.
//
// One JSON object per entry, in order:
//   expression, reading, matched: the entry strings unchanged.
//   deinflectionTraces: one array per candidate, each candidate's steps
//     reversed to outermost-first and rendered as {name, description}.
//   deinflectionTrace: the first candidate's reversed array, or [] when there
//     is no candidate.
//   glossaries: {dictionary, content, definitionTags, termTags}; content is
//     the raw glossary JSON text, not decoded.
//   frequencies: {dictionary, frequencies: [{value, displayValue}]}, values in
//     library order.
//   pitches: {dictionary, pitchPositions, transcriptions}, with duplicate
//     positions and transcriptions removed and the first occurrence kept.
//   rules: the space-separated rules split on single spaces with empty pieces
//     dropped.
// Strings are UTF-8. QJsonObject orders member names itself; the popup reads
// by name.

#pragma once

#include <QJsonArray>

#include <vector>

#include "models/dictionary/lookup_result.h"

namespace niratan::dictionary {

QJsonArray lookup_entries_json(const std::vector<LookupEntry>& entries);

}  // namespace niratan::dictionary
