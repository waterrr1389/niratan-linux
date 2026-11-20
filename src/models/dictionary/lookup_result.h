// SPDX-License-Identifier: GPL-3.0-or-later
//
// One lookup result as the popup needs it, following the fields of
// hoshidicts LookupResult (lookup.hpp, query.hpp, language.hpp) and the entry
// shape built by Features/Popup/PopupView.swift (buildLookupEntries).
//
// The values are the library's raw data: traces keep the candidate order and
// the innermost-first step order, rules stay a space-separated string, and
// frequency and pitch entries are not deduplicated. The popup builder in
// services/dictionary applies the reversal, splitting and deduplication, so
// this model stays a plain carrier and callers outside the dictionary service
// never see a hoshidicts type.

#pragma once

#include <string>
#include <vector>

namespace niratan::dictionary {

struct TraceStep {
    std::string name;
    std::string description;

    bool operator==(const TraceStep&) const = default;
};

struct GlossaryEntry {
    std::string dictionary;
    std::string content;
    std::string definition_tags;
    std::string term_tags;

    bool operator==(const GlossaryEntry&) const = default;
};

struct FrequencyValue {
    int value = 0;
    std::string display_value;

    bool operator==(const FrequencyValue&) const = default;
};

struct FrequencyGroup {
    std::string dictionary;
    std::vector<FrequencyValue> values;

    bool operator==(const FrequencyGroup&) const = default;
};

struct PitchGroup {
    std::string dictionary;
    std::vector<int> positions;
    std::vector<std::string> transcriptions;

    bool operator==(const PitchGroup&) const = default;
};

struct LookupEntry {
    std::string expression;
    std::string reading;
    std::string matched;
    // One vector per deinflection candidate, in library order. Each vector is
    // innermost-first; the popup reverses it.
    std::vector<std::vector<TraceStep>> traces;
    std::vector<GlossaryEntry> glossaries;
    std::vector<FrequencyGroup> frequencies;
    std::vector<PitchGroup> pitches;
    std::string rules;

    bool operator==(const LookupEntry&) const = default;
};

struct LookupOutcome {
    bool ok = false;
    std::string error;
    std::vector<LookupEntry> entries;
};

}  // namespace niratan::dictionary
