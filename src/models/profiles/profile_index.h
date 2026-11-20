// SPDX-License-Identifier: GPL-3.0-or-later
//
// The profile list and its resolution, following Models/Profile.swift
// (HoshiProfile, ProfileIndex, ProfileResolver.resolve(_:in:)).
//
// A profile carries its dictionary language ID and an is_default flag. The
// language is English only for the exact ID "en"; every other value falls
// back to Japanese, as ContentLanguageProfile(rawValue:) ?? .japanese does.
// The display name of the built-in default profile replaces the two legacy
// names "Japanese" and "Japanese EPUB" with the localized "Japanese"; every
// other name passes through.
//
// Resolution is runtime-global: the active profile ID is looked up first,
// then the default profile ID, then the first entry of the list, and finally
// the built-in default. Profile IDs are expected to be unique; a duplicate
// keeps the first entry, where Swift's Dictionary(uniqueKeysWithValues:)
// would trap.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "models/profiles/content_language.h"

namespace niratan::profiles {

struct HoshiProfile {
    std::string id;
    std::string name;
    std::string dictionary_language_id;
    bool is_default = false;

    ContentLanguage language() const;
    std::string display_name() const;

    bool operator==(const HoshiProfile&) const = default;
};

struct ProfileIndex {
    std::vector<HoshiProfile> profiles;
    std::string default_profile_id;
    std::string global_active_profile_id;
    std::map<std::string, std::string, std::less<>> primary_profile_ids_by_language;

    static ProfileIndex initial();

    bool operator==(const ProfileIndex&) const = default;
};

HoshiProfile default_japanese_profile();

// The global profile the next action uses.
HoshiProfile resolve_profile(const ProfileIndex& index);

}  // namespace niratan::profiles
