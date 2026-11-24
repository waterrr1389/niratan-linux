// SPDX-License-Identifier: GPL-3.0-or-later

#include "sync_settings_view_model.h"

#include "services/settings/global_settings.h"
#include "services/storage/app_paths.h"
#include "services/sync/ttu_convert.h"

#include <QPointer>
#include <QQmlEngine>

namespace niratan::viewmodels {
namespace {

constexpr auto google_drive = "https://www.googleapis.com";

auto data_file(const std::string& root, std::string_view name) -> decltype(storage::data_root()) {
    if (root.empty()) {
        return {};
    }
    return storage::resolve_within(root, name).value_or(decltype(storage::data_root()){});
}

}  // namespace

struct SyncSettingsViewModel::State {
    settings::GlobalSettings store;
    settings::SyncPreferences preferences;
    books::BookStorage books;
    sync::LoopbackAuthorization loopback;
    sync::GoogleDriveAuth auth;
    sync::DriveTransport transport;
    sync::TtuDrive drive;
    sync::TtuConverter converter;
    sync::TtuSync syncer;
    bool connecting = false;
    bool syncing = false;
    QString error;
    QString sync_error;

    explicit State(Config config)
        : store(data_file(config.data_root, "settings.json")),
          preferences(store.sync_preferences()), books(config.data_root),
          auth(data_file(config.data_root, "settings.json"),
               data_file(config.data_root, "sync/credentials.json"), loopback,
               std::move(config.auth)),
          transport(
              config.drive_base.isEmpty() ? QUrl(google_drive) : config.drive_base,
              [this] {
                  std::string error;
                  auto token = auth.access_token(&error);
                  return sync::DriveToken{token.value_or(""), std::move(error)};
              },
              [this](auto done) {
                  auth.refresh([done = std::move(done)](auto token, auto error) {
                      done({token.value_or(""), error ? error->description() : std::string{}});
                  });
              }),
          drive(transport), converter(books),
          syncer(drive, books,
                 {[this](const auto& book, const auto& out) {
                      return converter.export_zip(book, out);
                  },
                  [this](const auto& zip, const auto& dest) {
                      return converter.import_zip(zip, dest);
                  }},
                 store.statistics_preferences().preferences.reset_minutes,
                 config.zone.isValid() ? config.zone : QTimeZone::systemTimeZone()) {}

    void set_error(SyncSettingsViewModel& view, QString value) {
        if (error != value) {
            error = std::move(value);
            emit view.errorChanged();
        }
    }

    void set_sync_error(SyncSettingsViewModel& view, QString value) {
        if (sync_error != value) {
            sync_error = std::move(value);
            emit view.syncErrorChanged();
        }
    }

    template <class T, class Write>
    bool save(SyncSettingsViewModel& view, T& field, T value, Write write) {
        if (field == value) {
            return false;
        }
        if (write(value) != settings::StoreError::none) {
            set_error(view, view.tr("Could not save sync settings"));
            return false;
        }
        field = std::move(value);
        return true;
    }

    void connection_changed(SyncSettingsViewModel& view, bool was_authenticated,
                            bool had_credentials, const QString& old_status) {
        if (had_credentials != view.has_credentials()) {
            emit view.credentialsChanged();
        }
        if (was_authenticated != view.authenticated()) {
            emit view.authenticatedChanged();
        }
        if (old_status != view.connectionStatus()) {
            emit view.connectionStatusChanged();
        }
    }
};

SyncSettingsViewModel::SyncSettingsViewModel(QObject* parent)
    : SyncSettingsViewModel({storage::data_root().string(), {}, {}, {}}, parent) {}

SyncSettingsViewModel::SyncSettingsViewModel(Config config, QObject* parent)
    : QObject(parent), state_(std::make_unique<State>(std::move(config))) {}

SyncSettingsViewModel::~SyncSettingsViewModel() = default;

SyncSettingsViewModel*
SyncSettingsViewModel::instance(sync::LoopbackAuthorization::BrowserOpener opener) {
    static auto* model = [&] {
        Config config{storage::data_root().string(), {}, {}, {}};
        config.auth.browser_opener = std::move(opener);
        return new SyncSettingsViewModel(std::move(config));
    }();
    return model;
}

SyncSettingsViewModel* SyncSettingsViewModel::create(QQmlEngine*, QJSEngine*) {
    auto* model = instance();
    QJSEngine::setObjectOwnership(model, QJSEngine::CppOwnership);
    return model;
}

bool SyncSettingsViewModel::has_credentials() const {
    return state_->auth.access_token().has_value();
}

bool SyncSettingsViewModel::enabled() const {
    return state_->preferences.enabled;
}

QString SyncSettingsViewModel::syncMode() const {
    return QString::fromLatin1(settings::sync_mode_id(state_->preferences.mode));
}

bool SyncSettingsViewModel::autoSync() const {
    return state_->preferences.auto_sync;
}

QString SyncSettingsViewModel::clientId() const {
    return QString::fromStdString(state_->preferences.google_client_id);
}

QString SyncSettingsViewModel::connectionStatus() const {
    return connecting()   ? tr("Connecting…")
        : authenticated() ? tr("Connected")
                          : tr("Not connected");
}

bool SyncSettingsViewModel::connecting() const {
    return state_->connecting;
}

bool SyncSettingsViewModel::authenticated() const {
    return state_->auth.is_authenticated();
}

QString SyncSettingsViewModel::error() const {
    return state_->error;
}

bool SyncSettingsViewModel::uploadBooks() const {
    return state_->preferences.upload_books;
}

bool SyncSettingsViewModel::syncStatistics() const {
    return state_->preferences.statistics_sync;
}

bool SyncSettingsViewModel::syncAudioBook() const {
    return state_->preferences.sasayaki_sync;
}

bool SyncSettingsViewModel::statisticsEnabled() const {
    return state_->store.statistics_preferences().preferences.enabled;
}

bool SyncSettingsViewModel::sasayakiEnabled() const {
    return state_->store.sasayaki_settings().enable;
}

bool SyncSettingsViewModel::syncing() const {
    return state_->syncing;
}

QString SyncSettingsViewModel::syncError() const {
    return state_->sync_error;
}

void SyncSettingsViewModel::setEnabled(bool value) {
    if (state_->save(*this, state_->preferences.enabled, value,
                     [this](bool v) { return state_->store.save_sync_enabled(v); })) {
        emit enabledChanged();
    }
}

void SyncSettingsViewModel::setSyncMode(QString value) {
    const auto mode =
        value == "Manual" ? settings::SyncMode::manual : settings::SyncMode::automatic;
    if (state_->save(*this, state_->preferences.mode, mode,
                     [this](auto v) { return state_->store.save_sync_mode(v); })) {
        emit syncModeChanged();
    }
}

void SyncSettingsViewModel::setAutoSync(bool value) {
    if (state_->save(*this, state_->preferences.auto_sync, value,
                     [this](bool v) { return state_->store.save_auto_sync(v); })) {
        emit autoSyncChanged();
    }
}

void SyncSettingsViewModel::setClientId(QString value) {
    const auto was_authenticated = authenticated();
    const auto had_credentials = has_credentials();
    const auto old_status = connectionStatus();
    if (state_->save(*this, state_->preferences.google_client_id, value.toStdString(),
                     [this](const auto& v) { return state_->store.save_google_client_id(v); })) {
        emit clientIdChanged();
        state_->connection_changed(*this, was_authenticated, had_credentials, old_status);
    }
}

void SyncSettingsViewModel::setUploadBooks(bool value) {
    if (state_->save(*this, state_->preferences.upload_books, value,
                     [this](bool v) { return state_->store.save_sync_upload_books(v); })) {
        emit uploadBooksChanged();
    }
}

void SyncSettingsViewModel::setSyncStatistics(bool value) {
    if (state_->save(*this, state_->preferences.statistics_sync, value,
                     [this](bool v) { return state_->store.save_statistics_enable_sync(v); })) {
        emit syncStatisticsChanged();
    }
}

void SyncSettingsViewModel::setSyncAudioBook(bool value) {
    if (state_->save(*this, state_->preferences.sasayaki_sync, value,
                     [this](bool v) { return state_->store.save_sasayaki_enable_sync(v); })) {
        emit syncAudioBookChanged();
    }
}

void SyncSettingsViewModel::signIn() {
    if (connecting()) {
        const sync::AuthError failure{sync::AuthErrorKind::already_in_progress};
        state_->set_error(*this, QString::fromStdString(failure.description()));
        return;
    }
    clearError();
    const auto was_authenticated = authenticated();
    const auto had_credentials = has_credentials();
    state_->connecting = true;
    emit connectingChanged();
    emit connectionStatusChanged();
    state_->auth.authenticate(
        [this, guard = QPointer(this), was_authenticated, had_credentials](auto failure) {
            if (!guard) {
                return;
            }
            const auto old_status = connectionStatus();
            state_->connecting = false;
            emit connectingChanged();
            if (!failure) {
                state_->drive.clear_cache();
            }
            state_->connection_changed(*this, was_authenticated, had_credentials, old_status);
            if (failure && failure->kind != sync::AuthErrorKind::cancelled) {
                state_->set_error(*this, QString::fromStdString(failure->description()));
            }
        });
}

void SyncSettingsViewModel::cancelSignIn() {
    state_->auth.cancel_authentication();
}

void SyncSettingsViewModel::signOut() {
    const auto was_authenticated = authenticated();
    const auto had_credentials = has_credentials();
    const auto old_status = connectionStatus();
    const auto was_connecting = connecting();
    clearCache();
    state_->auth.sign_out();
    clearError();
    if (!was_connecting) {
        state_->connection_changed(*this, was_authenticated, had_credentials, old_status);
    }
}

void SyncSettingsViewModel::clearCache() {
    state_->drive.clear_cache();
}

void SyncSettingsViewModel::clearError() {
    state_->set_error(*this, {});
}

void SyncSettingsViewModel::clearSyncError() {
    state_->set_sync_error(*this, {});
}

void SyncSettingsViewModel::syncBook(QString folder) {
    if (syncing()) {
        return;
    }
    clearSyncError();
    if (!authenticated()) {
        state_->set_sync_error(*this, tr("Not authenticated\nPlease sign in"));
        return;
    }
    auto book = state_->books.load_metadata(folder.toStdString());
    if (!book) {
        state_->set_sync_error(*this, tr("Book not found"));
        return;
    }
    state_->syncing = true;
    emit syncingChanged();
    state_->syncer.sync_book(std::move(*book), std::nullopt, ttu_options(false),
                             [this, guard = QPointer(this)](auto result, auto failure) {
                                 if (!guard) {
                                     return;
                                 }
                                 state_->syncing = false;
                                 emit syncingChanged();
                                 if (result) {
                                     emit syncFinished(int(result->kind),
                                                       QString::fromStdString(result->title),
                                                       result->character_count);
                                 } else {
                                     const auto message = QString::fromStdString(
                                         failure.value_or(sync::GoogleDriveError{}).description());
                                     state_->set_sync_error(*this, message);
                                     emit syncFailed(message);
                                 }
                             });
}

sync::TtuSyncOptions SyncSettingsViewModel::ttu_options(bool import_only) const {
    return {enabled() && uploadBooks(), enabled() && syncStatistics(),
            state_->preferences.statistics_mode, sasayakiEnabled() && syncAudioBook(), import_only};
}

bool SyncSettingsViewModel::auto_sync_enabled() const {
    return enabled() && autoSync();
}

}  // namespace niratan::viewmodels
