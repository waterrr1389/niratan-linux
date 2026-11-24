// SPDX-License-Identifier: GPL-3.0-or-later
// SyncView.swift, NativeReaderView.configure and BookshelfViewModel.syncBook.

#include "fakes/oauth_fixture.h"
#include "fakes/ttu_fixture.h"
#include "services/settings/global_settings.h"
#include "services/sync/oauth_credentials.h"
#include "viewmodels/sync/sync_settings_view_model.h"

#include <QJsonDocument>
#include <QtTest>

#include <algorithm>

using namespace niratan;
using viewmodels::SyncSettingsViewModel;

namespace {

SyncSettingsViewModel::Config config(oauth_fixture::AuthFixture& fixture) {
    return {fixture.settings().parent_path().string(), fixture.config(),
            fixture.endpoint.base_url(), QTimeZone::utc()};
}

bool authorize(oauth_fixture::AuthFixture& fixture) {
    return sync::CredentialsStore(fixture.credentials())
        .save({"access", "refresh", oauth_fixture::client_id, {}});
}

bool authorize_library(oauth_fixture::AuthFixture& fixture, const ttu_fixture::Library& library,
                       SyncSettingsViewModel::Config& options) {
    const auto root = library.storage.app_directory();
    options.data_root = root.string();
    return oauth_fixture::write(root / "settings.json", oauth_fixture::read(fixture.settings())) &&
        sync::CredentialsStore(root / "sync/credentials.json")
            .save({"access", "refresh", oauth_fixture::client_id, {}});
}

}  // namespace

class SyncSettingsViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void preferences_and_options() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(oauth_fixture::write(
            fixture.settings(),
            R"({"enableStatistics":true,"enableSasayaki":true,"syncProvider":"gdrive","statisticsSyncMode":"Replace"})"));
        SyncSettingsViewModel model(config(fixture));
        QSignalSpy mode(&model, &SyncSettingsViewModel::syncModeChanged);
        model.setSyncMode("Manual");
        QCOMPARE(model.syncMode(), QStringLiteral("Manual"));
        QCOMPARE(mode.size(), 1);
        model.setAutoSync(true);
        QVERIFY(!model.auto_sync_enabled());
        model.setEnabled(true);
        QVERIFY(model.auto_sync_enabled());
        QVERIFY(model.statisticsEnabled() && model.sasayakiEnabled());
        model.setSyncStatistics(true);
        model.setSyncAudioBook(true);
        const auto options = model.ttu_options(true);
        QVERIFY(options.sync_book_data && options.sync_stats && options.sync_audio_book &&
                options.import_only);
        QCOMPARE(options.stats_mode, statistics::StatisticsSyncMode::replace);
        model.setUploadBooks(false);
        QVERIFY(!model.ttu_options(false).sync_book_data);
        model.setEnabled(false);
        QVERIFY(!model.ttu_options(false).sync_stats);
        QVERIFY(model.ttu_options(false).sync_audio_book);
        SyncSettingsViewModel reopened(config(fixture));
        QCOMPARE(reopened.syncMode(), QStringLiteral("Manual"));
        QVERIFY(reopened.autoSync() && reopened.syncStatistics() && reopened.syncAudioBook());
        QVERIFY(!reopened.enabled() && !reopened.uploadBooks());
    }

    void write_failure_keeps_property() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        SyncSettingsViewModel model(config(fixture));
        QVERIFY(oauth_fixture::write(fixture.settings(), "{damaged"));
        QSignalSpy changed(&model, &SyncSettingsViewModel::enabledChanged);
        model.setEnabled(true);
        QVERIFY(!model.enabled());
        QVERIFY(!model.error().isEmpty());
        QCOMPARE(changed.size(), 0);
        QCOMPARE(oauth_fixture::read(fixture.settings()), "{damaged");
    }

    void authentication_tracks_client_id() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(authorize(fixture));
        SyncSettingsViewModel model(config(fixture));
        QVERIFY(model.authenticated());
        QCOMPARE(model.connectionStatus(), QStringLiteral("Connected"));
        QSignalSpy changed(&model, &SyncSettingsViewModel::authenticatedChanged);
        model.setClientId("456-other.apps.googleusercontent.com");
        QVERIFY(!model.authenticated());
        QCOMPARE(model.connectionStatus(), QStringLiteral("Not connected"));
        QCOMPARE(changed.size(), 1);
        model.setClientId(" invalid ");
        model.signIn();
        QCOMPARE(model.error(), QStringLiteral("Invalid Client ID format"));
        QCOMPARE(fixture.browser_calls, 0);
        QVERIFY(fixture.endpoint.requests().empty());
    }

    void sign_in_cancel_exchange_and_sign_out() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        QVERIFY(authorize(fixture));
        SyncSettingsViewModel model(config(fixture));
        model.signIn();
        QTRY_COMPARE(fixture.browser_calls, 1);
        QVERIFY(model.connecting());
        QCOMPARE(model.connectionStatus(), QStringLiteral("Connecting…"));
        const auto uri = oauth_fixture::redirect(fixture.authorization);
        model.signIn();
        QCOMPARE(fixture.browser_calls, 1);
        const sync::AuthError in_progress{sync::AuthErrorKind::already_in_progress};
        QCOMPARE(model.error(), QString::fromStdString(in_progress.description()));
        model.cancelSignIn();
        QTRY_VERIFY(!model.connecting());
        QVERIFY(oauth_fixture::port_closed(uri));
        QVERIFY(model.authenticated());
        QCOMPARE(model.connectionStatus(), QStringLiteral("Connected"));
        model.signOut();
        QVERIFY(!model.authenticated());
        QVERIFY(!std::filesystem::exists(fixture.credentials()));
        QVERIFY(model.error().isEmpty());
        model.signIn();
        QTRY_COMPARE(fixture.browser_calls, 2);
        fixture.endpoint.enqueue({200, oauth_fixture::exchange_json});
        const auto auth = fixture.authorization;
        oauth_fixture::callback(oauth_fixture::redirect(auth),
                                "/?code=fixture&state=" + oauth_fixture::state(auth));
        QTRY_VERIFY(!model.connecting());
        QVERIFY(model.authenticated());
        QCOMPARE(model.connectionStatus(), QStringLiteral("Connected"));
    }

    void manual_sync_without_authentication() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        ttu_fixture::Library library;
        QVERIFY(library.start());
        auto options = config(fixture);
        options.data_root = library.storage.app_directory().string();
        SyncSettingsViewModel model(std::move(options));
        model.syncBook(QString::fromStdString(library.book.folder));
        QCOMPARE(model.syncError(), QStringLiteral("Not authenticated\nPlease sign in"));
        QVERIFY(fixture.endpoint.requests().empty());
        model.clearSyncError();
        QVERIFY(model.syncError().isEmpty());
    }

    void sign_in_clears_folder_caches() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        ttu_fixture::Library library;
        QVERIFY(library.start());
        auto options = config(fixture);
        QVERIFY(authorize_library(fixture, library, options));
        fixture.endpoint.enable_tree();
        ttu_fixture::seed_folders(fixture.endpoint);
        SyncSettingsViewModel model(std::move(options));
        QSignalSpy finished(&model, &SyncSettingsViewModel::syncFinished);
        const auto folder = QString::fromStdString(library.book.folder);
        model.syncBook(folder);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(fixture.endpoint.requests().size(), 3);
        model.signIn();
        QTRY_COMPARE(fixture.browser_calls, 1);
        fixture.endpoint.enqueue({200, oauth_fixture::exchange_json});
        const auto auth = fixture.authorization;
        oauth_fixture::callback(oauth_fixture::redirect(auth),
                                "/?code=fixture&state=" + oauth_fixture::state(auth));
        QTRY_VERIFY(!model.connecting());
        const auto before = fixture.endpoint.requests().size();
        model.syncBook(folder);
        QTRY_COMPARE(finished.size(), 2);
        QCOMPARE(fixture.endpoint.requests().size() - before, 3);
    }

    void sign_out_clears_folder_caches() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        ttu_fixture::Library library;
        QVERIFY(library.start());
        auto options = config(fixture);
        QVERIFY(authorize_library(fixture, library, options));
        fixture.endpoint.enable_tree();
        ttu_fixture::seed_folders(fixture.endpoint);
        SyncSettingsViewModel model(std::move(options));
        QSignalSpy finished(&model, &SyncSettingsViewModel::syncFinished);
        const auto folder = QString::fromStdString(library.book.folder);
        model.syncBook(folder);
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(fixture.endpoint.requests().size(), 3);
        model.signOut();
        QVERIFY(!model.authenticated());
        QVERIFY(sync::CredentialsStore(library.storage.app_directory() / "sync/credentials.json")
                    .save({"access", "refresh", oauth_fixture::client_id, {}}));
        const auto before = fixture.endpoint.requests().size();
        model.syncBook(folder);
        QTRY_COMPARE(finished.size(), 2);
        QCOMPARE(fixture.endpoint.requests().size() - before, 3);
    }

    void manual_sync_unknown_folder() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        ttu_fixture::Library library;
        QVERIFY(library.start());
        auto options = config(fixture);
        QVERIFY(authorize_library(fixture, library, options));
        SyncSettingsViewModel model(std::move(options));
        model.syncBook(QStringLiteral("missing"));
        QCOMPARE(model.syncError(), QStringLiteral("Book not found"));
        QVERIFY(!model.syncing());
        QVERIFY(fixture.endpoint.requests().empty());
    }

    void manual_sync_result_kinds_data() {
        QTest::addColumn<bool>("exporting");
        QTest::newRow("import") << false;
        QTest::newRow("export") << true;
    }

    void manual_sync_result_kinds() {
        QFETCH(bool, exporting);
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        ttu_fixture::Library library;
        QVERIFY(library.start());
        if (exporting) {
            QVERIFY(library.bookmark(600, ttu_fixture::remote_ms + 2000));
        }
        auto options = config(fixture);
        QVERIFY(authorize_library(fixture, library, options));
        fixture.endpoint.enable_tree();
        ttu_fixture::seed_folders(fixture.endpoint);
        ttu_fixture::seed_media(fixture.endpoint);
        SyncSettingsViewModel model(std::move(options));
        QSignalSpy finished(&model, &SyncSettingsViewModel::syncFinished);
        model.syncBook(QString::fromStdString(library.book.folder));
        QTRY_COMPARE(finished.size(), 1);
        using Kind = sync::TtuResult::Kind;
        QCOMPARE(finished.front(),
                 (QVariantList{int(exporting ? Kind::exported : Kind::imported),
                               QStringLiteral("Display Title"), exporting ? 600 : 250}));
        if (!exporting) {
            QCOMPARE(library.storage.load_bookmark(library.book.folder),
                     (std::optional{books::Bookmark{1, 0.0625, 250,
                                                    ttu_fixture::remote_ms / 1000.0 -
                                                        ttu_fixture::unix_reference}}));
        } else {
            const auto& requests = fixture.endpoint.requests();
            const auto patch = std::ranges::find_if(requests, [](const auto& request) {
                return request.method == "PATCH" && request.path == "/upload/drive/v3/files/p";
            });
            QVERIFY(patch != requests.end());
            const auto media = QJsonDocument::fromJson(patch->media).object();
            QCOMPARE(media.value("lastBookmarkModified").toInteger(), 1690000002000LL);
            QCOMPARE(media.value("exploredCharCount").toInt(), 600);
        }
    }

    void manual_sync_uses_shared_caches_and_signals() {
        oauth_fixture::AuthFixture fixture;
        QVERIFY(fixture.start());
        ttu_fixture::Library library;
        QVERIFY(library.start());
        auto options = config(fixture);
        const auto root = library.storage.app_directory();
        options.data_root = root.string();
        QVERIFY(
            oauth_fixture::write(root / "settings.json", oauth_fixture::read(fixture.settings())));
        QVERIFY(sync::CredentialsStore(root / "sync/credentials.json")
                    .save({"access", "refresh", oauth_fixture::client_id, {}}));
        fixture.endpoint.enable_tree();
        ttu_fixture::seed_folders(fixture.endpoint);
        SyncSettingsViewModel model(std::move(options));
        QSignalSpy finished(&model, &SyncSettingsViewModel::syncFinished);
        QSignalSpy failed(&model, &SyncSettingsViewModel::syncFailed);
        const auto folder = QString::fromStdString(library.book.folder);
        model.syncBook(folder);
        QVERIFY(model.syncing());
        model.syncBook(folder);
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(!model.syncing());
        QCOMPARE(
            finished.front(),
            (QVariantList{int(sync::TtuResult::Kind::synced), QStringLiteral("Display Title"), 0}));
        QCOMPARE(fixture.endpoint.requests().size(), 3);
        model.syncBook(folder);
        QTRY_COMPARE(finished.size(), 2);
        QCOMPARE(fixture.endpoint.requests().size(), 4);
        model.clearCache();
        QVERIFY(model.authenticated());
        model.syncBook(folder);
        QTRY_COMPARE(finished.size(), 3);
        QCOMPARE(fixture.endpoint.requests().size(), 7);
        fixture.endpoint.enqueue({500, {}});
        model.syncBook(folder);
        QTRY_COMPARE(failed.size(), 1);
        QVERIFY(!model.syncing());
        QVERIFY(!model.syncError().isEmpty());
        QCOMPARE(failed.front().front().toString(), model.syncError());
    }
};

QTEST_GUILESS_MAIN(SyncSettingsViewModelTest)
#include "tst_sync_settings_view_model.moc"
