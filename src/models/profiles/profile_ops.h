// SPDX-License-Identifier: GPL-3.0-or-later
//
// Profile index mutations and normalization without IO, following
// Core/ProfileRepository.swift (isSafeProfileID, normalized, createProfile,
// renameProfile, deleteProfile, setGlobalActiveProfile, setPrimaryProfile).
//
// Every successful mutation ends with normalize_profile_index, because the
// Swift repository calls Self.normalized inside persistIndex. That means an
// operation can be accepted and still have an unsafe profile dropped, a
// legacy default name replaced, or a missing primary profile filled in.
//
// normalize_profile_index mirrors normalized(_:):
//   1. drop profiles whose ID is not safe;
//   2. an empty list becomes the built-in Japanese profile;
//   3. the built-in profile present: its legacy names "Japanese" and
//      "Japanese EPUB" become "Japanese" and is_default is set; absent: it is
//      inserted first;
//   4. the legacy video profile ID "default-ja-video" is never a default;
//   5. the default ID is the stored one when it exists and is not the legacy
//      video ID, else the built-in ID;
//   6. the active ID is the stored one when it exists, else the default ID;
//   7. primary IDs are kept only for the ja/en languages and existing
//      profiles, and ja always has one.
//
// create_profile takes the already generated profile; the repository owns ID
// generation and file copying. Names are trimmed by Unicode whitespace and
// newlines, and a blank name fails.

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "models/profiles/profile_index.h"

namespace niratan::profiles {

enum class ProfileError {
    unsafe_profile_id,
    unknown_profile,
    blank_name,
    cannot_delete_default_profile,
    language_mismatch,
    // The index could not be written; the in-memory change was rolled back.
    storage_error,
};

// Exactly the identifiers that may name a profile directory.
bool is_safe_profile_id(std::string_view profile_id);

ProfileIndex normalize_profile_index(const ProfileIndex& index);

// The new profile is appended; copy_from is validated but not copied here.
// The return value is the error, or nothing on success.
std::optional<ProfileError> create_profile(ProfileIndex& index, HoshiProfile profile,
                                           std::optional<std::string> copy_from);

std::optional<ProfileError> rename_profile(ProfileIndex& index, std::string_view profile_id,
                                           std::string name);

std::optional<ProfileError> delete_profile(ProfileIndex& index, std::string_view profile_id);

std::optional<ProfileError> set_active_profile(ProfileIndex& index, std::string_view profile_id);

std::optional<ProfileError> set_primary_profile(ProfileIndex& index, std::string_view profile_id,
                                                ContentLanguage language);

}  // namespace niratan::profiles
