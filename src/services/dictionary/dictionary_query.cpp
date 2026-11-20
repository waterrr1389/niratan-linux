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
            LookupEntry entry;
            entry.expression = result.term.expression;
            entry.reading = result.term.reading;
            entry.matched = result.matched;
            entry.rules = result.term.rules;

            // Candidate and step order are kept as the library returns them;
            // the popup builder reverses each inner vector.
            entry.traces.reserve(result.trace_candidates.size());
            for (const auto& candidate : result.trace_candidates) {
                std::vector<TraceStep> steps;
                steps.reserve(candidate.trace.size());
                for (const auto& step : candidate.trace) {
                    TraceStep mapped;
                    mapped.name = step.name;
                    mapped.description = step.description;
                    steps.push_back(std::move(mapped));
                }
                entry.traces.push_back(std::move(steps));
            }

            entry.glossaries.reserve(result.term.glossaries.size());
            for (const auto& glossary : result.term.glossaries) {
                GlossaryEntry mapped;
                mapped.dictionary = glossary.dict_name;
                mapped.content = glossary.glossary;
                mapped.definition_tags = glossary.definition_tags;
                mapped.term_tags = glossary.term_tags;
                entry.glossaries.push_back(std::move(mapped));
            }

            entry.frequencies.reserve(result.term.frequencies.size());
            for (const auto& frequency : result.term.frequencies) {
                FrequencyGroup mapped;
                mapped.dictionary = frequency.dict_name;
                mapped.values.reserve(frequency.frequencies.size());
                for (const auto& value : frequency.frequencies) {
                    FrequencyValue mapped_value;
                    mapped_value.value = value.value;
                    mapped_value.display_value = value.display_value;
                    mapped.values.push_back(std::move(mapped_value));
                }
                entry.frequencies.push_back(std::move(mapped));
            }

            entry.pitches.reserve(result.term.pitches.size());
            for (const auto& pitch : result.term.pitches) {
                PitchGroup mapped;
                mapped.dictionary = pitch.dict_name;
                mapped.positions = pitch.pitch_positions;
                mapped.transcriptions = pitch.transcriptions;
                entry.pitches.push_back(std::move(mapped));
            }

            outcome.entries.push_back(std::move(entry));
        }
        outcome.ok = true;
    } catch (const std::exception& exception) {
        outcome.error = exception.what();
    }
    return outcome;
}

std::string DictionaryQueryService::media_file(std::string_view dictionary,
                                               std::string_view path) const {
    const ::MediaFileView view = state_->query.get_media_file_view(std::string(dictionary),
                                                                   std::string(path));
    if (view.size == 0 || view.data == nullptr) {
        return {};
    }
    return std::string(view.data, view.size);
}

std::vector<std::pair<std::string, std::string>> DictionaryQueryService::dictionary_styles() const {
    std::vector<std::pair<std::string, std::string>> result;
    for (const ::DictionaryStyle& style : state_->query.get_styles()) {
        result.emplace_back(style.dict_name, style.styles);
    }
    return result;
}

}  // namespace niratan::dictionary
