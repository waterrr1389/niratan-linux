// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_query.h"

#include "language_id.h"

#include <hoshidicts/language.hpp>
#include <hoshidicts/lookup.hpp>
#include <hoshidicts/query.hpp>

namespace niratan::dictionary {

struct DictionaryQueryService::State {
    ::DictionaryQuery query;
    std::unique_ptr<::Lookup> lookup;
};

DictionaryQueryService::DictionaryQueryService(
    std::string_view language_id, const std::vector<std::filesystem::path>& term_dictionaries)
    : state_(std::make_unique<State>()) {
    const auto normalized = normalize_language_id(language_id);
    for (const auto& directory : term_dictionaries) {
        state_->query.add_term_dict(directory.string());
    }
    state_->lookup = std::make_unique<::Lookup>(state_->query, ::language::get(normalized));
}

DictionaryQueryService::~DictionaryQueryService() = default;

LookupOutcome DictionaryQueryService::lookup(std::string_view text, int max_results,
                                              int scan_length) const {
    LookupOutcome outcome;
    try {
        const auto results =
            state_->lookup->lookup(std::string(text), max_results, scan_length);
        outcome.entries.reserve(results.size());
        for (const auto& result : results) {
            outcome.entries.push_back(
                LookupEntry{result.term.expression, result.term.reading, result.matched});
        }
        outcome.ok = true;
    } catch (const std::exception& exception) {
        outcome.error = exception.what();
    }
    return outcome;
}

}  // namespace niratan::dictionary
