// SPDX-License-Identifier: GPL-3.0-or-later
//
// The profile store, following Core/ProfileRepository.swift
// (ProfileRepository: init, profile(id:), activeProfile, createProfile,
// renameProfile, deleteProfile, setGlobalActiveProfile, setPrimaryProfile,
// persistIndex, copyProfileOwnedFiles, profileDirectory).
//
// The app directory holds Profiles/profiles.json and one directory per
// profile. Creation generates "profile-<uuid>" with a lowercase UUID, makes
// the directory, copies the five profile-owned files that exist in the
// source profile, and then persists. Deletion removes the directory only
// after the index was written.
//
// A damaged profiles.json is reported as an invalid read and left untouched:
// the repository starts from ProfileIndex::initial but does not overwrite the
// file, so the user can recover it. A missing file is written with the
// normalized initial index.
//
// Every mutation works on a copy and rolls back when the write fails, so an
// error leaves both the index and the disk as they were. Nothing here
// migrates the legacy macOS files (Dictionaries/config.json, the old video
// profile or the UserDefaults video preference): those need a Linux settings
// store and are recorded as a gap.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/profiles/profile_index.h"
#include "models/profiles/profile_ops.h"

namespace niratan::profiles {

struct CreateProfileResult {
    std::optional<HoshiProfile> profile;
    std::optional<ProfileError> error;
};

class ProfileRepository {
   public:
    // Creates the Profiles directory when needed and loads the index.
    explicit ProfileRepository(std::filesystem::path app_directory);

    const ProfileIndex& index() const;
    HoshiProfile active_profile() const;
    std::optional<HoshiProfile> profile(std::string_view profile_id) const;
    std::vector<HoshiProfile> profiles(ContentLanguage language) const;

    // nullopt when the ID is not safe to use as a directory name.
    std::optional<std::filesystem::path> profile_directory(std::string_view profile_id) const;

    CreateProfileResult create_profile(std::string name, ContentLanguage language,
                                       std::optional<std::string> copy_from);

    std::optional<ProfileError> rename_profile(std::string_view profile_id, std::string name);
    std::optional<ProfileError> delete_profile(std::string_view profile_id);
    std::optional<ProfileError> set_active_profile(std::string_view profile_id);
    std::optional<ProfileError> set_primary_profile(std::string_view profile_id,
                                                    ContentLanguage language);

   private:
    bool persist(const ProfileIndex& index);
    bool copy_owned_files(const std::filesystem::path& source,
                          const std::filesystem::path& destination);

    std::filesystem::path app_directory_;
    std::filesystem::path profiles_directory_;
    std::filesystem::path index_file_;
    ProfileIndex index_;
};

}  // namespace niratan::profiles
