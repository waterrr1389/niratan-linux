// SPDX-License-Identifier: GPL-3.0-or-later
//
// State and commands for the profile page, following
// Features/Settings/ProfilesView.swift (the active profile list, the row
// checkmark, language caption and Built-in marker, the Create and Rename
// editor, the delete confirmation and the error alert) and
// Core/ProfileRepository.swift (profile(id:), activeProfile, createProfile,
// renameProfile, deleteProfile, setGlobalActiveProfile).
//
// The list keeps the repository's order, so the built-in default stays first.
// "name" is HoshiProfile::display_name (the localized built-in name),
// "language" is the repository raw value "ja" or "en", "builtIn" marks the
// profile with the built-in ID, and "active" compares against the stored
// global active profile ID. The repository refuses to delete the profile whose
// is_default flag is set and the built-in ID is the profile normalization
// always marks that way, which is why the page hides Delete for it.
//
// createProfile copies the five profile-owned files from the active profile,
// exactly as createProfile(copyFromProfileID:) does, then makes the new
// profile active. A blank name or a failed index write reports an error and
// changes nothing; the profile is created and stored before activation runs,
// so a failed activation still leaves the new profile in the list and returns
// false.
//
// Activation only changes the stored active profile ID. Reader and dictionary
// settings are read per profile when those pages next load their settings;
// this class does not swap them live, and it does not clear the Suwayomi
// connection store because that store has no Linux port yet. Deletion has the
// same boundary: the repository drops the profile's directory and its
// primary-profile entries, while pages that already loaded their settings
// keep them until they load again.
//
// An empty storage::data_root() (no XDG_DATA_HOME and no HOME) leaves the
// class without a repository: the list stays empty, error reports that no data
// directory exists, and mutations fail instead of writing a relative Profiles
// directory into the working directory. A damaged profiles.json cannot be
// surfaced here because the repository falls back to the initial index without
// an error; the bytes stay in place for the user to recover.
//
// The class exposes only QVariantList and strings to QML and keeps no state
// that the repository does not own.

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::viewmodels {

class ProfilesViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

   public:
    explicit ProfilesViewModel(QObject* parent = nullptr);
    ~ProfilesViewModel() override;

    QVariantList profiles() const;
    QString error() const;

    Q_INVOKABLE bool createProfile(const QString& name, const QString& language);
    Q_INVOKABLE bool renameProfile(const QString& id, const QString& name);
    Q_INVOKABLE bool deleteProfile(const QString& id);
    Q_INVOKABLE bool activateProfile(const QString& id);
    Q_INVOKABLE void clearError();

   signals:
    void profilesChanged();
    void errorChanged();

   private:
    void reset_error();
    bool fail(const QString& message);
    bool ensure_repository();

    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels
