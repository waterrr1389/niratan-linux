// SPDX-License-Identifier: GPL-3.0-or-later
// SyncView.swift connection actions; NativeReaderView.configure and
// BookshelfViewModel.syncBook. Only TTU is effective in this build.
// Failed GlobalSettings writes keep the old value and set error.
// The model owns auth and one shared Drive/sync stack; the app injects
// the browser opener. Manual sync uses importOnly=false; reader open uses true.

#pragma once

#include "services/sync/oauth_client.h"
#include "services/sync/ttu_sync.h"

#include <QtQmlIntegration/qqmlintegration.h>

class QQmlEngine;
class QJSEngine;

namespace niratan::viewmodels {

class SyncSettingsViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool hasCredentials READ has_credentials NOTIFY credentialsChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString syncMode READ syncMode WRITE setSyncMode NOTIFY syncModeChanged)
    Q_PROPERTY(bool autoSync READ autoSync WRITE setAutoSync NOTIFY autoSyncChanged)
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)
    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool uploadBooks READ uploadBooks WRITE setUploadBooks NOTIFY uploadBooksChanged)
    Q_PROPERTY(bool syncStatistics READ syncStatistics WRITE setSyncStatistics NOTIFY
                   syncStatisticsChanged)
    Q_PROPERTY(
        bool syncAudioBook READ syncAudioBook WRITE setSyncAudioBook NOTIFY syncAudioBookChanged)
    Q_PROPERTY(bool statisticsEnabled READ statisticsEnabled NOTIFY statisticsEnabledChanged)
    Q_PROPERTY(bool sasayakiEnabled READ sasayakiEnabled NOTIFY sasayakiEnabledChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(QString syncError READ syncError NOTIFY syncErrorChanged)

   public:
    struct Config {
        std::string data_root;
        sync::GoogleDriveAuth::Config auth;
        QUrl drive_base;
        QTimeZone zone;
    };

    explicit SyncSettingsViewModel(QObject* parent = nullptr);
    explicit SyncSettingsViewModel(Config config, QObject* parent = nullptr);
    ~SyncSettingsViewModel() override;
    static SyncSettingsViewModel* instance(sync::LoopbackAuthorization::BrowserOpener opener = {});
    static SyncSettingsViewModel* create(QQmlEngine*, QJSEngine*);
    bool has_credentials() const;
    bool enabled() const;
    QString syncMode() const;
    bool autoSync() const;
    QString clientId() const;
    QString connectionStatus() const;
    bool connecting() const;
    bool authenticated() const;
    QString error() const;
    bool uploadBooks() const;
    bool syncStatistics() const;
    bool syncAudioBook() const;
    bool statisticsEnabled() const;
    bool sasayakiEnabled() const;
    bool syncing() const;
    QString syncError() const;

    void setEnabled(bool value);
    void setSyncMode(QString value);
    void setAutoSync(bool value);
    void setClientId(QString value);
    void setUploadBooks(bool value);
    void setSyncStatistics(bool value);
    void setSyncAudioBook(bool value);

    Q_INVOKABLE void signIn();
    Q_INVOKABLE void cancelSignIn();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void clearSyncError();
    Q_INVOKABLE void syncBook(QString folder);

    sync::TtuSyncOptions ttu_options(bool import_only) const;
    bool auto_sync_enabled() const;

   signals:
    void credentialsChanged();
    void enabledChanged();
    void syncModeChanged();
    void autoSyncChanged();
    void clientIdChanged();
    void connectionStatusChanged();
    void connectingChanged();
    void authenticatedChanged();
    void errorChanged();
    void uploadBooksChanged();
    void syncStatisticsChanged();
    void syncAudioBookChanged();
    void statisticsEnabledChanged();
    void sasayakiEnabledChanged();
    void syncingChanged();
    void syncErrorChanged();
    void syncFinished(int kind, QString title, int characterCount);
    void syncFailed(QString message);

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::viewmodels
