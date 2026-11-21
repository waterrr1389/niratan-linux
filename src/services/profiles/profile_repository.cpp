// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile_repository.h"

#include <QString>
#include <QUuid>

#include <algorithm>
#include <system_error>
#include <utility>

#include "models/profiles/profile_index_json.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

namespace niratan::profiles {
namespace {

// The files a new profile inherits from its source, in the order Swift copies
// them; nothing else is profile-owned.
constexpr std::string_view owned_file_names[] = {
    "dictionary_config.json", "collapsed.json", "dictionary_settings.json", "anki_config.json",
    "reader_settings.json"};

std::string generate_profile_id() {
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return std::string("profile-") + uuid.toLower().toStdString();
}

bool ensure_directory(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return !error;
}

}  // namespace

std::optional<ProfileRepository> ProfileRepository::at_data_root() {
    std::filesystem::path root = storage::data_root();
    if (root.empty()) {
        return std::nullopt;
    }
    return ProfileRepository(std::move(root));
}

ProfileRepository::ProfileRepository(std::filesystem::path app_directory)
    : app_directory_(std::move(app_directory)),
      profiles_directory_(app_directory_ / "Profiles"),
      index_file_(profiles_directory_ / "profiles.json"),
      index_(ProfileIndex::initial()) {
    ensure_directory(profiles_directory_);

    const storage::JsonReadResult read = storage::read_json_file(index_file_);
    switch (read.status) {
        case storage::JsonReadStatus::missing:
            index_ = ProfileIndex::initial();
            persist(index_);
            break;
        case storage::JsonReadStatus::success:
            // A document that parses but does not decode is treated like a
            // damaged file: the bytes stay untouched for recovery.
            if (const std::optional<ProfileIndex> decoded =
                    profile_index_from_json(read.document)) {
                index_ = normalize_profile_index(*decoded);
                persist(index_);
            } else {
                index_ = ProfileIndex::initial();
            }
            break;
        case storage::JsonReadStatus::invalid:
            index_ = ProfileIndex::initial();
            break;
    }

    if (const std::optional<std::filesystem::path> directory =
            profile_directory(index_.default_profile_id)) {
        ensure_directory(*directory);
    }
}

const ProfileIndex& ProfileRepository::index() const {
    return index_;
}

HoshiProfile ProfileRepository::active_profile() const {
    return resolve_profile(index_);
}

std::optional<HoshiProfile> ProfileRepository::profile(std::string_view profile_id) const {
    for (const HoshiProfile& candidate : index_.profiles) {
        if (candidate.id == profile_id) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::vector<HoshiProfile> ProfileRepository::profiles(ContentLanguage language) const {
    std::vector<HoshiProfile> result;
    for (const HoshiProfile& candidate : index_.profiles) {
        if (candidate.language() == language) {
            result.push_back(candidate);
        }
    }
    return result;
}

std::optional<std::filesystem::path> ProfileRepository::profile_directory(
    std::string_view profile_id) const {
    if (!is_safe_profile_id(profile_id)) {
        return std::nullopt;
    }
    return profiles_directory_ / std::string(profile_id);
}

CreateProfileResult ProfileRepository::create_profile(std::string name, ContentLanguage language,
                                                      std::optional<std::string> copy_from) {
    const std::string id = generate_profile_id();

    ProfileIndex candidate = index_;
    HoshiProfile profile{id, std::move(name), std::string(language_id(language)), false};
    if (const std::optional<ProfileError> error =
            ::niratan::profiles::create_profile(candidate, std::move(profile), copy_from)) {
        return {{}, error};
    }

    const auto stored = std::find_if(candidate.profiles.begin(), candidate.profiles.end(),
                                     [&id](const HoshiProfile& entry) { return entry.id == id; });
    if (stored == candidate.profiles.end()) {
        // The generated ID is safe, so normalization always keeps it.
        return {{}, ProfileError::storage_error};
    }
    const HoshiProfile created = *stored;

    const std::optional<std::filesystem::path> destination = profile_directory(id);
    if (!destination.has_value() || !ensure_directory(*destination)) {
        return {{}, ProfileError::storage_error};
    }

    if (copy_from.has_value()) {
        if (const std::optional<std::filesystem::path> source =
                profile_directory(*copy_from)) {
            if (!copy_owned_files(*source, *destination)) {
                std::error_code remove_error;
                std::filesystem::remove_all(*destination, remove_error);
                return {{}, ProfileError::storage_error};
            }
        }
    }

    if (!persist(candidate)) {
        std::error_code remove_error;
        std::filesystem::remove_all(*destination, remove_error);
        return {{}, ProfileError::storage_error};
    }

    index_ = std::move(candidate);
    return {created, std::nullopt};
}

std::optional<ProfileError> ProfileRepository::rename_profile(std::string_view profile_id,
                                                             std::string name) {
    ProfileIndex candidate = index_;
    if (const std::optional<ProfileError> error =
            ::niratan::profiles::rename_profile(candidate, profile_id, std::move(name))) {
        return error;
    }
    if (!persist(candidate)) {
        return ProfileError::storage_error;
    }
    index_ = std::move(candidate);
    return std::nullopt;
}

std::optional<ProfileError> ProfileRepository::delete_profile(std::string_view profile_id) {
    const std::optional<std::filesystem::path> directory = profile_directory(profile_id);

    ProfileIndex candidate = index_;
    if (const std::optional<ProfileError> error =
            ::niratan::profiles::delete_profile(candidate, profile_id)) {
        return error;
    }
    if (!persist(candidate)) {
        return ProfileError::storage_error;
    }

    index_ = std::move(candidate);
    if (directory.has_value()) {
        std::error_code remove_error;
        std::filesystem::remove_all(*directory, remove_error);
    }
    return std::nullopt;
}

std::optional<ProfileError> ProfileRepository::set_active_profile(std::string_view profile_id) {
    ProfileIndex candidate = index_;
    if (const std::optional<ProfileError> error =
            ::niratan::profiles::set_active_profile(candidate, profile_id)) {
        return error;
    }
    if (!persist(candidate)) {
        return ProfileError::storage_error;
    }
    index_ = std::move(candidate);
    return std::nullopt;
}

std::optional<ProfileError> ProfileRepository::set_primary_profile(std::string_view profile_id,
                                                                  ContentLanguage language) {
    ProfileIndex candidate = index_;
    if (const std::optional<ProfileError> error =
            ::niratan::profiles::set_primary_profile(candidate, profile_id, language)) {
        return error;
    }
    if (!persist(candidate)) {
        return ProfileError::storage_error;
    }
    index_ = std::move(candidate);
    return std::nullopt;
}

bool ProfileRepository::persist(const ProfileIndex& index) {
    return storage::write_json_file(index_file_, profile_index_to_json(index));
}

bool ProfileRepository::copy_owned_files(const std::filesystem::path& source,
                                         const std::filesystem::path& destination) {
    for (const std::string_view name : owned_file_names) {
        const std::filesystem::path source_file = source / std::string(name);
        std::error_code exists_error;
        if (!std::filesystem::exists(source_file, exists_error)) {
            continue;
        }
        std::error_code copy_error;
        std::filesystem::copy_file(source_file, destination / std::string(name), copy_error);
        if (copy_error) {
            return false;
        }
    }
    return true;
}

}  // namespace niratan::profiles
