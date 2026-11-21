// SPDX-License-Identifier: GPL-3.0-or-later
//
// The active profile's content language and dictionary settings, kept
// current while another window edits them. Swift reaches the same values
// through Core/ProfileSettingsStore.swift (activate, the private load) and
// Core/UserConfig.swift (apply(dictionaryProfileSettings:)), observed in
// memory; the Linux windows share them through files instead.
//
// The index is read from <data root>/Profiles/profiles.json and resolved with
// models/profiles resolve_profile, without constructing ProfileRepository:
// its construction can rewrite the index, which would notify the watch. The
// profile's <data root>/Profiles/<id>/dictionary_settings.json supplies the
// settings through resolved_dictionary_settings. A missing file, an unsafe
// profile ID or an invalid file seen first gives the defaults; an invalid
// live edit keeps the last good settings of the same profile.
//
// The Profiles directory, the index, the profile directory and its settings
// file are watched. Watching directories survives atomic replacement and
// notices a file that does not exist yet. An empty data root reads nothing
// and watches nothing.

#pragma once

#include <QFileSystemWatcher>
#include <QObject>

#include <filesystem>
#include <string_view>

#include "models/profiles/dictionary_settings.h"

namespace niratan::settings {

class DictionarySettingsWatch : public QObject {
    Q_OBJECT

   public:
    // Under storage::data_root().
    explicit DictionarySettingsWatch(QObject* parent = nullptr);
    explicit DictionarySettingsWatch(std::filesystem::path data_root, QObject* parent = nullptr);

    // Rereads the index and the settings. Returns whether the language, the
    // settings or the settings file changed.
    bool reload();

    std::string_view language_id() const { return language_id_; }
    const profiles::DictionaryProfileSettings& settings() const { return settings_; }

   signals:
    // A watched path changed and reload() reported a change.
    void changed();

   private:
    void on_path_changed();

    std::filesystem::path data_root_;
    QFileSystemWatcher watcher_;
    std::filesystem::path settings_file_;
    profiles::DictionaryProfileSettings settings_ = profiles::default_dictionary_settings();
    std::string_view language_id_ = "ja";
};

}  // namespace niratan::settings
