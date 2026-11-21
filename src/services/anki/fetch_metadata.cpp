// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/fetch_metadata.h"

#include <QString>

#include <algorithm>
#include <utility>

namespace niratan::anki {

std::optional<FetchedMetadata>
apply_fetched_metadata(std::vector<std::string> decks, std::vector<NoteType> note_types,
                       const std::optional<std::string>& selected_deck,
                       const std::optional<std::string>& selected_note_type,
                       const FieldMappings& mappings) {
    std::erase_if(note_types, [](const NoteType& note_type) { return note_type.fields.empty(); });
    if (decks.empty() || note_types.empty()) {
        return std::nullopt;
    }

    auto deck = selected_deck ? std::ranges::find(decks, *selected_deck) : decks.end();
    if (deck == decks.end()) {
        deck = std::ranges::find_if(decks, [](const std::string& name) {
            return QString::fromStdString(name).compare(QStringLiteral("Default"),
                                                        Qt::CaseInsensitive) != 0;
        });
    }
    if (deck == decks.end()) {
        deck = decks.begin();
    }

    auto note_type = selected_note_type
        ? std::ranges::find(note_types, *selected_note_type, &NoteType::name)
        : note_types.end();
    if (note_type == note_types.end()) {
        note_type = note_types.begin();
    }

    // Fetch prunes even when the selected note type does not have a template.
    FieldMappings pruned;
    for (const auto& field : note_type->fields) {
        if (const auto existing = mappings.find(field); existing != mappings.end()) {
            pruned.insert(*existing);
        }
    }
    auto repaired = autofill_mappings(note_type->name, note_type->fields, pruned);
    auto deck_name = *deck;
    auto model_name = note_type->name;
    return FetchedMetadata{std::move(decks), std::move(note_types), std::move(deck_name),
                           std::move(model_name), std::move(repaired)};
}

}  // namespace niratan::anki
