// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "models/dictionary/lookup_result.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
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

    // The media file a term dictionary stores under path, or an empty string
    // when the dictionary or path is unknown. Bytes are returned unchanged.
    // Follows LookupEngine.withMediaFile (get_media_file_view): a zero length
    // or null view is empty.
    std::string media_file(std::string_view dictionary, std::string_view path) const;

    // Dictionary styles keyed by dictionary name, matching
    // LookupEngine.dictionaryStyles (get_styles).
    std::vector<std::pair<std::string, std::string>> dictionary_styles() const;

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::dictionary
