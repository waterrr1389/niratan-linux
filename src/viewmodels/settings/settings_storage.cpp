// SPDX-License-Identifier: GPL-3.0-or-later
//
// Loading and saving for SettingsViewModel: construction, reload and the
// writes to the shared settings store and the global settings document.

#include "settings_view_model.h"

#include "settings_view_model_state.h"

#include <QString>

#include <utility>

#include "models/profiles/dictionary_settings.h"
#include "models/profiles/reader_settings.h"
#include "services/settings/global_settings.h"
#include "services/settings/settings_store.h"

namespace niratan::viewmodels {

using settings::GlobalSettings;
using settings::SettingsStore;
using settings::StoreError;

void SettingsViewModel::State::load_profile_settings() {
    reader_error.clear();
    dictionary_error.clear();
    {
        const auto result = store->active_reader();
        reader_writable = result.status != settings::LoadStatus::invalid;
        reader = result.status == settings::LoadStatus::invalid
            ? profiles::default_reader_settings()
            : result.settings;
        if (!reader_writable) {
            reader_error = SettingsViewModel::tr("Could not load reader_settings.json: %1.")
                               .arg(QString::fromStdString(result.error));
        }
    }
    {
        const auto result = store->active_dictionary();
        dictionary_writable = result.status != settings::LoadStatus::invalid;
        dictionary = result.status == settings::LoadStatus::invalid
            ? profiles::default_dictionary_settings()
            : result.settings;
        if (!dictionary_writable) {
            dictionary_error = SettingsViewModel::tr("Could not load dictionary_settings.json: %1.")
                                   .arg(QString::fromStdString(result.error));
        }
    }
}

SettingsViewModel::SettingsViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->store = SettingsStore::shared();
    if (!state_->store) {
        state_->profile_error = tr("The Niratan data folder could not be located.");
        return;
    }
    state_->load_profile_settings();

    GlobalSettings global = GlobalSettings::at_data_root();
    const auto load = global.statistics_preferences();
    if (load.status == settings::LoadStatus::invalid) {
        state_->statistics_error =
            tr("Could not load settings.json: %1.").arg(QString::fromStdString(load.error));
    }
    state_->statistics = load.preferences;
    state_->global = std::move(global);
}

// ProfileSettingsStore.activate: show the active profile's settings again and
// let the other consumers of the shared store re-read too.
void SettingsViewModel::reload() {
    if (!state_->store) {
        return;
    }
    state_->load_profile_settings();
    emit errorChanged();
    emit readerChanged();
    emit dictionaryChanged();
    state_->store->refresh();
}

const settings::SettingsStore* SettingsViewModel::store() const {
    return state_->store.get();
}

void SettingsViewModel::save_reader() {
    if (!state_->reader_writable || !state_->store)
        return;
    const auto err = state_->store->update_active_reader(
        [this](profiles::ReaderProfileSettings& stored) { stored = state_->reader; });
    if (err != StoreError::none)
        set_error(state_->reader_error, tr("Could not save reader_settings.json."));
    else
        set_error(state_->reader_error, {});
}

void SettingsViewModel::save_dictionary() {
    if (!state_->dictionary_writable || !state_->store)
        return;
    // A dictionary document that became damaged is never replaced.
    const auto err = state_->store->active_dictionary().status == settings::LoadStatus::invalid
        ? StoreError::io_error
        : state_->store->save_active_dictionary(state_->dictionary);
    if (err != StoreError::none)
        set_error(state_->dictionary_error, tr("Could not save dictionary_settings.json."));
    else
        set_error(state_->dictionary_error, {});
}

void SettingsViewModel::report_statistics_save(StoreError error) {
    set_error(state_->statistics_error,
              error == StoreError::none ? QString() : tr("Could not save settings.json."));
}

void SettingsViewModel::set_error(QString& slot, QString error) {
    if (slot != error) {
        slot = std::move(error);
        emit errorChanged();
    }
}

}  // namespace niratan::viewmodels
