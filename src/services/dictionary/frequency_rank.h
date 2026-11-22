// SPDX-License-Identifier: GPL-3.0-or-later
//
// Re-ranking of lookup results by one frequency dictionary, matching
// LookupEngine.sortedByFrequencyDictionary. The library already ranked the
// results; this only breaks ties.
//
// A result ranks ahead of another when its matched text is longer, then when
// its best trace key precedes the other's lexicographically. The trace key is
// the minimum over the candidates of (preprocessor steps, trace length, 1
// when the candidate's deinflected form differs from the term's expression).
// A result with no candidates sorts as (max int, max int, 1), after every
// real trace.
//
// Frequency applies only when both length and trace are equal. Entries from
// other dictionaries and negative values are ignored. Ascending keeps the
// smallest remaining value, descending the largest. A result that has a value
// ranks ahead of one that has none, in both directions. Equal values, and the
// case where neither has a value, keep the input order.
//
// The order "auto" and an empty dictionary name both leave the input order
// untouched, as does a single result.
//
// frequency_rank_order returns the permutation of result indices that
// rank_by_frequency uses; callers apply it to the mapped entries after the
// library's truncation. With fewer than two results it returns the identity.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::dictionary {

enum class FrequencySortOrder { automatic, ascending, descending };

struct FrequencySample {
    int value = 0;

    bool operator==(const FrequencySample&) const = default;
};

struct FrequencyReading {
    std::string dictionary;
    std::vector<FrequencySample> samples;

    bool operator==(const FrequencyReading&) const = default;
};

struct TraceCandidate {
    int preprocessor_steps = 0;
    int trace_length = 0;
    std::string deinflected;

    bool operator==(const TraceCandidate&) const = default;
};

// The fields the ranking reads. The dictionary result carries more, but the
// ranking must not depend on it.
struct RankedLookup {
    std::string matched;
    std::string expression;
    std::vector<TraceCandidate> traces;
    std::vector<FrequencyReading> frequencies;

    bool operator==(const RankedLookup&) const = default;
};

std::vector<RankedLookup> rank_by_frequency(const std::vector<RankedLookup>& results,
                                            FrequencySortOrder order,
                                            std::string_view dictionary);

// The permutation rank_by_frequency applies, as indices into results.
std::vector<std::size_t> frequency_rank_order(const std::vector<RankedLookup>& results,
                                              FrequencySortOrder order,
                                              std::string_view dictionary);

}  // namespace niratan::dictionary
