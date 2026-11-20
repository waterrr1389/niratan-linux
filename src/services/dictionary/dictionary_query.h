// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "models/dictionary/lookup_result.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::dictionary {

// Owns one DictionaryQuery and one Lookup for a language, matching
// LookupEngine.QueryBundle. The hoshidicts objects stay behind this type.
class DictionaryQueryService {
   public:
    DictionaryQueryService(std::string_view language_id,
                           const std::vector<std::filesystem::path>& term_dictionaries);
    ~DictionaryQueryService();

    DictionaryQueryService(const DictionaryQueryService&) = delete;
    DictionaryQueryService& operator=(const DictionaryQueryService&) = delete;

    // scan_length is measured in Unicode scalar values, as LookupEngine passes
    // it to Lookup::lookup.
    LookupOutcome lookup(std::string_view text, int max_results, int scan_length) const;

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::dictionary
