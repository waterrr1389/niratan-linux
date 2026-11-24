// SPDX-License-Identifier: GPL-3.0-or-later

#include "sasayaki_settings_view_model.h"

#include <QString>

#include <cstddef>
#include <memory>
#include <optional>

#include "models/profiles/reader_settings.h"
#include "models/sasayaki/sasayaki_settings.h"
#include "services/settings/global_settings.h"
#include "services/settings/settings_store.h"

namespace niratan::viewmodels {

using niratan::profiles::ReaderProfileSettings;
using niratan::settings::GlobalSettings;
using niratan::settings::LoadStatus;
using niratan::settings::ReaderLoad;
using niratan::settings::SettingsStore;
using niratan::settings::StoreError;

struct SasayakiSettingsViewModel::State {
    GlobalSettings global = GlobalSettings::at_data_root();
    std::shared_ptr<SettingsStore> store;
    std::size_t store_token = 0;

    bool enable = false;
    bool skip_controls = false;
    bool show_sasayaki_toggle = false;
    bool auto_scroll = true;
    bool auto_pause = true;
    QString text_color = QStringLiteral("#000000");
    QString background_color = QStringLiteral("#87CFFA66");
    QString dark_text_color = QStringLiteral("#FFFFFF");
    QString dark_background_color = QStringLiteral("#87CFFA66");
};

SasayakiSettingsViewModel::SasayakiSettingsViewModel(QObject* parent)
    : QObject(parent), state_(std::make_unique<State>()) {
    state_->store = SettingsStore::shared();
    if (state_->store) {
        // Re-read when another consumer of the store reports that the active
        // profile may have changed.
        state_->store_token = state_->store->subscribe([this] { reload(); });
    }
    reload();
}

SasayakiSettingsViewModel::~SasayakiSettingsViewModel() {
    if (state_->store) {
        state_->store->unsubscribe(state_->store_token);
    }
}

bool SasayakiSettingsViewModel::enable() const {
    return state_->enable;
}

bool SasayakiSettingsViewModel::skip_controls() const {
    return state_->skip_controls;
}

bool SasayakiSettingsViewModel::show_sasayaki_toggle() const {
    return state_->show_sasayaki_toggle;
}

bool SasayakiSettingsViewModel::auto_scroll() const {
    return state_->auto_scroll;
}

bool SasayakiSettingsViewModel::auto_pause() const {
    return state_->auto_pause;
}

QString SasayakiSettingsViewModel::text_color() const {
    return state_->text_color;
}

QString SasayakiSettingsViewModel::background_color() const {
    return state_->background_color;
}

QString SasayakiSettingsViewModel::dark_text_color() const {
    return state_->dark_text_color;
}

QString SasayakiSettingsViewModel::dark_background_color() const {
    return state_->dark_background_color;
}

void SasayakiSettingsViewModel::reload() {
    const bool show =
        state_->store ? state_->store->active_reader().settings.show_sasayaki_toggle : false;
    if (show != state_->show_sasayaki_toggle) {
        state_->show_sasayaki_toggle = show;
        emit showSasayakiToggleChanged();
    }

    const sasayaki::SasayakiSettings settings = state_->global.sasayaki_settings();
    if (settings.enable != state_->enable) {
        state_->enable = settings.enable;
        emit enableChanged();
    }
    if (settings.skip_controls != state_->skip_controls) {
        state_->skip_controls = settings.skip_controls;
        emit skipControlsChanged();
    }
    if (settings.auto_scroll != state_->auto_scroll) {
        state_->auto_scroll = settings.auto_scroll;
        emit autoScrollChanged();
    }
    if (settings.auto_pause != state_->auto_pause) {
        state_->auto_pause = settings.auto_pause;
        emit autoPauseChanged();
    }
    const QString text = QString::fromStdString(settings.text_color);
    if (text != state_->text_color) {
        state_->text_color = text;
        emit textColorChanged();
    }
    const QString background = QString::fromStdString(settings.background_color);
    if (background != state_->background_color) {
        state_->background_color = background;
        emit backgroundColorChanged();
    }
    const QString dark_text = QString::fromStdString(settings.dark_text_color);
    if (dark_text != state_->dark_text_color) {
        state_->dark_text_color = dark_text;
        emit darkTextColorChanged();
    }
    const QString dark_background = QString::fromStdString(settings.dark_background_color);
    if (dark_background != state_->dark_background_color) {
        state_->dark_background_color = dark_background;
        emit darkBackgroundColorChanged();
    }
}

void SasayakiSettingsViewModel::setEnable(bool enabled) {
    if (state_->global.save_enable_sasayaki(enabled) != StoreError::none) {
        return;
    }
    const bool stored = state_->global.sasayaki_settings().enable;
    if (stored != state_->enable) {
        state_->enable = stored;
        emit enableChanged();
    }
}

void SasayakiSettingsViewModel::setSkipControls(bool enabled) {
    if (state_->global.save_sasayaki_skip_controls(enabled) != StoreError::none) {
        return;
    }
    const bool stored = state_->global.sasayaki_settings().skip_controls;
    if (stored != state_->skip_controls) {
        state_->skip_controls = stored;
        emit skipControlsChanged();
    }
}

void SasayakiSettingsViewModel::setShowSasayakiToggle(bool show) {
    if (!state_->store) {
        return;
    }
    // A document that is valid JSON but does not decode must not be replaced
    // by defaults: the failed read counts as a failed save (S2).
    const ReaderLoad active = state_->store->active_reader();
    if (active.status == LoadStatus::invalid) {
        return;
    }
    ReaderProfileSettings reader = active.settings;
    reader.show_sasayaki_toggle = show;
    if (state_->store->save_active_reader(reader) != StoreError::none) {
        return;
    }
    const bool stored = state_->store->active_reader().settings.show_sasayaki_toggle;
    if (stored != state_->show_sasayaki_toggle) {
        state_->show_sasayaki_toggle = stored;
        emit showSasayakiToggleChanged();
    }
}

void SasayakiSettingsViewModel::setAutoScroll(bool enabled) {
    if (state_->global.save_sasayaki_auto_scroll(enabled) != StoreError::none) {
        return;
    }
    const bool stored = state_->global.sasayaki_settings().auto_scroll;
    if (stored != state_->auto_scroll) {
        state_->auto_scroll = stored;
        emit autoScrollChanged();
    }
}

void SasayakiSettingsViewModel::setAutoPause(bool enabled) {
    if (state_->global.save_sasayaki_auto_pause(enabled) != StoreError::none) {
        return;
    }
    const bool stored = state_->global.sasayaki_settings().auto_pause;
    if (stored != state_->auto_pause) {
        state_->auto_pause = stored;
        emit autoPauseChanged();
    }
}

void SasayakiSettingsViewModel::setTextColor(const QString& color) {
    if (state_->global.save_sasayaki_text_color(color.toStdString()) != StoreError::none) {
        return;
    }
    const QString stored = QString::fromStdString(state_->global.sasayaki_settings().text_color);
    if (stored != state_->text_color) {
        state_->text_color = stored;
        emit textColorChanged();
    }
}

void SasayakiSettingsViewModel::setBackgroundColor(const QString& color) {
    if (state_->global.save_sasayaki_background_color(color.toStdString()) != StoreError::none) {
        return;
    }
    const QString stored =
        QString::fromStdString(state_->global.sasayaki_settings().background_color);
    if (stored != state_->background_color) {
        state_->background_color = stored;
        emit backgroundColorChanged();
    }
}

void SasayakiSettingsViewModel::setDarkTextColor(const QString& color) {
    if (state_->global.save_sasayaki_dark_text_color(color.toStdString()) != StoreError::none) {
        return;
    }
    const QString stored =
        QString::fromStdString(state_->global.sasayaki_settings().dark_text_color);
    if (stored != state_->dark_text_color) {
        state_->dark_text_color = stored;
        emit darkTextColorChanged();
    }
}

void SasayakiSettingsViewModel::setDarkBackgroundColor(const QString& color) {
    if (state_->global.save_sasayaki_dark_background_color(color.toStdString()) !=
        StoreError::none) {
        return;
    }
    const QString stored =
        QString::fromStdString(state_->global.sasayaki_settings().dark_background_color);
    if (stored != state_->dark_background_color) {
        state_->dark_background_color = stored;
        emit darkBackgroundColorChanged();
    }
}

const settings::SettingsStore* SasayakiSettingsViewModel::store() const {
    return state_->store.get();
}

}  // namespace niratan::viewmodels
