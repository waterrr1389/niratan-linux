// SPDX-License-Identifier: GPL-3.0-or-later

#include "profiles_view_model.h"

#include <QVariantMap>

#include <optional>
#include <string>

#include "models/profiles/content_language.h"
#include "models/profiles/profile_index.h"
#include "models/profiles/profile_ops.h"
#include "services/anki/anki_config.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

namespace niratan::viewmodels {
namespace {

using profiles::ContentLanguage;
using profiles::CreateProfileResult;
using profiles::HoshiProfile;
using profiles::ProfileError;
using profiles::ProfileIndex;
using profiles::ProfileRepository;
using profiles::default_japanese_profile;
using profiles::language_id;
using profiles::normalize_language;

// ProfileRepositoryError.errorDescription, plus the storage failure the Qt
// repository reports when the index cannot be written.
QString message_for(ProfileError error) {
    switch (error) {
        case ProfileError::unsafe_profile_id:
            return ProfilesViewModel::tr("Unsafe profile identifier.");
        case ProfileError::unknown_profile:
            return ProfilesViewModel::tr("The selected profile no longer exists.");
        case ProfileError::blank_name:
            return ProfilesViewModel::tr("Profile name cannot be empty.");
        case ProfileError::cannot_delete_default_profile:
            return ProfilesViewModel::tr("The default profile cannot be deleted.");
        case ProfileError::language_mismatch:
            return ProfilesViewModel::tr("The profile language does not match.");
        case ProfileError::storage_error:
            return ProfilesViewModel::tr("Could not write the profile index.");
    }
    return ProfilesViewModel::tr("Could not update the profile.");
}

// Core/AnkiManager.activateProfile for the profile that just became active:
// load its Anki configuration, autofill the field mappings from its selected
// note type, and write only when they changed. The outgoing profile is not
// rewritten because every edit already saved (Decision 4).
void autofill_active_profile(const ProfileRepository& repository) {
    anki::AnkiConfigStore store{storage::data_root(), repository};
    store.load();
    if (store.autofill_field_mappings()) {
        store.save();
    }
}

}  // namespace

struct ProfilesViewModel::State {
    State() {
        repository = ProfileRepository::at_data_root();
        if (!repository.has_value()) {
            error = ProfilesViewModel::tr("Niratan has no data directory.");
        }
    }

    // An empty optional means the data root is unusable, not that the index
    // failed to load: the repository itself always starts from a valid index.
    std::optional<ProfileRepository> repository;
    QVariantList profiles_view;
    QString error;

    void rebuild() {
        profiles_view.clear();
        if (!repository.has_value()) {
            return;
        }

        const ProfileIndex& index = repository->index();
        for (const HoshiProfile& profile : index.profiles) {
            QVariantMap entry;
            entry.insert(QStringLiteral("id"), QString::fromStdString(profile.id));
            entry.insert(QStringLiteral("name"), QString::fromStdString(profile.display_name()));
            entry.insert(QStringLiteral("language"),
                         QString::fromStdString(std::string(language_id(profile.language()))));
            entry.insert(QStringLiteral("isDefault"), profile.is_default);
            entry.insert(QStringLiteral("active"),
                         profile.id == index.global_active_profile_id);
            entry.insert(QStringLiteral("builtIn"),
                         profile.id == default_japanese_profile().id);
            profiles_view.append(entry);
        }
    }
};

ProfilesViewModel::ProfilesViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->rebuild();
}

ProfilesViewModel::~ProfilesViewModel() = default;

QVariantList ProfilesViewModel::profiles() const { return state_->profiles_view; }

QString ProfilesViewModel::error() const { return state_->error; }

void ProfilesViewModel::clearError() { reset_error(); }

void ProfilesViewModel::reset_error() {
    if (state_->error.isEmpty()) {
        return;
    }
    state_->error.clear();
    emit errorChanged();
}

bool ProfilesViewModel::fail(const QString& message) {
    state_->error = message;
    emit errorChanged();
    return false;
}

bool ProfilesViewModel::ensure_repository() {
    if (state_->repository.has_value()) {
        return true;
    }
    return fail(tr("Niratan has no data directory."));
}

bool ProfilesViewModel::createProfile(const QString& name, const QString& language) {
    reset_error();
    if (!ensure_repository()) {
        return false;
    }

    const std::optional<ContentLanguage> content_language =
        normalize_language(language.toStdString());
    if (!content_language.has_value()) {
        return fail(tr("Unknown profile language."));
    }

    // The source is the profile that is active now, as ProfilesView.swift
    // passes repository.activeProfile.id.
    const std::string source = state_->repository->active_profile().id;
    const CreateProfileResult created =
        state_->repository->create_profile(name.toStdString(), *content_language, source);
    if (!created.profile.has_value()) {
        return fail(message_for(created.error.value_or(ProfileError::storage_error)));
    }

    // The index write happens before activation, so the new profile exists
    // even when the follow-up activation fails.
    state_->rebuild();
    emit profilesChanged();

    if (const std::optional<ProfileError> error =
            state_->repository->set_active_profile(created.profile->id)) {
        return fail(message_for(*error));
    }

    autofill_active_profile(*state_->repository);

    state_->rebuild();
    emit profilesChanged();
    return true;
}

bool ProfilesViewModel::renameProfile(const QString& id, const QString& name) {
    reset_error();
    if (!ensure_repository()) {
        return false;
    }

    if (const std::optional<ProfileError> error =
            state_->repository->rename_profile(id.toStdString(), name.toStdString())) {
        return fail(message_for(*error));
    }

    state_->rebuild();
    emit profilesChanged();
    return true;
}

bool ProfilesViewModel::deleteProfile(const QString& id) {
    reset_error();
    if (!ensure_repository()) {
        return false;
    }

    const std::string previous_active = state_->repository->index().global_active_profile_id;
    if (const std::optional<ProfileError> error =
            state_->repository->delete_profile(id.toStdString())) {
        return fail(message_for(*error));
    }

    // Only a delete that moved the active profile runs the Anki step, for the
    // profile that took over.
    if (state_->repository->index().global_active_profile_id != previous_active) {
        autofill_active_profile(*state_->repository);
    }

    state_->rebuild();
    emit profilesChanged();
    return true;
}

bool ProfilesViewModel::activateProfile(const QString& id) {
    reset_error();
    if (!ensure_repository()) {
        return false;
    }

    const std::string previous_active = state_->repository->index().global_active_profile_id;
    if (const std::optional<ProfileError> error =
            state_->repository->set_active_profile(id.toStdString())) {
        return fail(message_for(*error));
    }

    // AnkiManager.activateProfile's guard: the already active profile does
    // nothing, so no autofill and no write.
    if (state_->repository->index().global_active_profile_id != previous_active) {
        autofill_active_profile(*state_->repository);
    }

    state_->rebuild();
    emit profilesChanged();
    return true;
}

}  // namespace niratan::viewmodels
