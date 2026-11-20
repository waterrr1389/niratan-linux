// SPDX-License-Identifier: GPL-3.0-or-later
//
// AnkiConnect duplicate options, matching Models/Anki.swift
// (DuplicateScope.ankiConnectOptions).
//
// AnkiConnect has no deck-root scope, so deck root is sent as a deck scope
// whose deck is the first non-empty piece between "::" separators and whose
// children are included. A deck name without "::" is its own root, and so is
// one made only of separators.
//
// check_all_models adds its flag to the scope options without changing the
// scope. The scope options object is omitted entirely when it would be empty,
// which is the collection and deck scopes unless check_all_models is set.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace niratan::anki {

enum class DuplicateScope { collection, deck, deck_root };

struct DuplicateScopeOptions {
    std::optional<std::string> deck_name;
    bool check_children = false;
    bool check_all_models = false;

    bool operator==(const DuplicateScopeOptions&) const = default;
};

struct DuplicateOptions {
    bool allow_duplicate = false;
    // "collection" or "deck". Deck root is reported as "deck".
    std::string scope;
    std::optional<DuplicateScopeOptions> scope_options;

    bool operator==(const DuplicateOptions&) const = default;
};

DuplicateOptions duplicate_options(DuplicateScope scope, std::string_view deck, bool allow_duplicates,
                                   bool check_all_models);

}  // namespace niratan::anki
