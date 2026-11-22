// SPDX-License-Identifier: GPL-3.0-or-later
//
// The profile page's view model as the page runs it: creation copies the
// active profile's owned files and activates the result, rename keeps the id
// and language, delete refuses the built-in profile and falls back to it, and
// activation autofills the new profile's Anki field mappings.
//
// Follows Core/ProfileRepository.swift (createProfile, copyProfileOwnedFiles,
// renameProfile, deleteProfile, setGlobalActiveProfile),
// Core/ProfileActivationCoordinator.swift (activateGlobal) and
// Core/AnkiManager.swift (activateProfile, autofillFieldMappings). Every test
// method points the data root at a temporary directory, so the developer's
// profiles are never read.

#include "data_root.h"

#include "services/anki/anki_config.h"
#include "services/anki/field_mapping.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "viewmodels/profiles/profiles_view_model.h"

#include <QFile>
#include <QMap>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

#include <filesystem>
#include <optional>
#include <string>

using niratan::anki::AnkiConfigStore;
using niratan::anki::FieldMappings;
using niratan::anki::NoteType;
using niratan::profiles::ContentLanguage;
using niratan::profiles::ProfileRepository;
using niratan::test::TempDataRoot;
using niratan::viewmodels::ProfilesViewModel;

namespace {

// Every profile-owned file copyProfileOwnedFiles copies.
QStringList owned_files() {
    return {QStringLiteral("dictionary_config.json"), QStringLiteral("collapsed.json"),
            QStringLiteral("dictionary_settings.json"), QStringLiteral("anki_config.json"),
            QStringLiteral("reader_settings.json")};
}

std::filesystem::path profile_directory(std::string_view id) {
    return niratan::storage::data_root() / "Profiles" / std::string(id);
}

std::filesystem::path anki_file(std::string_view id) {
    return profile_directory(id) / "anki_config.json";
}

QByteArray read_bytes(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool write_bytes(const std::filesystem::path& path, const QByteArray& bytes) {
    std::filesystem::create_directories(path.parent_path());
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(bytes) == bytes.size();
}

// The five profile-owned files as they are on disk.
QMap<QString, QByteArray> owned_bytes(std::string_view id) {
    QMap<QString, QByteArray> bytes;
    for (const QString& name : owned_files()) {
        bytes.insert(name, read_bytes(profile_directory(id) / name.toStdString()));
    }
    return bytes;
}

std::string last_id(const ProfilesViewModel& model) {
    return model.profiles().last().toMap().value(QStringLiteral("id")).toString().toStdString();
}

// Seeded Anki profile state for the autofill cases.
void seed_anki_profile(std::string_view id, const std::optional<FieldMappings>& mappings) {
    ProfileRepository repository{niratan::storage::data_root()};
    QVERIFY(!repository.set_active_profile(id).has_value());

    AnkiConfigStore store{niratan::storage::data_root(), repository};
    store.load();
    store.settings().selected_note_type = "Lapis";
    store.settings().available_note_types = {NoteType{"Lapis", {"Expression", "Sentence"}}};
    if (mappings.has_value()) {
        store.settings().field_mappings = *mappings;
    }
    QVERIFY(store.save().ok);

    QVERIFY(!repository.set_active_profile("default-ja").has_value());
}

}  // namespace

class ProfilesViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void create_profile_copies_the_active_profiles_files_and_activates();
    void create_profile_copies_only_existing_files();
    void rename_profile_keeps_the_id_and_language();
    void delete_profile_refuses_the_builtin_and_falls_back_to_default();
    void activating_a_profile_autofills_its_field_mappings();
    void activating_does_not_rewrite_unchanged_mappings();
    void activating_the_active_profile_does_not_autofill();
};

// B1: ProfileRepository.createProfile and copyProfileOwnedFiles, as the create
// sheet and ProfilesView.activate run them.
void ProfilesViewModelTest::create_profile_copies_the_active_profiles_files_and_activates() {
    TempDataRoot root;
    QVERIFY(root.isValid());

    const std::filesystem::path default_directory = profile_directory("default-ja");
    QVERIFY(write_bytes(
        default_directory / "dictionary_config.json",
        R"({"termDictionaries":[],"frequencyDictionaries":[],"pitchDictionaries":[]})"));
    QVERIFY(write_bytes(default_directory / "collapsed.json", R"(["Probe"])"));
    QVERIFY(write_bytes(default_directory / "dictionary_settings.json", R"({"maxResults":7})"));
    QVERIFY(write_bytes(default_directory / "reader_settings.json", R"({"fontSize":30})"));
    QVERIFY(write_bytes(default_directory / "notes.txt", "x"));

    {
        ProfileRepository repository{niratan::storage::data_root()};
        AnkiConfigStore store{niratan::storage::data_root(), repository};
        store.load();
        store.settings().tags = "source-tags";
        QVERIFY(store.save().ok);
    }

    const QMap<QString, QByteArray> before = owned_bytes("default-ja");
    for (auto it = before.cbegin(); it != before.cend(); ++it) {
        QVERIFY2(!it.value().isEmpty(), qPrintable(it.key()));
    }

    ProfilesViewModel model;
    QVERIFY(model.createProfile(QStringLiteral("  English \n"), QStringLiteral("en")));

    const QVariantList profiles = model.profiles();
    QCOMPARE(profiles.size(), qsizetype(2));
    const QVariantMap created = profiles.last().toMap();
    QCOMPARE(created.value(QStringLiteral("name")).toString(), QStringLiteral("English"));
    QCOMPARE(created.value(QStringLiteral("language")).toString(), QStringLiteral("en"));
    QVERIFY(created.value(QStringLiteral("active")).toBool());
    QVERIFY(!created.value(QStringLiteral("builtIn")).toBool());
    const std::string id = created.value(QStringLiteral("id")).toString().toStdString();

    ProfileRepository fresh{niratan::storage::data_root()};
    QCOMPARE(fresh.active_profile().id, id);
    const auto primary = fresh.index().primary_profile_ids_by_language.find("en");
    QVERIFY(primary != fresh.index().primary_profile_ids_by_language.end());
    QCOMPARE(primary->second, id);

    const QMap<QString, QByteArray> copied = owned_bytes(id);
    for (const QString& name : owned_files()) {
        QCOMPARE(copied.value(name), before.value(name));
    }
    QVERIFY(!std::filesystem::exists(profile_directory(id) / "notes.txt"));

    QVERIFY(!model.createProfile(QStringLiteral(" \t\n\u3000"), QStringLiteral("ja")));
    QCOMPARE(model.error(), QStringLiteral("Profile name cannot be empty."));
    QCOMPARE(model.profiles().size(), qsizetype(2));
    ProfileRepository unchanged{niratan::storage::data_root()};
    QCOMPARE(unchanged.index().profiles.size(), std::size_t(2));
}

// B1: createProfile(copyFromProfileID:) copies only files that exist.
void ProfilesViewModelTest::create_profile_copies_only_existing_files() {
    TempDataRoot root;
    QVERIFY(root.isValid());

    ProfilesViewModel model;
    QVERIFY(model.createProfile(QStringLiteral("Empty"), QStringLiteral("ja")));

    const std::string id = last_id(model);
    const std::filesystem::path directory = profile_directory(id);
    QVERIFY(std::filesystem::is_directory(directory));
    QVERIFY(std::filesystem::is_empty(directory));
}

// B2: ProfileRepository.renameProfile; ProfilesView.profileEditor(locksLanguage:).
void ProfilesViewModelTest::rename_profile_keeps_the_id_and_language() {
    TempDataRoot root;
    QVERIFY(root.isValid());

    ProfilesViewModel model;
    QVERIFY(model.createProfile(QStringLiteral("English"), QStringLiteral("en")));
    const std::string id = last_id(model);

    QVERIFY(model.renameProfile(QString::fromStdString(id), QStringLiteral("  English Novel \n")));
    const QVariantMap renamed = model.profiles().last().toMap();
    QCOMPARE(renamed.value(QStringLiteral("name")).toString(), QStringLiteral("English Novel"));
    QCOMPARE(renamed.value(QStringLiteral("id")).toString().toStdString(), id);
    QCOMPARE(renamed.value(QStringLiteral("language")).toString(), QStringLiteral("en"));
    QVERIFY(renamed.value(QStringLiteral("active")).toBool());

    ProfileRepository fresh{niratan::storage::data_root()};
    QVERIFY(fresh.profile(id).has_value());
    QCOMPARE(fresh.profile(id)->name, std::string("English Novel"));

    QVERIFY(!model.renameProfile(QString::fromStdString(id), QStringLiteral(" \n")));
    QCOMPARE(model.error(), QStringLiteral("Profile name cannot be empty."));
    ProfileRepository unchanged{niratan::storage::data_root()};
    QCOMPARE(unchanged.profile(id)->name, std::string("English Novel"));

    QVERIFY(!model.renameProfile(QStringLiteral("profile-missing"), QStringLiteral("X")));
    QCOMPARE(model.error(), QStringLiteral("The selected profile no longer exists."));
}

// B3: ProfileRepository.deleteProfile and ProfilesView's delete confirmation.
void ProfilesViewModelTest::delete_profile_refuses_the_builtin_and_falls_back_to_default() {
    TempDataRoot root;
    QVERIFY(root.isValid());

    ProfilesViewModel model;
    QVERIFY(!model.deleteProfile(QStringLiteral("default-ja")));
    QCOMPARE(model.error(), QStringLiteral("The default profile cannot be deleted."));
    QCOMPARE(model.profiles().size(), qsizetype(1));

    QVERIFY(model.createProfile(QStringLiteral("English"), QStringLiteral("en")));
    const std::string id = last_id(model);
    QVERIFY(std::filesystem::is_directory(profile_directory(id)));

    QVERIFY(model.deleteProfile(QString::fromStdString(id)));
    QCOMPARE(model.profiles().size(), qsizetype(1));
    const QVariantMap remaining = model.profiles().first().toMap();
    QCOMPARE(remaining.value(QStringLiteral("id")).toString(), QStringLiteral("default-ja"));
    QVERIFY(remaining.value(QStringLiteral("active")).toBool());
    QVERIFY(!std::filesystem::exists(profile_directory(id)));

    ProfileRepository fresh{niratan::storage::data_root()};
    QCOMPARE(fresh.active_profile().id, std::string("default-ja"));
    QVERIFY(fresh.index().primary_profile_ids_by_language.find("en") ==
            fresh.index().primary_profile_ids_by_language.end());

    QVERIFY(!model.deleteProfile(QStringLiteral("profile-missing")));
    QCOMPARE(model.error(), QStringLiteral("The selected profile no longer exists."));
}

// B4: AnkiManager.activateProfile with autofillFieldMappings and save; the
// template keeps an explicitly empty mapping and adds the missing default.
void ProfilesViewModelTest::activating_a_profile_autofills_its_field_mappings() {
    TempDataRoot root;
    QVERIFY(root.isValid());

    std::string other;
    {
        ProfileRepository repository{niratan::storage::data_root()};
        const auto created =
            repository.create_profile("Other", ContentLanguage::english, std::nullopt);
        QVERIFY(created.profile.has_value());
        other = created.profile->id;
    }
    seed_anki_profile(other, FieldMappings{{"Sentence", ""}});

    ProfilesViewModel model;
    QVERIFY(model.activateProfile(QString::fromStdString(other)));

    ProfileRepository fresh{niratan::storage::data_root()};
    QCOMPARE(fresh.active_profile().id, other);
    AnkiConfigStore reloaded{niratan::storage::data_root(), fresh};
    reloaded.load();
    const FieldMappings& mappings = reloaded.settings().field_mappings;
    QCOMPARE(static_cast<int>(mappings.size()), 2);
    QCOMPARE(mappings.at("Expression"), std::string("{expression}"));
    QCOMPARE(mappings.at("Sentence"), std::string(""));
}

// B4: the configuration is written only when the mappings changed.
void ProfilesViewModelTest::activating_does_not_rewrite_unchanged_mappings() {
    TempDataRoot root;
    QVERIFY(root.isValid());

    std::string other;
    {
        ProfileRepository repository{niratan::storage::data_root()};
        const auto created =
            repository.create_profile("Other", ContentLanguage::english, std::nullopt);
        QVERIFY(created.profile.has_value());
        other = created.profile->id;
    }
    seed_anki_profile(other, FieldMappings{{"Expression", "{expression}"}, {"Sentence", ""}});

    QVERIFY(write_bytes(anki_file(other), read_bytes(anki_file(other)) + "\n  \n"));
    const QByteArray padded = read_bytes(anki_file(other));
    QVERIFY(!padded.isEmpty());

    ProfilesViewModel model;
    QVERIFY(model.activateProfile(QString::fromStdString(other)));
    QCOMPARE(read_bytes(anki_file(other)), padded);
    QVERIFY(model.activateProfile(QString::fromStdString(other)));
    QCOMPARE(read_bytes(anki_file(other)), padded);
}

// B4: activateProfile's guard leaves the already active profile, and its file,
// untouched.
void ProfilesViewModelTest::activating_the_active_profile_does_not_autofill() {
    TempDataRoot root;
    QVERIFY(root.isValid());

    std::string other;
    {
        ProfileRepository repository{niratan::storage::data_root()};
        const auto created =
            repository.create_profile("Other", ContentLanguage::english, std::nullopt);
        QVERIFY(created.profile.has_value());
        other = created.profile->id;
        QVERIFY(!repository.set_active_profile(other).has_value());
    }
    seed_anki_profile(other, FieldMappings{{"Sentence", ""}});
    QVERIFY(write_bytes(anki_file(other), read_bytes(anki_file(other)) + "\n  \n"));
    const QByteArray padded = read_bytes(anki_file(other));

    // The view model's repository has Other active already.
    {
        ProfileRepository repository{niratan::storage::data_root()};
        QVERIFY(!repository.set_active_profile(other).has_value());
    }
    ProfilesViewModel active_model;
    QVERIFY(active_model.activateProfile(QString::fromStdString(other)));
    QCOMPARE(read_bytes(anki_file(other)), padded);

    ProfileRepository fresh{niratan::storage::data_root()};
    AnkiConfigStore reloaded{niratan::storage::data_root(), fresh};
    reloaded.load();
    const FieldMappings& mappings = reloaded.settings().field_mappings;
    QCOMPARE(static_cast<int>(mappings.size()), 1);
    QCOMPARE(mappings.at("Sentence"), std::string(""));
}

QTEST_GUILESS_MAIN(ProfilesViewModelTest)
#include "tst_profiles_view_model.moc"
