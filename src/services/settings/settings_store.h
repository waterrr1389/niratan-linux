// SPDX-License-Identifier: GPL-3.0-or-later
//
// The settings store, following Core/UserConfig.swift (the UserDefaults
// keyed reads and writes for the global settings) and
// Core/ProfileSettingsStore.swift (bootstrap, activate,
// persistCurrent, dictionarySettings(for:fallback:) and the private
// load/save pair) for the per-profile reader_settings.json and
// dictionary_settings.json.
//
// On macOS the global settings live in UserDefaults and are written one key
// at a time; here they are one JSON object in <data root>/settings.json,
// written atomically as a whole document on each change, keeping the other
// keys untouched. A missing file reads as the defaults; a damaged file is
// reported as invalid and never rewritten by a read, so the user can recover
// it. Only the keys the caller names are read or written.
//
// The profile store mirrors ProfileSettingsStore: a profile owns
// reader_settings.json and dictionary_settings.json in its own directory.
// The active profile is resolved once, from ProfileRepository, at
// construction. dictionarySettings(for:fallback:) returns the stored
// settings of a profile that is not the applied one, or the fallback. The
// store does not hold the applied settings in memory; the caller owns the
// values and calls save or load. This differs from Swift, where the store
// mutates a shared UserConfig.
//
// Callers must treat profile IDs as opaque strings from ProfileRepository;
// the store only checks that the ID is a safe directory name.
//
// create() is the production entry point: it calls storage::data_root() and
// returns nullopt when the data root is unusable. The explicit-path
// constructor is for tests. A view model that calls create() never writes
// std::filesystem itself, which is what gets it off the layer gate.

#pragma once

#include <QJsonDocument>
#include <QJsonValue>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/profiles/dictionary_settings.h"
#include "models/profiles/reader_settings.h"

namespace niratan::settings {

enum class LoadStatus { missing, success, invalid };

enum class StoreError { none, unsafe_profile_id, unknown_profile, io_error };

// The active profile's reader settings, with the status of the file.
// On missing, settings are the defaults. On invalid, settings are the
// defaults and error holds the reason; the file is left untouched.
struct ReaderLoad {
    LoadStatus status = LoadStatus::missing;
    profiles::ReaderProfileSettings settings = profiles::default_reader_settings();
    std::string error;
};

struct DictionaryLoad {
    LoadStatus status = LoadStatus::missing;
    profiles::DictionaryProfileSettings settings = profiles::default_dictionary_settings();
    std::string error;
};

// The result of reading the global settings document for a set of keys.
struct GlobalRead {
    enum class Status { missing, success, invalid };
    Status status = Status::missing;
    std::string error;
    std::vector<std::pair<std::string, QJsonValue>> entries;
};

class SettingsStore {
   public:
    // Production entry point. Calls storage::data_root() and resolves the
    // active profile from ProfileRepository. Returns nullopt when the data
    // root is unusable.
    static std::optional<SettingsStore> create();

    // Test entry point. The data root must be a writable directory.
    explicit SettingsStore(std::filesystem::path data_root);

    std::string_view active_profile_id() const;

    ReaderLoad active_reader() const;
    DictionaryLoad active_dictionary() const;

    StoreError save_active_reader(const profiles::ReaderProfileSettings& settings);
    StoreError save_active_dictionary(const profiles::DictionaryProfileSettings& settings);

    // Arbitrary profile access. load_* returns nullopt when the profile has
    // no settings file or the file is damaged.
    std::optional<profiles::ReaderProfileSettings> load_reader(
        std::string_view profile_id) const;
    std::optional<profiles::DictionaryProfileSettings> load_dictionary(
        std::string_view profile_id) const;

    StoreError save_reader(std::string_view profile_id,
                           const profiles::ReaderProfileSettings& settings);
    StoreError save_dictionary(std::string_view profile_id,
                                const profiles::DictionaryProfileSettings& settings);

    // ProfileSettingsStore.dictionarySettings(for:fallback:): the stored
    // dictionary settings of a profile that is not the applied one, or the
    // fallback. An unsafe or missing profile returns the fallback.
    profiles::DictionaryProfileSettings dictionary_settings_for(
        std::string_view profile_id,
        const profiles::DictionaryProfileSettings& fallback) const;

    // Global settings (<data root>/settings.json). Reads only the named keys;
    // a key absent from the document decodes as undefined. The caller
    // supplies defaults for missing keys, as UserConfig does.
    // statisticsResetTime is not normalized here; the caller normalizes it
    // with statistics::normalized_reset_minutes, matching UserConfig's setter.
    GlobalRead read_global(const std::vector<std::string>& keys) const;

    // Writes one key and keeps every other key of the document. A damaged
    // document is never replaced; the write fails with io_error. A missing
    // document is created.
    StoreError write_global(const std::string& key, const QJsonValue& value);

   private:
    std::optional<std::filesystem::path> profile_directory(
        std::string_view profile_id) const;

    std::filesystem::path data_root_;
    std::filesystem::path profiles_directory_;
    std::filesystem::path global_file_;
    std::string active_profile_id_;
};

}  // namespace niratan::settings
