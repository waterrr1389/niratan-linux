// SPDX-License-Identifier: GPL-3.0-or-later

#include "field_mapping.h"

#include <optional>

namespace niratan::anki {
namespace {

// CharacterSet.whitespacesAndNewlines: the Unicode space separators plus the
// line and paragraph breaks. Japanese input commonly produces U+3000.
bool is_whitespace(char32_t c) {
    return (c >= 0x09 && c <= 0x0D) || c == 0x20 || c == 0x85 || c == 0xA0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F ||
           c == 0x205F || c == 0x3000;
}

// Mappings come from settings this app wrote, so the text is valid UTF-8. A
// malformed byte counts as content, which keeps the user's mapping.
bool is_blank(std::string_view value) {
    std::size_t i = 0;
    while (i < value.size()) {
        const auto lead = static_cast<unsigned char>(value[i]);
        const std::size_t length = lead < 0x80 ? 1 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 0;
        if (length == 0 || i + length > value.size()) {
            return false;
        }
        char32_t c = length == 1 ? lead : lead & (0x7F >> length);
        for (std::size_t k = 1; k < length; ++k) {
            c = (c << 6) | (static_cast<unsigned char>(value[i + k]) & 0x3F);
        }
        if (!is_whitespace(c)) {
            return false;
        }
        i += length;
    }
    return true;
}

std::string ascii_lower(std::string_view value) {
    std::string lowered;
    lowered.reserve(value.size());
    for (const char c : value) {
        lowered.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return lowered;
}

const FieldMappings lapis_mappings = {
    {"Expression", "{expression}"},
    {"ExpressionFurigana", "{furigana-plain}"},
    {"ExpressionReading", "{reading}"},
    {"ExpressionAudio", "{audio}"},
    {"SelectionText", "{popup-selection-text}"},
    {"MainDefinition", "{glossary-first}"},
    {"Sentence", "{sentence}"},
    {"SentenceAudio", "{sasayaki-audio}"},
    {"Picture", "{book-cover}"},
    {"Glossary", "{glossary}"},
    {"PitchPosition", "{pitch-accent-positions}"},
    {"PitchCategories", "{pitch-accent-categories}"},
    {"Frequency", "{frequencies}"},
    {"FreqSort", "{frequency-harmonic-rank}"},
    {"MiscInfo", "{document-title}"},
};

const FieldMappings kiku_mappings = {
    {"Expression", "{expression}"},
    {"ExpressionFurigana", "{furigana-plain}"},
    {"ExpressionReading", "{reading}"},
    {"ExpressionAudio", "{audio}"},
    {"SelectionText", "{popup-selection-text}"},
    {"MainDefinition", "{glossary-first}"},
    {"Sentence", "{sentence}"},
    {"Picture", "{book-cover}"},
    {"Glossary", "{glossary}"},
    {"PitchPosition", "{pitch-accent-positions}"},
    {"PitchCategories", "{pitch-accent-categories}"},
    {"Frequency", "{frequencies}"},
    {"FreqSort", "{frequency-harmonic-rank}"},
    {"MiscInfo", "{document-title}"},
};

const FieldMappings senren_mappings = {
    {"word", "{expression}"},
    {"reading", "{reading}"},
    {"sentence", "{sentence}"},
    {"selectionText", "{popup-selection-text}"},
    {"definition", "{glossary-first}"},
    {"wordAudio", "{audio}"},
    {"picture", "{book-cover}"},
    {"glossary", "{glossary}"},
    {"pitchPositions", "{pitch-accent-positions}"},
    {"pitchCategories", "{pitch-accent-categories}"},
    {"frequencies", "{frequencies}"},
    {"freqSort", "{frequency-harmonic-rank}"},
    {"miscInfo", "{document-title}"},
};

const FieldMappings* template_for(std::string_view note_type) {
    if (note_type == "Lapis") {
        return &lapis_mappings;
    }
    if (note_type == "Kiku") {
        return &kiku_mappings;
    }
    if (note_type == "Senren") {
        return &senren_mappings;
    }
    return nullptr;
}

std::optional<std::string_view> default_mapping(std::string_view field,
                                                const FieldMappings& note_type_mappings) {
    const std::string lowered = ascii_lower(field);
    if (lowered == "sentenceaudio") {
        return "{sasayaki-audio}";
    }
    if (lowered == "picture") {
        return "{book-cover}";
    }
    const auto found = note_type_mappings.find(field);
    if (found == note_type_mappings.end()) {
        return std::nullopt;
    }
    return found->second;
}

FieldMappings available_only(const FieldMappings& existing,
                             const std::vector<std::string>& available_fields) {
    FieldMappings result;
    for (const auto& field : available_fields) {
        const auto found = existing.find(field);
        if (found != existing.end()) {
            result.emplace(found->first, found->second);
        }
    }
    return result;
}

}  // namespace

bool has_field_defaults(std::string_view note_type) { return template_for(note_type) != nullptr; }

bool clears_mapping(std::string_view note_type, std::string_view field) {
    return note_type == "Lapis" && field == "DefinitionPicture";
}

FieldMappings autofill_mappings(std::string_view note_type,
                                const std::vector<std::string>& available_fields,
                                const FieldMappings& existing) {
    const FieldMappings* note_type_mappings = template_for(note_type);
    if (note_type_mappings == nullptr) {
        return existing;
    }

    FieldMappings result = available_only(existing, available_fields);
    for (const auto& field : available_fields) {
        const auto fallback = default_mapping(field, *note_type_mappings);
        if (!fallback.has_value()) {
            continue;
        }
        const auto current = result.find(field);
        // An empty value means the user turned the field off; only a value
        // made of whitespace counts as missing.
        if (current != result.end() && (current->second.empty() || !is_blank(current->second))) {
            continue;
        }
        result[field] = *fallback;
    }
    return result;
}

FieldMappings apply_default_mappings(std::string_view note_type,
                                     const std::vector<std::string>& available_fields,
                                     const FieldMappings& existing) {
    const FieldMappings* note_type_mappings = template_for(note_type);
    if (note_type_mappings == nullptr) {
        return existing;
    }

    FieldMappings result = available_only(existing, available_fields);
    for (const auto& field : available_fields) {
        const auto fallback = default_mapping(field, *note_type_mappings);
        if (!fallback.has_value()) {
            continue;
        }
        result[field] = *fallback;
    }
    for (const auto& field : available_fields) {
        if (clears_mapping(note_type, field)) {
            result.erase(field);
        }
    }
    return result;
}

}  // namespace niratan::anki
