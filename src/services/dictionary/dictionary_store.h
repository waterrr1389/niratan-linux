// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace niratan::dictionary {

struct ImportOutcome {
    bool ok = false;
    std::string error;
    // Directory named after the dictionary title, which queries consume.
    std::filesystem::path directory;
    int term_count = 0;
};

// Installed dictionaries are directories, never zips. import() writes a
// temporary directory first and renames it onto <root>/<title>, because
// dictionary_importer::import deletes its output path before reading the
// archive and would otherwise destroy an already installed dictionary when
// titles collide.
class DictionaryStore {
   public:
    explicit DictionaryStore(std::filesystem::path root);

    ImportOutcome import_zip(const std::filesystem::path& zip) const;
    std::vector<std::filesystem::path> installed() const;

   private:
    std::filesystem::path root_;
};

}  // namespace niratan::dictionary
