// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings_store.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/profiles/dictionary_settings.h"
#include "models/profiles/profile_ops.h"
#include "models/profiles/reader_settings.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

namespace niratan::settings {

using profiles::DictionaryProfileSettings;
using profiles::ReaderProfileSettings;

static constexpr std::string_view kProfilesDir = "Profiles";
static constexpr std::string_view kReaderFile = "reader_settings.json";
static constexpr std::string_view kDictionaryFile = "dictionary_settings.json";

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

std::optional<SettingsStore> SettingsStore::create() {
    const auto root = storage::data_root();
    if (root.empty()) {
        return std::nullopt;
    }
    return SettingsStore(root);
}

SettingsStore::SettingsStore(std::filesystem::path data_root)
    : data_root_(std::move(data_root)),
      profiles_directory_(data_root_ / kProfilesDir) {
    profiles::ProfileRepository repository(data_root_);
    active_profile_id_ = repository.active_profile().id;
}

std::string_view SettingsStore::active_profile_id() const {
    return active_profile_id_;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::optional<std::filesystem::path> SettingsStore::profile_directory(
    std::string_view profile_id) const {
    if (!profiles::is_safe_profile_id(profile_id)) {
        return std::nullopt;
    }
    return profiles_directory_ / std::string(profile_id);
}

static std::optional<ReaderProfileSettings> load_reader_from(
    const std::filesystem::path& dir) {
    const auto read = storage::read_json_file(dir / kReaderFile);
    if (read.status != storage::JsonReadStatus::success) {
        return std::nullopt;
    }
    return profiles::reader_settings_from_json(read.document);
}

static std::optional<DictionaryProfileSettings> load_dictionary_from(
    const std::filesystem::path& dir) {
    const auto read = storage::read_json_file(dir / kDictionaryFile);
    if (read.status != storage::JsonReadStatus::success) {
        return std::nullopt;
    }
    return profiles::dictionary_settings_from_json(read.document);
}

// ---------------------------------------------------------------------------
// Active profile
// ---------------------------------------------------------------------------

ReaderLoad SettingsStore::active_reader() const {
    const auto dir = profile_directory(active_profile_id_);
    if (!dir) {
        return {};  // missing: defaults
    }
    const auto read = storage::read_json_file(*dir / kReaderFile);
    ReaderLoad result;
    if (read.status == storage::JsonReadStatus::missing) {
        result.status = LoadStatus::missing;
    } else if (read.status == storage::JsonReadStatus::success) {
        if (auto decoded = profiles::reader_settings_from_json(read.document)) {
            result.status = LoadStatus::success;
            result.settings = std::move(*decoded);
        } else {
            result.status = LoadStatus::invalid;
            result.error = "unexpected reader settings format";
        }
    } else {
        result.status = LoadStatus::invalid;
        result.error = read.error;
    }
    return result;
}

DictionaryLoad SettingsStore::active_dictionary() const {
    const auto dir = profile_directory(active_profile_id_);
    if (!dir) {
        return {};  // missing: defaults
    }
    const auto read = storage::read_json_file(*dir / kDictionaryFile);
    DictionaryLoad result;
    if (read.status == storage::JsonReadStatus::missing) {
        result.status = LoadStatus::missing;
    } else if (read.status == storage::JsonReadStatus::success) {
        if (auto decoded = profiles::dictionary_settings_from_json(read.document)) {
            result.status = LoadStatus::success;
            result.settings = std::move(*decoded);
        } else {
            result.status = LoadStatus::invalid;
            result.error = "unexpected dictionary settings format";
        }
    } else {
        result.status = LoadStatus::invalid;
        result.error = read.error;
    }
    return result;
}

StoreError SettingsStore::save_active_reader(const ReaderProfileSettings& settings) {
    return save_reader(active_profile_id_, settings);
}

StoreError SettingsStore::save_active_dictionary(const DictionaryProfileSettings& settings) {
    return save_dictionary(active_profile_id_, settings);
}

// ---------------------------------------------------------------------------
// Arbitrary profile access
// ---------------------------------------------------------------------------

std::optional<ReaderProfileSettings> SettingsStore::load_reader(
    std::string_view profile_id) const {
    const auto dir = profile_directory(profile_id);
    if (!dir) {
        return std::nullopt;
    }
    return load_reader_from(*dir);
}

std::optional<DictionaryProfileSettings> SettingsStore::load_dictionary(
    std::string_view profile_id) const {
    const auto dir = profile_directory(profile_id);
    if (!dir) {
        return std::nullopt;
    }
    return load_dictionary_from(*dir);
}

StoreError SettingsStore::save_reader(std::string_view profile_id,
                                       const ReaderProfileSettings& settings) {
    const auto dir = profile_directory(profile_id);
    if (!dir) {
        return StoreError::unsafe_profile_id;
    }
    const bool ok = storage::write_json_file(*dir / kReaderFile,
                                              profiles::reader_settings_to_json(settings));
    return ok ? StoreError::none : StoreError::io_error;
}

StoreError SettingsStore::save_dictionary(std::string_view profile_id,
                                           const DictionaryProfileSettings& settings) {
    const auto dir = profile_directory(profile_id);
    if (!dir) {
        return StoreError::unsafe_profile_id;
    }
    const bool ok = storage::write_json_file(
        *dir / kDictionaryFile, profiles::dictionary_settings_to_json(settings));
    return ok ? StoreError::none : StoreError::io_error;
}

DictionaryProfileSettings SettingsStore::dictionary_settings_for(
    std::string_view profile_id, const DictionaryProfileSettings& fallback) const {
    const auto dir = profile_directory(profile_id);
    if (!dir) {
        return fallback;
    }
    const auto decoded = load_dictionary_from(*dir);
    return decoded.value_or(fallback);
}

}  // namespace niratan::settings
