// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings_store.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/profiles/dictionary_settings.h"
#include "models/profiles/profile_index.h"
#include "models/profiles/profile_index_json.h"
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
static constexpr std::string_view kIndexFile = "profiles.json";

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

std::shared_ptr<SettingsStore> SettingsStore::shared() {
    // One store per data root, alive while a consumer holds it.
    static std::map<std::filesystem::path, std::weak_ptr<SettingsStore>> stores;
    const auto root = storage::data_root();
    if (root.empty()) {
        return nullptr;
    }
    std::erase_if(stores, [](const auto& entry) { return entry.second.expired(); });
    if (auto existing = stores[root].lock()) {
        return existing;
    }
    auto store = std::make_shared<SettingsStore>(root);
    stores[root] = store;
    return store;
}

SettingsStore::SettingsStore(std::filesystem::path data_root)
    : data_root_(std::move(data_root)),
      profiles_directory_(data_root_ / kProfilesDir) {
    profiles::ProfileRepository repository(data_root_);
    active_profile_id_ = repository.active_profile().id;
}

// ProfileSettingsStore.activate follows the repository's active profile; the
// index is read on each call so a switch made by another repository instance
// is seen. An index that cannot be read resolves like the repository does for
// a damaged file: to the id resolved at construction.
std::string SettingsStore::active_profile_id() const {
    const auto read = storage::read_json_file(profiles_directory_ / kIndexFile);
    if (read.status == storage::JsonReadStatus::success) {
        if (const auto decoded = profiles::profile_index_from_json(read.document)) {
            return profiles::resolve_profile(profiles::normalize_profile_index(*decoded)).id;
        }
    }
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
    const auto dir = profile_directory(active_profile_id());
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
    const auto dir = profile_directory(active_profile_id());
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
    return save_reader(active_profile_id(), settings);
}

StoreError SettingsStore::save_active_dictionary(const DictionaryProfileSettings& settings) {
    return save_dictionary(active_profile_id(), settings);
}

StoreError
SettingsStore::update_active_reader(const std::function<void(ReaderProfileSettings&)>& edit) {
    const std::string profile_id = active_profile_id();
    const auto dir = profile_directory(profile_id);
    if (!dir) {
        return StoreError::unsafe_profile_id;
    }
    // Start from the file's current contents so fields other consumers wrote
    // since they last read are kept. A damaged file is never replaced.
    ReaderProfileSettings settings = profiles::default_reader_settings();
    const auto read = storage::read_json_file(*dir / kReaderFile);
    if (read.status == storage::JsonReadStatus::success) {
        auto decoded = profiles::reader_settings_from_json(read.document);
        if (!decoded) {
            return StoreError::io_error;
        }
        settings = std::move(*decoded);
    } else if (read.status != storage::JsonReadStatus::missing) {
        return StoreError::io_error;
    }
    edit(settings);
    return save_reader(profile_id, settings);
}

std::size_t SettingsStore::subscribe(std::function<void()> listener) {
    listeners_.emplace(next_token_, std::move(listener));
    return next_token_++;
}

void SettingsStore::unsubscribe(std::size_t token) {
    listeners_.erase(token);
}

void SettingsStore::refresh() {
    // A listener may unsubscribe while running; iterate over a copy.
    const auto listeners = listeners_;
    for (const auto& [token, listener] : listeners) {
        listener();
    }
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
