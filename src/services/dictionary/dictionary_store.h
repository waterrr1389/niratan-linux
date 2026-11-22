// SPDX-License-Identifier: GPL-3.0-or-later
//
// The typed dictionary library, following Core/DictionaryManager.swift
// (getDictionariesFromStorage(type:), importDictionary(from:)) and
// Models/Dictionary.swift (DictionaryType, DictionaryIndex).
//
// Dictionaries live under <root>/<Type>/<title>, one directory per type the
// imported archive supplies; the directories are DictionaryType.rawValue.
// A type scan creates its directory when missing, skips hidden entries,
// deletes every non-directory entry and every directory whose index.json does
// not decode as DictionaryIndex, and returns the survivors in filesystem
// iteration order with the directory name as title. Nothing is sorted.
//
// installed() is the union scan the popup media handler uses: every surviving
// directory under Term, Frequency, Pitch and Kanji, in that order.
//
// import_zip imports into a temporary staging directory and then publishes one
// full copy per type whose importer count is above zero, replacing an existing
// directory of the same title. The archive's index.json title names the
// directory. A failed import writes nothing below the type directories.
// term_count and the other counts are the importer's counts; directory is the
// Term directory when the archive has terms, otherwise the first written type
// directory, otherwise empty. directories lists the written type directories
// in Term, Frequency, Pitch, Kanji order.
//
// A hidden entry is left alone rather than deleted; only visible entries are
// cleaned up.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "models/dictionary/dictionary_config.h"

namespace niratan::dictionary {

struct ImportOutcome {
    bool ok = false;
    std::string error;
    // The Term directory when the archive has terms, else the first written
    // type directory, else empty.
    std::filesystem::path directory;
    // Written type directories in Term, Frequency, Pitch, Kanji order.
    std::vector<std::filesystem::path> directories;
    int term_count = 0;
    int freq_count = 0;
    int pitch_count = 0;
    int kanji_count = 0;
};

// Installed dictionaries are directories, never zips. import_zip writes a
// temporary directory first and publishes it onto <root>/<Type>/<title>,
// because dictionary_importer::import deletes its output path before reading
// the archive and would otherwise destroy an already installed dictionary when
// titles collide.
class DictionaryStore {
   public:
    explicit DictionaryStore(std::filesystem::path root);

    ImportOutcome import_zip(const std::filesystem::path& zip) const;
    std::vector<DictionaryInfo> installed(DictionaryType type) const;
    // Every dictionary under any of the four types, in type order.
    std::vector<std::filesystem::path> installed() const;

   private:
    std::filesystem::path root_;
};

}  // namespace niratan::dictionary
