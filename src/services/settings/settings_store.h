// SPDX-License-Identifier: GPL-3.0-or-later
//
// The per-profile settings store, following Core/ProfileSettingsStore.swift
// (bootstrap, activate, persistCurrent, dictionarySettings(for:fallback:) and
// the private load/save pair) for reader_settings.json and
// dictionary_settings.json. The global settings.json belongs to
// services/settings/global_settings.h.
//
// The store mirrors ProfileSettingsStore: a profile owns
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

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/profiles/dictionary_settings.h"
#include "models/profiles/reader_settings.h"
#include "services/settings/settings_status.h"

namespace niratan::settings {

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

   private:
    std::optional<std::filesystem::path> profile_directory(
        std::string_view profile_id) const;

    std::filesystem::path data_root_;
    std::filesystem::path profiles_directory_;
    std::string active_profile_id_;
};

}  // namespace niratan::settings
