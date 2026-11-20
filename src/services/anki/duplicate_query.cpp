// SPDX-License-Identifier: GPL-3.0-or-later

#include "duplicate_query.h"

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <QString>

namespace niratan::anki {

namespace {

// Unicode lowercasing, as String.lowercased() does for field names. The name
// is the only part that is case folded; everything else keeps its bytes.
std::string lowercased(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const QString decoded = QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
    return decoded.toLower().toUtf8().toStdString();
}

// Anki search term: every '"' is dropped, then the whole term is quoted. No
// other character is escaped and nothing is trimmed. Byte comparison is safe
// because '"' never occurs inside a multibyte UTF-8 sequence.
std::string quoted_term(std::string_view term) {
    std::string result;
    result.reserve(term.size() + 2);
    result.push_back('"');
    for (const char c : term) {
        if (c != '"') {
            result.push_back(c);
        }
    }
    result.push_back('"');
    return result;
}

}  // namespace

std::string duplicate_search_query(std::string_view word, std::string_view deck,
                                   std::string_view note_type_name, std::string_view first_field,
                                   DuplicateScope scope, bool check_all_models,
                                   const std::vector<NoteType>& note_types) {
    std::vector<std::string> terms;

    switch (scope) {
        case DuplicateScope::collection:
            break;
        case DuplicateScope::deck:
            terms.push_back(quoted_term("deck:" + std::string(deck)));
            break;
        case DuplicateScope::deck_root:
            terms.push_back(quoted_term("deck:" + deck_root(deck)));
            break;
    }

    if (!check_all_models) {
        terms.push_back(quoted_term("note:" + std::string(note_type_name)));
    }

    // Fields keep the order they are offered in; a name already seen, compared
    // lowercased, is dropped and the first occurrence supplies the casing.
    std::vector<std::string> field_terms;
    std::unordered_set<std::string> seen_fields;
    const auto add_field = [&](std::string_view field) {
        const std::string name = lowercased(field);
        if (!seen_fields.insert(name).second) {
            return;
        }
        field_terms.push_back(quoted_term(name + ":" + std::string(word)));
    };

    if (check_all_models) {
        for (const NoteType& note_type : note_types) {
            if (!note_type.fields.empty()) {
                add_field(note_type.fields.front());
            }
        }
    } else {
        add_field(first_field);
    }

    if (field_terms.size() == 1) {
        terms.push_back(std::move(field_terms.front()));
    } else if (!field_terms.empty()) {
        std::string combined = "(";
        for (std::size_t i = 0; i < field_terms.size(); ++i) {
            if (i != 0) {
                combined += " or ";
            }
            combined += field_terms[i];
        }
        combined += ")";
        terms.push_back(std::move(combined));
    }

    std::string query;
    for (std::size_t i = 0; i < terms.size(); ++i) {
        if (i != 0) {
            query.push_back(' ');
        }
        query += terms[i];
    }
    return query;
}

}  // namespace niratan::anki
