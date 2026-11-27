// SPDX-License-Identifier: GPL-3.0-or-later
//
// Models/Dictionary.swift: DictionaryCategory and the DictionaryInfo fields
// used when rendering card fields and listing glossary mappings.

#pragma once

#include <string>

namespace niratan::anki {

enum class DictionaryCategory { none, monolingual, bilingual, exclude };

// A term dictionary in the order the user ranked it.
struct TermDictionary {
    std::string title;
    DictionaryCategory category = DictionaryCategory::none;
};

}  // namespace niratan::anki
