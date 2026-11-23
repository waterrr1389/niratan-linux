// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings_view_model.h"

#include "services/settings/global_settings.h"

namespace niratan::viewmodels {

QString SettingsViewModel::preferredAudioSource() const {
    return QString::fromStdString(
        settings::GlobalSettings::at_data_root().preferred_audio_source());
}

QString SettingsViewModel::savePreferredAudioSource(const QString& source) {
    const auto text = source.trimmed().toStdString();
    if (!settings::GlobalSettings::valid_preferred_audio_source(text)) {
        return tr("Enter an HTTP or HTTPS URL containing {term}, or leave it empty.");
    }
    if (settings::GlobalSettings::at_data_root().save_preferred_audio_source(text) !=
        settings::StoreError::none) {
        return tr("Could not save the audio source.");
    }
    return {};
}

}  // namespace niratan::viewmodels
