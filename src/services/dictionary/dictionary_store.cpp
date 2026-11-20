// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_store.h"

#include <hoshidicts/importer.hpp>

#include <random>
#include <system_error>

namespace niratan::dictionary {
namespace {

std::filesystem::path temporary_sibling(const std::filesystem::path& root) {
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<unsigned> distribution(0, 0xffffff);
    return root / (".import-" + std::to_string(distribution(generator)));
}

}  // namespace

DictionaryStore::DictionaryStore(std::filesystem::path root) : root_(std::move(root)) {}

ImportOutcome DictionaryStore::import_zip(const std::filesystem::path& zip) const {
    ImportOutcome outcome;
    std::error_code error;
    std::filesystem::create_directories(root_, error);
    if (error) {
        outcome.error = "cannot create dictionary root: " + error.message();
        return outcome;
    }

    // The importer derives the final directory name from index.title, which
    // is only known after a successful import, so the staging directory
    // cannot be predicted up front.
    const auto staging = temporary_sibling(root_);
    std::filesystem::remove_all(staging, error);

    ::ImportResult imported;
    try {
        imported = dictionary_importer::import(zip.string(), staging.string(), false);
    } catch (const std::exception& exception) {
        std::filesystem::remove_all(staging, error);
        outcome.error = exception.what();
        return outcome;
    }
    if (!imported.success) {
        std::filesystem::remove_all(staging, error);
        outcome.error = imported.errors.empty() ? "import failed" : imported.errors.front();
        return outcome;
    }

    const auto produced = staging / imported.title;
    const auto destination = root_ / imported.title;
    std::filesystem::remove_all(destination, error);
    std::filesystem::rename(produced, destination, error);
    std::filesystem::remove_all(staging, error);
    if (error) {
        outcome.error = "cannot publish imported dictionary: " + error.message();
        return outcome;
    }

    outcome.ok = true;
    outcome.directory = destination;
    outcome.term_count = imported.term_count;
    return outcome;
}

std::vector<std::filesystem::path> DictionaryStore::installed() const {
    std::vector<std::filesystem::path> directories;
    std::error_code error;
    if (!std::filesystem::is_directory(root_, error)) return directories;
    for (const auto& entry : std::filesystem::directory_iterator(root_, error)) {
        if (entry.is_directory() && !entry.path().filename().string().starts_with('.')) {
            directories.push_back(entry.path());
        }
    }
    return directories;
}

}  // namespace niratan::dictionary
