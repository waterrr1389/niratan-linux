// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_query.h"

#include "language_id.h"

#include <hoshidicts/language.hpp>
#include <hoshidicts/lookup.hpp>
#include <hoshidicts/query.hpp>

namespace niratan::dictionary {
namespace {

LookupConfiguration
term_only_configuration(std::string_view language_id,
                        const std::vector<std::filesystem::path>& term_dictionaries) {
    LookupConfiguration configuration;
    configuration.term_dictionaries = term_dictionaries;
    configuration.language_id = std::string(language_id);
    return configuration;
}

}  // namespace

struct DictionaryQueryService::State {
    explicit State(LookupConfiguration configuration) : configuration(std::move(configuration)) {
        for (const auto& directory : this->configuration.term_dictionaries) {
            query.add_term_dict(directory.string());
        }
        for (const auto& directory : this->configuration.frequency_dictionaries) {
            query.add_freq_dict(directory.string());
        }
        for (const auto& directory : this->configuration.pitch_dictionaries) {
            query.add_pitch_dict(directory.string());
        }
        for (const auto& directory : this->configuration.kanji_dictionaries) {
            query.add_kanji_dict(directory.string());
        }
        lookup = std::make_unique<::Lookup>(
            query, ::language::get(normalize_language_id(this->configuration.language_id)));
    }

    LookupConfiguration configuration;
    ::DictionaryQuery query;
    std::unique_ptr<::Lookup> lookup;
};

DictionaryQueryService::DictionaryQueryService(const LookupConfiguration& configuration)
    : state_(std::make_unique<State>(configuration)) {}

DictionaryQueryService::DictionaryQueryService(
    std::string_view language_id, const std::vector<std::filesystem::path>& term_dictionaries)
    : state_(std::make_unique<State>(term_only_configuration(language_id, term_dictionaries))) {}

DictionaryQueryService::~DictionaryQueryService() = default;

LookupOutcome DictionaryQueryService::lookup(std::string_view text, int max_results,
                                             int scan_length) const {
    LookupOutcome outcome;
    try {
        const auto results = state_->lookup->lookup(std::string(text), max_results, scan_length);
        outcome.entries.reserve(results.size());
        std::vector<RankedLookup> rankable;
        rankable.reserve(results.size());
        for (const auto& result : results) {
            RankedLookup rankable_entry;
            rankable_entry.matched = result.matched;
            rankable_entry.expression = result.term.expression;
            rankable_entry.traces.reserve(result.trace_candidates.size());
            for (const auto& candidate : result.trace_candidates) {
                rankable_entry.traces.push_back(TraceCandidate{
                    candidate.preprocessor_steps, static_cast<int>(candidate.trace.size()),
                    candidate.deinflected});
            }
            rankable_entry.frequencies.reserve(result.term.frequencies.size());
            for (const auto& frequency : result.term.frequencies) {
                FrequencyReading reading;
                reading.dictionary = frequency.dict_name;
                reading.samples.reserve(frequency.frequencies.size());
                for (const auto& value : frequency.frequencies) {
                    reading.samples.push_back(FrequencySample{value.value});
                }
                rankable_entry.frequencies.push_back(std::move(reading));
            }

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
            rankable.push_back(std::move(rankable_entry));
        }

        const std::vector<std::size_t> rank_order =
            frequency_rank_order(rankable, state_->configuration.frequency_sort_order,
                                 state_->configuration.frequency_sort_dictionary);
        std::vector<LookupEntry> ordered;
        ordered.reserve(outcome.entries.size());
        for (const std::size_t index : rank_order) {
            ordered.push_back(std::move(outcome.entries[index]));
        }
        outcome.entries = std::move(ordered);
        outcome.ok = true;
    } catch (const std::exception& exception) {
        outcome.error = exception.what();
    }
    return outcome;
}

std::string DictionaryQueryService::media_file(std::string_view dictionary,
                                               std::string_view path) const {
    const ::MediaFileView view =
        state_->query.get_media_file_view(std::string(dictionary), std::string(path));
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
