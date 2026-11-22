// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Settings tab of the Sasayaki panel, following
// Features/Sasayaki/SasayakiSheet.swift (settingsTab and its Playback and
// appearance rows) and Core/UserConfig.swift (readerShowSasayakiToggle, the
// sasayakiAutoScroll and sasayakiAutoPause booleans, the four sasayaki
// colours and UserConfig.init's defaults).
//
// The show toggle lives in the active profile's reader_settings.json through
// services/settings/settings_store.h, following D3: the other six keys live in
// the global settings document through services/settings/global_settings.h and
// keep module 3's defaults and colour normalization:
//   showSasayakiToggle  false
//   autoScroll          true
//   autoPause           true
//   textColor           #000000
//   backgroundColor     #87CFFA66
//   darkTextColor       #FFFFFF
//   darkBackgroundColor #87CFFA66
//
// Construction and reload() read both stores. Each setter writes only its own
// key and then exposes the stored value, so a colour reads back normalized; a
// save that fails writes nothing and leaves the exposed value unchanged. A
// reader settings document that does not decode is never replaced by defaults:
// its failed read counts as a failed save, like a damaged file.

#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace niratan::viewmodels {

class SasayakiSettingsViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
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

    bool show_sasayaki_toggle() const;
    bool auto_scroll() const;
    bool auto_pause() const;
    QString text_color() const;
    QString background_color() const;
    QString dark_text_color() const;
    QString dark_background_color() const;

    void setShowSasayakiToggle(bool value);
    void setAutoScroll(bool value);
    void setAutoPause(bool value);
    void setTextColor(const QString& value);
    void setBackgroundColor(const QString& value);
    void setDarkTextColor(const QString& value);
    void setDarkBackgroundColor(const QString& value);

    // Re-reads the active profile's reader settings and the global settings.
    Q_INVOKABLE void reload();

   signals:
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
