// SPDX-License-Identifier: GPL-3.0-or-later
//
// An Anki note type and its field names in order, matching AnkiNoteType in
// Models/Anki.swift.

#pragma once

#include <string>
#include <vector>

namespace niratan::anki {

struct NoteType {
    std::string name;
    std::vector<std::string> fields;

    bool operator==(const NoteType&) const = default;
};

}  // namespace niratan::anki
