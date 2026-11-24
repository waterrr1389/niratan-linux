// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Sasayaki settings model, following Features/Sasayaki/SasayakiSheet.swift
// (settingsTab and its Playback and appearance rows),
// Features/Settings/SasayakiSettingsView.swift (the Enable, Settings, Command
// Center, Light Theme and Dark Theme cards) and Core/UserConfig.swift
// (enableSasayaki, readerShowSasayakiToggle, sasayakiAutoScroll,
// sasayakiAutoPause, sasayakiSkipControls, the four sasayaki colours and
// UserConfig.init's defaults).
//
// The show toggle lives in the active profile's reader_settings.json through
// services/settings/settings_store.h, following D3: the other eight keys live
// in the global settings document through services/settings/global_settings.h
// and keep module 3's defaults and colour normalization:
//   enable              false
//   skipControls        false
//   autoScroll          true
//   autoPause           true
//   textColor           #000000
//   backgroundColor     #87CFFA66
//   darkTextColor       #FFFFFF
//   darkBackgroundColor #87CFFA66
//
// The reader settings go through SettingsStore::shared(), the store the
// Settings model also uses; the toggle writes only showSasayakiToggle over the
// active profile's current reader_settings.json. The model re-reads when the
// store refreshes after a possible profile switch.
//
// Construction and reload() read both stores. Each setter writes only its own
// key and then exposes the stored value, so a colour reads back normalized; a
// save that fails writes nothing and leaves the exposed value unchanged. A
// reader settings document that does not decode is never replaced by defaults:
// its failed read counts as a failed save, like a damaged file.
//
// UserConfig.init's `?? false` means a global key that is not a JSON boolean
// reads as false, which is also each key's default.

#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::settings {
class SettingsStore;
}

namespace niratan::viewmodels {

class SasayakiSettingsViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool enable READ enable WRITE setEnable NOTIFY enableChanged)
    Q_PROPERTY(
        bool skipControls READ skip_controls WRITE setSkipControls NOTIFY skipControlsChanged)
    Q_PROPERTY(bool showSasayakiToggle READ show_sasayaki_toggle WRITE setShowSasayakiToggle NOTIFY
                   showSasayakiToggleChanged)
    Q_PROPERTY(bool autoScroll READ auto_scroll WRITE setAutoScroll NOTIFY autoScrollChanged)
    Q_PROPERTY(bool autoPause READ auto_pause WRITE setAutoPause NOTIFY autoPauseChanged)
    Q_PROPERTY(QString textColor READ text_color WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(QString backgroundColor READ background_color WRITE setBackgroundColor NOTIFY
                   backgroundColorChanged)
    Q_PROPERTY(QString darkTextColor READ dark_text_color WRITE setDarkTextColor NOTIFY
                   darkTextColorChanged)
    Q_PROPERTY(QString darkBackgroundColor READ dark_background_color WRITE setDarkBackgroundColor
                   NOTIFY darkBackgroundColorChanged)

   public:
    explicit SasayakiSettingsViewModel(QObject* parent = nullptr);
    ~SasayakiSettingsViewModel() override;

    bool enable() const;
    bool skip_controls() const;
    bool show_sasayaki_toggle() const;
    bool auto_scroll() const;
    bool auto_pause() const;
    QString text_color() const;
    QString background_color() const;
    QString dark_text_color() const;
    QString dark_background_color() const;

    void setEnable(bool value);
    void setSkipControls(bool value);
    void setShowSasayakiToggle(bool value);
    void setAutoScroll(bool value);
    void setAutoPause(bool value);
    void setTextColor(const QString& value);
    void setBackgroundColor(const QString& value);
    void setDarkTextColor(const QString& value);
    void setDarkBackgroundColor(const QString& value);

    // Re-reads the active profile's reader settings and the global settings.
    Q_INVOKABLE void reload();

    // The store the toggle reads and writes; null when the data root is
    // unusable.
    const settings::SettingsStore* store() const;

   signals:
    void enableChanged();
    void skipControlsChanged();
    void showSasayakiToggleChanged();
    void autoScrollChanged();
    void autoPauseChanged();
    void textColorChanged();
    void backgroundColorChanged();
    void darkTextColorChanged();
    void darkBackgroundColorChanged();

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels
