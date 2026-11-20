// SPDX-License-Identifier: GPL-3.0-or-later
//
// Anki field mapping defaults, matching Models/Anki.swift (AnkiFieldTemplate,
// defaultMapping, autofilledMappings, appliedDefaultMappings, clearsMapping).
//
// Field names and mappings are UTF-8. Only Lapis, Kiku and Senren have
// defaults. An unknown note type keeps its
// existing mappings untouched.
//
// A default is found by the field name. "sentenceaudio" and "picture",
// compared case-insensitively, map to {sasayaki-audio} and {book-cover} for
// every known note type. Every other field matches the note type's table
// exactly, so case matters there.
//
// Autofill keeps an existing mapping, including an empty one, which means the
// user turned the field off. A mapping that is only whitespace counts as
// missing and takes the default. Fields the note type does not have are
// dropped, whether or not the note type is known.
//
// Applying defaults overwrites every field that has one and then drops the
// fields clears_mapping names. Lapis clears DefinitionPicture, because an
// earlier default pointed it at the glossary.

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::anki {

bool has_field_defaults(std::string_view note_type);

// True when applying defaults removes the field instead of filling it.
bool clears_mapping(std::string_view note_type, std::string_view field);

// Keys are field names, values are handlebars such as "{expression}".
using FieldMappings = std::map<std::string, std::string, std::less<>>;

FieldMappings autofill_mappings(std::string_view note_type,
                                const std::vector<std::string>& available_fields,
                                const FieldMappings& existing);

FieldMappings apply_default_mappings(std::string_view note_type,
                                     const std::vector<std::string>& available_fields,
                                     const FieldMappings& existing);

}  // namespace niratan::anki
