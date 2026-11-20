// SPDX-License-Identifier: GPL-3.0-or-later

#include "duplicate_scope.h"

namespace niratan::anki {
namespace {

// deck.split(separator: "::", maxSplits: 1).first: empty pieces are dropped
// and do not count as a split, so leading separators are skipped. A name made
// only of separators has no piece and stays whole. "::" is ASCII, so byte
// search cannot match inside a multibyte character.
std::string deck_root(std::string_view deck) {
    std::string_view rest = deck;
    while (rest.starts_with("::")) {
        rest.remove_prefix(2);
    }
    if (rest.empty()) {
        return std::string(deck);
    }
    return std::string(rest.substr(0, rest.find("::")));
}

}  // namespace

DuplicateOptions duplicate_options(DuplicateScope scope, std::string_view deck, bool allow_duplicates,
                                   bool check_all_models) {
    DuplicateOptions options;
    options.allow_duplicate = allow_duplicates;

    DuplicateScopeOptions scope_options;
    switch (scope) {
        case DuplicateScope::collection:
            options.scope = "collection";
            break;
        case DuplicateScope::deck:
            options.scope = "deck";
            break;
        case DuplicateScope::deck_root:
            options.scope = "deck";
            scope_options.deck_name = deck_root(deck);
            scope_options.check_children = true;
            break;
    }

    scope_options.check_all_models = check_all_models;

    const bool has_scope_options = scope_options.deck_name.has_value() || scope_options.check_children ||
                                   scope_options.check_all_models;
    if (has_scope_options) {
        options.scope_options = scope_options;
    }
    return options;
}

}  // namespace niratan::anki
