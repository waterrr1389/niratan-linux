// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for DictionarySettingsWatch: the active profile's dictionary
// settings as Core/ProfileSettingsStore.swift loads them and
// Core/UserConfig.swift applies them (apply(dictionaryProfileSettings:)),
// with the content language of ContentLanguageProfile in
// Models/Profile.swift.

#include "services/settings/dictionary_settings_watch.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <fstream>
#include <string>

#include "models/profiles/content_language.h"
#include "models/profiles/dictionary_settings.h"
#include "services/profiles/profile_repository.h"

using niratan::profiles::ContentLanguage;
using niratan::profiles::default_dictionary_settings;
using niratan::profiles::dictionary_settings_to_json;
using niratan::profiles::DictionaryProfileSettings;
using niratan::profiles::language_id;
using niratan::profiles::ProfileRepository;
using niratan::settings::DictionarySettingsWatch;

namespace {

std::filesystem::path root_of(const QTemporaryDir& dir) {
    return std::filesystem::path(dir.path().toStdString());
}

std::filesystem::path settings_file(const QTemporaryDir& dir, const std::string& profile) {
    return root_of(dir) / "Profiles" / profile / "dictionary_settings.json";
}

std::filesystem::path config_file(const QTemporaryDir& dir, const std::string& profile) {
    return root_of(dir) / "Profiles" / profile / "dictionary_config.json";
}

std::filesystem::path collapsed_file(const QTemporaryDir& dir, const std::string& profile) {
    return root_of(dir) / "Profiles" / profile / "collapsed.json";
}

const char* const kProbeConfig =
    R"({"termDictionaries":[{"fileName":"Probe","isEnabled":true,"order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})";

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
}

void write_settings(const QTemporaryDir& dir, const std::string& profile, int max_results) {
    DictionaryProfileSettings settings = default_dictionary_settings();
    settings.max_results = max_results;
    write_text(settings_file(dir, profile),
               dictionary_settings_to_json(settings).toJson().toStdString());
}

}  // namespace

class DictionarySettingsWatchTest : public QObject {
    Q_OBJECT

   private slots:
    void noFiles_givesDefaultsForJapanese() {
        QTemporaryDir dir;
        DictionarySettingsWatch watch(root_of(dir));
        watch.reload();
        QCOMPARE(watch.language_id(), std::string_view("ja"));
        QCOMPARE(watch.settings(), default_dictionary_settings());
        QVERIFY(!std::filesystem::exists(root_of(dir) / "Profiles"));
    }

    void storedSettings_areRead() {
        QTemporaryDir dir;
        write_settings(dir, "default-ja", 7);
        DictionarySettingsWatch watch(root_of(dir));
        QVERIFY(watch.reload());
        QCOMPARE(watch.settings().max_results, 7);
        QVERIFY(!watch.reload());
    }

    void invalidInitialFile_givesDefaults() {
        QTemporaryDir dir;
        write_text(settings_file(dir, "default-ja"), "{ damaged");
        DictionarySettingsWatch watch(root_of(dir));
        watch.reload();
        QCOMPARE(watch.settings(), default_dictionary_settings());
    }

    void invalidLiveEdit_keepsLastGoodSettings() {
        QTemporaryDir dir;
        write_settings(dir, "default-ja", 7);
        DictionarySettingsWatch watch(root_of(dir));
        watch.reload();
        write_text(settings_file(dir, "default-ja"), "{ damaged");
        QVERIFY(!watch.reload());
        QCOMPARE(watch.settings().max_results, 7);
    }

    void removedFile_givesDefaults() {
        QTemporaryDir dir;
        write_settings(dir, "default-ja", 7);
        DictionarySettingsWatch watch(root_of(dir));
        watch.reload();
        std::filesystem::remove(settings_file(dir, "default-ja"));
        QVERIFY(watch.reload());
        QCOMPARE(watch.settings(), default_dictionary_settings());
    }

    void activeProfile_suppliesLanguageAndSettings() {
        QTemporaryDir dir;
        std::string id;
        {
            ProfileRepository repository(root_of(dir));
            const auto created =
                repository.create_profile("English", ContentLanguage::english, std::nullopt);
            QVERIFY(created.profile.has_value());
            id = created.profile->id;
            QVERIFY(!repository.set_active_profile(id).has_value());
        }
        write_settings(dir, id, 9);
        DictionarySettingsWatch watch(root_of(dir));
        QVERIFY(watch.reload());
        QCOMPARE(watch.language_id(), language_id(ContentLanguage::english));
        QCOMPARE(watch.settings().max_results, 9);
    }

    void editInAnotherWindow_emitsChanged() {
        QTemporaryDir dir;
        write_settings(dir, "default-ja", 7);
        DictionarySettingsWatch watch(root_of(dir));
        watch.reload();
        QSignalSpy spy(&watch, &DictionarySettingsWatch::changed);
        write_settings(dir, "default-ja", 11);
        QTRY_VERIFY(spy.count() > 0);
        QCOMPARE(watch.settings().max_results, 11);
    }

    void fileCreatedLater_emitsChanged() {
        QTemporaryDir dir;
        std::filesystem::create_directories(root_of(dir) / "Profiles");
        DictionarySettingsWatch watch(root_of(dir));
        watch.reload();
        QSignalSpy spy(&watch, &DictionarySettingsWatch::changed);
        write_settings(dir, "default-ja", 5);
        QTRY_VERIFY(spy.count() > 0);
        QCOMPARE(watch.settings().max_results, 5);
    }

    void emptyDataRoot_readsNothing() {
        DictionarySettingsWatch watch{std::filesystem::path{}};
        QVERIFY(!watch.reload());
        QCOMPARE(watch.settings(), default_dictionary_settings());
    }

    // B5: Core/DictionaryManager.swift loadDictionaries and
    // loadCollapsedDictionaries read the profile's two dictionary files, so a
    // change to either reaches the open popup.
    void configWrite_emitsChanged() {
        QTemporaryDir dir;
        write_settings(dir, "default-ja", 7);
        DictionarySettingsWatch watch(root_of(dir));
        watch.reload();
        QSignalSpy spy(&watch, &DictionarySettingsWatch::changed);
        write_text(config_file(dir, "default-ja"), kProbeConfig);
        QTRY_VERIFY(spy.count() > 0);
    }

    void collapsedWrite_emitsChanged() {
        QTemporaryDir dir;
        write_settings(dir, "default-ja", 7);
        DictionarySettingsWatch watch(root_of(dir));
        watch.reload();
        QSignalSpy spy(&watch, &DictionarySettingsWatch::changed);
        write_text(collapsed_file(dir, "default-ja"), R"(["Probe"])");
        QTRY_VERIFY(spy.count() > 0);
    }

    void identicalDictionaryRewrite_doesNotEmit() {
        QTemporaryDir dir;
        write_settings(dir, "default-ja", 7);
        write_text(config_file(dir, "default-ja"), kProbeConfig);
        write_text(collapsed_file(dir, "default-ja"), "[]");
        DictionarySettingsWatch watch(root_of(dir));
        watch.reload();
        QSignalSpy spy(&watch, &DictionarySettingsWatch::changed);
        write_text(config_file(dir, "default-ja"), kProbeConfig);
        write_text(collapsed_file(dir, "default-ja"), "[]");
        QTest::qWait(200);
        QCOMPARE(spy.count(), 0);

        // The same watcher reports the next real change, so the silence above
        // came from the identical bytes rather than a dead watch.
        write_text(collapsed_file(dir, "default-ja"), R"(["Probe"])");
        QTRY_VERIFY(spy.count() > 0);
    }
};

QTEST_GUILESS_MAIN(DictionarySettingsWatchTest)
#include "tst_dictionary_settings_watch.moc"
