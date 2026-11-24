// SPDX-License-Identifier: GPL-3.0-or-later
//
// Module-private state shared by the SettingsViewModel translation units.
// Nothing outside src/viewmodels/settings/ includes this header.

#pragma once

#include <memory>
#include <optional>
#include <utility>

#include "settings_view_model.h"

#include "models/profiles/dictionary_settings.h"
#include "models/profiles/reader_settings.h"
#include "models/statistics/statistics.h"
#include "services/settings/global_settings.h"
#include "services/settings/settings_store.h"

namespace niratan::viewmodels {

struct SettingsViewModel::State {
    profiles::ReaderProfileSettings reader = profiles::default_reader_settings();
    profiles::DictionaryProfileSettings dictionary = profiles::default_dictionary_settings();
    settings::StatisticsPreferences statistics;
    int font_wheel_remainder = 0;

    bool reader_writable = false;
    bool dictionary_writable = false;
    QString profile_error;
    QString reader_error;
    QString dictionary_error;
    QString statistics_error;

    std::shared_ptr<settings::SettingsStore> store;
    std::optional<settings::GlobalSettings> global;

    // Loads the active profile's reader and dictionary settings. A damaged
    // file leaves the defaults in place and makes that document read-only.
    void load_profile_settings();

    // Re-reads the active profile's reader settings into reader so an edit
    // starts from what is stored now: another consumer's change, or another
    // profile after a switch. A damaged file keeps the copy in memory; the save
    // that follows refuses to replace it. Returns whether the copy changed.
    bool refresh_reader() {
        if (!store || !reader_writable) {
            return false;
        }
        const settings::ReaderLoad load = store->active_reader();
        if (load.status == settings::LoadStatus::invalid || load.settings == reader) {
            return false;
        }
        reader = load.settings;
        return true;
    }

    // The dictionary counterpart of refresh_reader.
    bool refresh_dictionary() {
        if (!store || !dictionary_writable) {
            return false;
        }
        const settings::DictionaryLoad load = store->active_dictionary();
        if (load.status == settings::LoadStatus::invalid || load.settings == dictionary) {
            return false;
        }
        dictionary = load.settings;
        return true;
    }
};

template <typename Field, typename Value>
void SettingsViewModel::update_reader(Field& field, Value value) {
    const bool refreshed = state_->refresh_reader();
    if (field != value) {
        field = std::move(value);
        save_reader();
        emit readerChanged();
    } else if (refreshed) {
        emit readerChanged();
    }
}

template <typename Field, typename Value>
void SettingsViewModel::update_dictionary(Field& field, Value value) {
    const bool refreshed = state_->refresh_dictionary();
    if (field != value) {
        field = std::move(value);
        save_dictionary();
        emit dictionaryChanged();
    } else if (refreshed) {
        emit dictionaryChanged();
    }
}

}  // namespace niratan::viewmodels
