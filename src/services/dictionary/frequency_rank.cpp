// SPDX-License-Identifier: GPL-3.0-or-later

#include "frequency_rank.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <tuple>
#include <vector>

namespace niratan::dictionary {
namespace {

using TraceKey = std::tuple<int, int, int>;

// Counts Unicode scalar values. The lookup text carries no combining marks, so
// counting code points matches the grapheme count used for the ranking.
std::size_t scalar_count(std::string_view text) {
    std::size_t count = 0;
    for (unsigned char byte : text) {
        if ((byte & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

TraceKey trace_key(const RankedLookup& lookup) {
    constexpr int max = std::numeric_limits<int>::max();
    bool found = false;
    TraceKey best{};
    for (const TraceCandidate& candidate : lookup.traces) {
        const TraceKey key{candidate.preprocessor_steps, candidate.trace_length,
                           candidate.deinflected != lookup.expression ? 1 : 0};
        if (!found || key < best) {
            best = key;
            found = true;
        }
    }
    return found ? best : TraceKey{max, max, 1};
}

std::optional<int> frequency_value(const RankedLookup& lookup, std::string_view dictionary,
                                   bool descending) {
    std::optional<int> value;
    for (const FrequencyReading& reading : lookup.frequencies) {
        if (reading.dictionary != dictionary) {
            continue;
        }
        for (const FrequencySample& sample : reading.samples) {
            if (sample.value < 0) {
                continue;
            }
            if (!value || (descending ? sample.value > *value : sample.value < *value)) {
                value = sample.value;
            }
        }
    }
    return value;
}

}  // namespace

std::vector<std::size_t> frequency_rank_order(const std::vector<RankedLookup>& results,
                                              FrequencySortOrder order,
                                              std::string_view dictionary) {
    std::vector<std::size_t> indices(results.size());
    for (std::size_t index = 0; index < indices.size(); ++index) {
        indices[index] = index;
    }
    if (order == FrequencySortOrder::automatic || dictionary.empty() || results.size() < 2) {
        return indices;
    }

    const bool descending = order == FrequencySortOrder::descending;

    struct Ranked {
        std::size_t length = 0;
        TraceKey trace;
        std::optional<int> frequency;
    };

    std::vector<Ranked> ranked;
    ranked.reserve(results.size());
    for (const RankedLookup& lookup : results) {
        ranked.push_back(Ranked{scalar_count(lookup.matched), trace_key(lookup),
                                frequency_value(lookup, dictionary, descending)});
    }

    std::stable_sort(indices.begin(), indices.end(), [&](std::size_t left, std::size_t right) {
        const Ranked& a = ranked[left];
        const Ranked& b = ranked[right];
        if (a.length != b.length) {
            return a.length > b.length;
        }
        if (a.trace != b.trace) {
            return a.trace < b.trace;
        }
        if (a.frequency && b.frequency) {
            if (*a.frequency != *b.frequency) {
                return descending ? *a.frequency > *b.frequency : *a.frequency < *b.frequency;
            }
            return false;
        }
        if (a.frequency) {
            return true;
        }
        return false;
    });
    return indices;
}

std::vector<RankedLookup> rank_by_frequency(const std::vector<RankedLookup>& results,
                                            FrequencySortOrder order, std::string_view dictionary) {
    const std::vector<std::size_t> order_indices = frequency_rank_order(results, order, dictionary);
    std::vector<RankedLookup> ranked;
    ranked.reserve(results.size());
    for (const std::size_t index : order_indices) {
        ranked.push_back(results[index]);
    }
    return ranked;
}

}  // namespace niratan::dictionary
