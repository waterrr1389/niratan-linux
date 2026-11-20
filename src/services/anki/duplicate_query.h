// SPDX-License-Identifier: GPL-3.0-or-later
//
// Anki search query that finds the notes a duplicate check collided with,
// matching Core/AnkiManager.swift (duplicateSearchQuery, quotedAnkiSearchTerm).
//
// Terms are joined by single spaces in this order: deck, note type, field. A
// query with no term is empty.
//   - Deck term: none for the collection scope, "deck:<deck>" for the deck
//     scope, and "deck:<root>" for the deck root scope, where the root is
//     deck_root(deck).
//   - Note type term: "note:<note type name>", none when check_all_models is
//     set.
//   - Field term: "<field>:<word>" with the field name lowercased. Without
//     check_all_models the field is first_field. With it, the fields are the
//     first field of every note type that has one, in order, and first_field
//     is not used. A field already seen, compared lowercased, is dropped. One
//     field gives a bare term, several give "(a or b)", none gives no term.
//
// Every term is quoted whole after the '"' characters inside it are removed.
// Nothing else is escaped: the word, deck and field names pass through as
// given, without trimming. Lowercasing follows Unicode, not ASCII.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "models/anki/note_type.h"
#include "services/anki/duplicate_scope.h"

namespace niratan::anki {

std::string duplicate_search_query(std::string_view word, std::string_view deck,
                                   std::string_view note_type_name, std::string_view first_field,
                                   DuplicateScope scope, bool check_all_models,
                                   const std::vector<NoteType>& note_types);

}  // namespace niratan::anki
