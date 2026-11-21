// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for SettingsStore, following Core/ProfileSettingsStore.swift
// (bootstrap, activate, persistCurrent, dictionarySettings(for:fallback:)).

#include "services/settings/settings_store.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <fstream>
#include <string>

#include "models/profiles/dictionary_settings.h"
#include "models/profiles/reader_settings.h"
#include "services/storage/json_file.h"

using niratan::profiles::default_dictionary_settings;
using niratan::profiles::default_reader_settings;
using niratan::settings::SettingsStore;
using niratan::settings::StoreError;
using ReaderProfileSettings = niratan::profiles::ReaderProfileSettings;
using DictionaryProfileSettings = niratan::profiles::DictionaryProfileSettings;

namespace {

std::filesystem::path make_root(const QTemporaryDir& dir) {
    return std::filesystem::path(dir.path().toStdString());
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << text;
}

}  // namespace

class SettingsStoreTest : public QObject {
    Q_OBJECT

   private slots:
    // --- profile settings load ---

    void loadReader_missingFile_returnsNullopt() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsStore store(make_root(dir));

        const auto result = store.load_reader("profile-abc");
        QVERIFY(!result.has_value());
    }

    void loadReader_validFile_returnsSettings() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto root = make_root(dir);
        const auto pdir = root / "Profiles" / "profile-abc";

        // Write a minimal valid reader_settings.json.
        ReaderProfileSettings s = default_reader_settings();
        s.font_size = 28;
        const bool ok =
            niratan::storage::write_json_file(pdir / "reader_settings.json",
                                               niratan::profiles::reader_settings_to_json(s));
        QVERIFY(ok);

        SettingsStore store(root);
        const auto result = store.load_reader("profile-abc");
        QVERIFY(result.has_value());
        QCOMPARE(result->font_size, 28);
    }

    void loadReader_unsafeProfileId_returnsNullopt() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsStore store(make_root(dir));

        const auto result = store.load_reader("../escape");
        QVERIFY(!result.has_value());
    }

    // --- profile settings save ---

    void saveReader_writesAndRoundTrips() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto root = make_root(dir);
        std::filesystem::create_directories(root / "Profiles" / "profile-abc");
        SettingsStore store(root);

        ReaderProfileSettings s = default_reader_settings();
        s.vertical_writing = false;
        const auto err = store.save_reader("profile-abc", s);
        QCOMPARE(err, StoreError::none);

        const auto loaded = store.load_reader("profile-abc");
        QVERIFY(loaded.has_value());
        QVERIFY(!loaded->vertical_writing);
    }

    void saveReader_unsafeProfileId_returnsError() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsStore store(make_root(dir));

        const auto err = store.save_reader("../escape", default_reader_settings());
        QCOMPARE(err, StoreError::unsafe_profile_id);
    }

    void saveDictionary_writesAndRoundTrips() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto root = make_root(dir);
        std::filesystem::create_directories(root / "Profiles" / "profile-abc");
        SettingsStore store(root);

        DictionaryProfileSettings s = default_dictionary_settings();
        s.max_results = 8;
        const auto err = store.save_dictionary("profile-abc", s);
        QCOMPARE(err, StoreError::none);

        const auto loaded = store.load_dictionary("profile-abc");
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->max_results, 8);
    }

    // --- dictionary_settings_for ---

    void dictionarySettingsFor_missingFile_returnsFallback() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsStore store(make_root(dir));

        DictionaryProfileSettings fallback = default_dictionary_settings();
        fallback.max_results = 5;

        const auto result = store.dictionary_settings_for("profile-other", fallback);
        QCOMPARE(result.max_results, 5);
    }

    void dictionarySettingsFor_existingFile_returnsStoredSettings() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto root = make_root(dir);
        std::filesystem::create_directories(root / "Profiles" / "profile-other");
        SettingsStore store(root);

        DictionaryProfileSettings s = default_dictionary_settings();
        s.max_results = 12;
        store.save_dictionary("profile-other", s);

        const auto fallback = default_dictionary_settings();
        const auto result = store.dictionary_settings_for("profile-other", fallback);
        QCOMPARE(result.max_results, 12);
    }

    void dictionarySettingsFor_unsafeId_returnsFallback() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsStore store(make_root(dir));

        DictionaryProfileSettings fallback = default_dictionary_settings();
        fallback.max_results = 7;

        const auto result = store.dictionary_settings_for("../escape", fallback);
        QCOMPARE(result.max_results, 7);
    }

    // --- active profile (ProfileRepository writes initial index on first use) ---

    void activeProfileId_freshRoot_isDefaultJa() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsStore store(make_root(dir));
        QCOMPARE(store.active_profile_id(), std::string_view("default-ja"));
    }

    void activeReader_missingFile_returnsDefaults() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsStore store(make_root(dir));

        const auto result = store.active_reader();
        QCOMPARE(result.status, niratan::settings::LoadStatus::missing);
        QCOMPARE(result.settings.font_size,
                 niratan::profiles::default_reader_settings().font_size);
    }

    void activeReader_damagedFile_isInvalidWithDefaults() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto root = make_root(dir);
        SettingsStore store(root);
        write_text(root / "Profiles" / std::string(store.active_profile_id()) /
                       "reader_settings.json",
                   "{bad json");

        const auto result = store.active_reader();
        QCOMPARE(result.status, niratan::settings::LoadStatus::invalid);
        QVERIFY(!result.error.empty());
        QCOMPARE(result.settings.font_size,
                 niratan::profiles::default_reader_settings().font_size);
    }

    void activeReader_savedFile_roundTrips() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsStore store(make_root(dir));

        ReaderProfileSettings s = default_reader_settings();
        s.font_size = 30;
        const auto err = store.save_active_reader(s);
        QCOMPARE(err, StoreError::none);

        const auto result = store.active_reader();
        QCOMPARE(result.status, niratan::settings::LoadStatus::success);
        QCOMPARE(result.settings.font_size, 30);
    }

    void activeDictionary_savedFile_roundTrips() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SettingsStore store(make_root(dir));

        DictionaryProfileSettings s = default_dictionary_settings();
        s.max_results = 10;
        const auto err = store.save_active_dictionary(s);
        QCOMPARE(err, StoreError::none);

        const auto result = store.active_dictionary();
        QCOMPARE(result.status, niratan::settings::LoadStatus::success);
        QCOMPARE(result.settings.max_results, 10);
    }
};

QTEST_APPLESS_MAIN(SettingsStoreTest)
#include "tst_settings_store.moc"
