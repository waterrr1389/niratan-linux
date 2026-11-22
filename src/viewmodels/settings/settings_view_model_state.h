// SPDX-License-Identifier: GPL-3.0-or-later
//
// Module-private state shared by the SettingsViewModel translation units.
// Nothing outside src/viewmodels/settings/ includes this header.

#pragma once

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

    bool reader_writable = false;
    bool dictionary_writable = false;
    QString profile_error;
    QString reader_error;
    QString dictionary_error;
    QString statistics_error;

    std::optional<settings::SettingsStore> store;
    std::optional<settings::GlobalSettings> global;
};

template <typename Field, typename Value>
void SettingsViewModel::update_reader(Field& field, Value value) {
    if (field != value) {
        field = std::move(value);
        save_reader();
        emit readerChanged();
    }
}

}  // namespace niratan::viewmodels
