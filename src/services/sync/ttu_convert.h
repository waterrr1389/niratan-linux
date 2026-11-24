// SPDX-License-Identifier: GPL-3.0-or-later
// TtuConverter.swift: convertToTtu and convertFromTtu; BookProcessor.process.
// Synchronous conversions return nullopt on failure. Export reads metadata,
// bookinfo and EPUB under the storage's Books; import writes the generated
// EPUB and its sidecars under that same Books directory. Archive entry names,
// staticdata values and EPUB templates are the contract, not JSON formatting.

#pragma once

#include "services/books/book_storage.h"

#include <cstdint>
#include <functional>

namespace niratan::sync {

struct TtuConvertOptions {
    std::function<std::int64_t()> now_unix_ms;
    std::function<std::string()> new_uuid;
};

class TtuConverter {
   public:
    explicit TtuConverter(books::BookStorage& storage, TtuConvertOptions options = {});
    std::optional<std::filesystem::path> export_zip(const std::filesystem::path& book_folder,
                                                    const std::filesystem::path& output_dir) const;
    std::optional<std::filesystem::path> import_zip(const std::filesystem::path& zip,
                                                    const std::filesystem::path& books_dir) const;

   private:
    books::BookStorage& storage_;
    TtuConvertOptions options_;
};

}  // namespace niratan::sync
