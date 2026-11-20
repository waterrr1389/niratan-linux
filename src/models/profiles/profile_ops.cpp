// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile_ops.h"

#include <QString>

#include <algorithm>
#include <utility>

namespace niratan::profiles {
namespace {

// The legacy video profile keeps its directory but is never a default.
constexpr std::string_view legacy_video_profile_id = "default-ja-video";

bool same_id(const std::string& id, std::string_view other) {
    return std::string_view(id) == other;
}

const HoshiProfile* find_profile(const ProfileIndex& index, std::string_view profile_id) {
    const auto found = std::find_if(index.profiles.begin(), index.profiles.end(),
                                    [profile_id](const HoshiProfile& profile) {
                                        return same_id(profile.id, profile_id);
                                    });
    return found == index.profiles.end() ? nullptr : &*found;
}

bool has_id(const ProfileIndex& index, std::string_view profile_id) {
    return find_profile(index, profile_id) != nullptr;
}

// Names are trimmed by Unicode whitespace and newlines, as
// NSString.trimmingCharacters(in: .whitespacesAndNewlines) does.
std::optional<std::string> trimmed_name(const std::string& name) {
    const QString trimmed = QString::fromStdString(name).trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }
    return trimmed.toStdString();
}

bool is_primary_language(const std::string& language) {
    return language == "ja" || language == "en";
}

}  // namespace

bool is_safe_profile_id(std::string_view profile_id) {
    if (profile_id.empty() || profile_id == "." || profile_id == "..") {
        return false;
    }
    return profile_id.find('/') == std::string_view::npos &&
           profile_id.find('\\') == std::string_view::npos;
}

ProfileIndex normalize_profile_index(const ProfileIndex& index) {
    const HoshiProfile built_in = default_japanese_profile();

    ProfileIndex result;
    result.profiles.reserve(index.profiles.size() + 1);
    for (const HoshiProfile& profile : index.profiles) {
        if (is_safe_profile_id(profile.id)) {
            result.profiles.push_back(profile);
        }
    }
    if (result.profiles.empty()) {
        result.profiles.push_back(built_in);
    }

    const auto built_in_profile =
        std::find_if(result.profiles.begin(), result.profiles.end(),
                     [&built_in](const HoshiProfile& profile) {
                         return profile.id == built_in.id;
                     });
    if (built_in_profile != result.profiles.end()) {
        if (built_in_profile->name == "Japanese" || built_in_profile->name == "Japanese EPUB") {
            built_in_profile->name = built_in.name;
        }
        built_in_profile->is_default = true;
    } else {
        result.profiles.insert(result.profiles.begin(), built_in);
    }

    for (HoshiProfile& profile : result.profiles) {
        if (same_id(profile.id, legacy_video_profile_id)) {
            profile.is_default = false;
        }
    }

    const bool stored_default_is_usable =
        index.default_profile_id != legacy_video_profile_id &&
        has_id(result, index.default_profile_id);
    const std::string& default_id = stored_default_is_usable ? index.default_profile_id : built_in.id;
    const std::string& active_id = has_id(result, index.global_active_profile_id)
                                       ? index.global_active_profile_id
                                       : default_id;

    result.default_profile_id = default_id;
    result.global_active_profile_id = active_id;

    for (const auto& [language, profile_id] : index.primary_profile_ids_by_language) {
        if (is_primary_language(language) && has_id(result, profile_id)) {
            result.primary_profile_ids_by_language.emplace(language, profile_id);
        }
    }
    if (result.primary_profile_ids_by_language.find("ja") ==
        result.primary_profile_ids_by_language.end()) {
        result.primary_profile_ids_by_language.emplace("ja", default_id);
    }

    return result;
}

std::optional<ProfileError> create_profile(ProfileIndex& index, HoshiProfile profile,
                                           std::optional<std::string> copy_from) {
    const std::optional<std::string> name = trimmed_name(profile.name);
    if (!name.has_value()) {
        return ProfileError::blank_name;
    }
    if (copy_from.has_value() && !has_id(index, *copy_from)) {
        return ProfileError::unknown_profile;
    }

    profile.name = *name;
    const std::string_view primary_language = language_id(profile.language());
    index.profiles.push_back(std::move(profile));
    if (index.primary_profile_ids_by_language.find(primary_language) ==
        index.primary_profile_ids_by_language.end()) {
        index.primary_profile_ids_by_language.emplace(std::string(primary_language),
                                                     index.profiles.back().id);
    }

    index = normalize_profile_index(index);
    return std::nullopt;
}

std::optional<ProfileError> rename_profile(ProfileIndex& index, std::string_view profile_id,
                                           std::string name) {
    const std::optional<std::string> trimmed = trimmed_name(name);
    if (!trimmed.has_value()) {
        return ProfileError::blank_name;
    }
    const auto found = std::find_if(index.profiles.begin(), index.profiles.end(),
                                    [profile_id](const HoshiProfile& profile) {
                                        return same_id(profile.id, profile_id);
                                    });
    if (found == index.profiles.end()) {
        return ProfileError::unknown_profile;
    }

    found->name = *trimmed;
    index = normalize_profile_index(index);
    return std::nullopt;
}

std::optional<ProfileError> delete_profile(ProfileIndex& index, std::string_view profile_id) {
    const auto found = std::find_if(index.profiles.begin(), index.profiles.end(),
                                    [profile_id](const HoshiProfile& profile) {
                                        return same_id(profile.id, profile_id);
                                    });
    if (found == index.profiles.end()) {
        return ProfileError::unknown_profile;
    }
    if (found->is_default || same_id(index.default_profile_id, profile_id)) {
        return ProfileError::cannot_delete_default_profile;
    }

    index.profiles.erase(found);
    if (same_id(index.global_active_profile_id, profile_id)) {
        index.global_active_profile_id = index.default_profile_id;
    }
    std::erase_if(index.primary_profile_ids_by_language, [profile_id](const auto& entry) {
        return same_id(entry.second, profile_id);
    });

    index = normalize_profile_index(index);
    return std::nullopt;
}

std::optional<ProfileError> set_active_profile(ProfileIndex& index, std::string_view profile_id) {
    if (!has_id(index, profile_id)) {
        return ProfileError::unknown_profile;
    }

    index.global_active_profile_id = profile_id;
    index = normalize_profile_index(index);
    return std::nullopt;
}

std::optional<ProfileError> set_primary_profile(ProfileIndex& index, std::string_view profile_id,
                                                ContentLanguage language) {
    const HoshiProfile* profile = find_profile(index, profile_id);
    if (profile == nullptr) {
        return ProfileError::unknown_profile;
    }
    if (profile->language() != language) {
        return ProfileError::language_mismatch;
    }

    index.primary_profile_ids_by_language[std::string(language_id(language))] =
        std::string(profile_id);
    index = normalize_profile_index(index);
    return std::nullopt;
}

}  // namespace niratan::profiles
