// SPDX-License-Identifier: GPL-3.0-or-later
//
// Core/AnkiManager.swift (applyFetchedAnkiMetadata, pruneFieldMappings,
// autofillFieldMappings): discard models with no fields and reject an empty
// deck list or no usable models. Keep exact selections where possible;
// otherwise pick the first non-Default deck (case-insensitive test), or the
// first deck, and the first usable model. Prune against the selected model
// even if it has no template defaults. Optionally autofill whitespace-only
// and absent mappings from the template; keep explicitly empty mappings.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "models/anki/note_type.h"
#include "services/anki/field_mapping.h"

namespace niratan::anki {

struct FetchedMetadata {
    std::vector<std::string> decks;
    std::vector<NoteType> note_types;
    std::string selected_deck;
    std::string selected_note_type;
    FieldMappings mappings;

    bool operator==(const FetchedMetadata&) const = default;
};

std::optional<FetchedMetadata>
apply_fetched_metadata(std::vector<std::string> decks, std::vector<NoteType> note_types,
                       const std::optional<std::string>& selected_deck,
                       const std::optional<std::string>& selected_note_type,
                       const FieldMappings& mappings);

}  // namespace niratan::anki
