// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/profiles/profile_repository.h"

#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "models/profiles/profile_index_json.h"
#include "services/dictionary/dictionary_config_store.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

using niratan::dictionary::DictionaryConfig;
using niratan::dictionary::load_collapsed_dictionaries;
using niratan::dictionary::load_dictionary_config;
using niratan::profiles::ContentLanguage;
using niratan::profiles::profile_index_from_json;
using niratan::profiles::ProfileError;
using niratan::profiles::ProfileIndex;
using niratan::profiles::ProfileRepository;
using niratan::storage::read_json_file;

// Core/ProfileRepository.swift and script/test_profile_repository.swift
// (testRepositoryMigrationAndPersistence, testProfileLifecycleAndPathSafety)
// define the behaviour; the cases follow them, without the macOS legacy
// migrations, which this layer does not implement.
namespace {

std::filesystem::path app_dir(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
}

std::filesystem::path index_file(const QTemporaryDir& directory) {
    return app_dir(directory) / "Profiles" / "profiles.json";
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::optional<ProfileIndex> persisted_index(const QTemporaryDir& directory) {
    const auto read = read_json_file(index_file(directory));
    if (read.status != niratan::storage::JsonReadStatus::success) {
        return std::nullopt;
    }
    return profile_index_from_json(read.document);
}

std::filesystem::path profile_directory(const QTemporaryDir& directory, const std::string& id) {
    return app_dir(directory) / "Profiles" / id;
}

}  // namespace

class ProfileRepositoryTest : public QObject {
    Q_OBJECT

   private slots:
    void construction_creates_the_directory_and_index();
    void a_persisted_index_is_reloaded();
    void a_normalized_index_is_left_untouched();
    void a_changed_normalization_is_saved();
    void create_generates_a_safe_id_and_directory();
    void create_copies_the_profile_owned_files();
    void create_rejects_a_blank_name_without_a_directory();
    void rename_and_delete_persist();
    void the_default_profile_cannot_be_deleted();
    void a_damaged_index_is_preserved();
    void primary_selection_requires_a_matching_language();
    void active_profile_uses_the_resolver();
    void the_data_root_repository_lives_under_the_data_root();
    void no_data_root_gives_no_repository();
    void removing_a_dictionary_cleans_every_profile();
    void removing_a_dictionary_renumbers_even_without_a_match();
    void damaged_dictionary_files_are_left_untouched();
};

void ProfileRepositoryTest::construction_creates_the_directory_and_index() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));

    QCOMPARE(repository.index(), ProfileIndex::initial());
    QVERIFY(std::filesystem::exists(index_file(directory)));
    QVERIFY(std::filesystem::exists(app_dir(directory) / "Profiles" / "default-ja"));
    QCOMPARE(persisted_index(directory), std::optional<ProfileIndex>{ProfileIndex::initial()});
}

void ProfileRepositoryTest::a_persisted_index_is_reloaded() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    {
        ProfileRepository repository(app_dir(directory));
        const auto result = repository.create_profile("English", ContentLanguage::english,
                                                      std::nullopt);
        QVERIFY(result.profile.has_value());
        QVERIFY(!repository.set_active_profile(result.profile->id).has_value());
    }

    ProfileRepository reopened(app_dir(directory));
    QCOMPARE(reopened.index().profiles.size(), 2u);
    QCOMPARE(reopened.index().global_active_profile_id, reopened.index().profiles.back().id);
}

// ProfileRepository.init loads and normalizes the index; following the
// Decision, a decodable index that normalization leaves unchanged is not
// rewritten, so its bytes and modification time stay as they were.
void ProfileRepositoryTest::a_normalized_index_is_left_untouched() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    {
        ProfileRepository repository(app_dir(directory));
        const auto result =
            repository.create_profile("English", ContentLanguage::english, std::nullopt);
        QVERIFY(result.profile.has_value());
    }

    // The same index in a formatting the writer never produces.
    QFile file(QString::fromStdString(index_file(directory).string()));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray compact =
        QJsonDocument::fromJson(file.readAll()).toJson(QJsonDocument::Compact) + "\n\n";
    file.close();
    write_text(index_file(directory), compact.toStdString());
    const auto sentinel = std::filesystem::file_time_type::clock::now() - std::chrono::hours(48);
    std::filesystem::last_write_time(index_file(directory), sentinel);

    ProfileRepository reopened(app_dir(directory));

    QCOMPARE(reopened.index().profiles.size(), 2u);
    QCOMPARE(read_text(index_file(directory)), compact.toStdString());
    QVERIFY(std::filesystem::last_write_time(index_file(directory)) == sentinel);
}

// ProfileRepository.init with persistIndex: an index that normalization
// changes (the legacy built-in name "Japanese EPUB") is saved normalized.
void ProfileRepositoryTest::a_changed_normalization_is_saved() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    std::filesystem::create_directories(app_dir(directory) / "Profiles");
    ProfileIndex legacy = ProfileIndex::initial();
    QVERIFY(!legacy.profiles.empty());
    legacy.profiles.front().name = "Japanese EPUB";
    QFile file(QString::fromStdString(index_file(directory).string()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(niratan::profiles::profile_index_to_json(legacy).toJson());
    file.close();
    const auto sentinel = std::filesystem::file_time_type::clock::now() - std::chrono::hours(48);
    std::filesystem::last_write_time(index_file(directory), sentinel);

    ProfileRepository repository(app_dir(directory));

    QCOMPARE(repository.index(), ProfileIndex::initial());
    QCOMPARE(persisted_index(directory), std::optional<ProfileIndex>{ProfileIndex::initial()});
    QVERIFY(std::filesystem::last_write_time(index_file(directory)) != sentinel);
}

void ProfileRepositoryTest::create_generates_a_safe_id_and_directory() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));

    const auto result = repository.create_profile("  English  ", ContentLanguage::english,
                                                  std::nullopt);

    QVERIFY(result.profile.has_value());
    QVERIFY(!result.error.has_value());
    QCOMPARE(result.profile->name, std::string("English"));
    QVERIFY(result.profile->id.starts_with("profile-"));
    QVERIFY(niratan::profiles::is_safe_profile_id(result.profile->id));
    QVERIFY(std::filesystem::exists(app_dir(directory) / "Profiles" / result.profile->id));
    QCOMPARE(repository.index().primary_profile_ids_by_language.at("en"), result.profile->id);

    const auto persisted = persisted_index(directory);
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->profiles.size(), 2u);
}

void ProfileRepositoryTest::create_copies_the_profile_owned_files() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));

    const std::filesystem::path source =
        app_dir(directory) / "Profiles" / "default-ja";
    const std::vector<std::string> owned{"dictionary_config.json", "collapsed.json",
                                         "dictionary_settings.json", "anki_config.json",
                                         "reader_settings.json"};
    for (const std::string& name : owned) {
        write_text(source / name, "{}");
    }
    write_text(source / "not_owned.json", "{}");

    const auto result = repository.create_profile("Copy", ContentLanguage::japanese,
                                                  std::string("default-ja"));

    QVERIFY(result.profile.has_value());
    const std::filesystem::path destination =
        app_dir(directory) / "Profiles" / result.profile->id;
    for (const std::string& name : owned) {
        QVERIFY2(std::filesystem::exists(destination / name), name.c_str());
    }
    QVERIFY(!std::filesystem::exists(destination / "not_owned.json"));
}

void ProfileRepositoryTest::create_rejects_a_blank_name_without_a_directory() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));
    const std::size_t before = repository.index().profiles.size();

    const auto result = repository.create_profile("\u3000 \n", ContentLanguage::japanese,
                                                  std::nullopt);

    QCOMPARE(result.error, std::optional<ProfileError>{ProfileError::blank_name});
    QCOMPARE(repository.index().profiles.size(), before);
    std::size_t entries = 0;
    for ([[maybe_unused]] const auto& entry :
         std::filesystem::directory_iterator(app_dir(directory) / "Profiles")) {
        ++entries;
    }
    QCOMPARE(entries, std::size_t{2});
}

void ProfileRepositoryTest::rename_and_delete_persist() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));
    const auto created = repository.create_profile("English", ContentLanguage::english,
                                                   std::nullopt);
    QVERIFY(created.profile.has_value());
    const std::string id = created.profile->id;

    QVERIFY(!repository.rename_profile(id, "  English JH  ").has_value());
    QCOMPARE(repository.profile(id)->name, std::string("English JH"));

    QVERIFY(!repository.delete_profile(id).has_value());
    QVERIFY(!repository.profile(id).has_value());
    QVERIFY(!std::filesystem::exists(app_dir(directory) / "Profiles" / id));

    ProfileRepository reopened(app_dir(directory));
    QCOMPARE(reopened.index().profiles.size(), 1u);
    QCOMPARE(reopened.index().global_active_profile_id, std::string("default-ja"));
}

void ProfileRepositoryTest::the_default_profile_cannot_be_deleted() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));

    QCOMPARE(repository.delete_profile("default-ja"),
             std::optional<ProfileError>{ProfileError::cannot_delete_default_profile});
    QCOMPARE(repository.index().profiles.size(), 1u);
}

void ProfileRepositoryTest::a_damaged_index_is_preserved() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    std::filesystem::create_directories(app_dir(directory) / "Profiles");
    write_text(index_file(directory), "not json");

    ProfileRepository repository(app_dir(directory));

    QCOMPARE(repository.index(), ProfileIndex::initial());
    QCOMPARE(read_text(index_file(directory)), std::string("not json"));
}

void ProfileRepositoryTest::primary_selection_requires_a_matching_language() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));
    const auto created = repository.create_profile("English", ContentLanguage::english,
                                                   std::nullopt);
    QVERIFY(created.profile.has_value());

    QCOMPARE(repository.set_primary_profile(created.profile->id, ContentLanguage::japanese),
             std::optional<ProfileError>{ProfileError::language_mismatch});
    QVERIFY(!repository.set_primary_profile("default-ja", ContentLanguage::japanese).has_value());
    QCOMPARE(repository.index().primary_profile_ids_by_language.at("ja"),
             std::string("default-ja"));
}

void ProfileRepositoryTest::active_profile_uses_the_resolver() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));
    const auto created = repository.create_profile("English", ContentLanguage::english,
                                                   std::nullopt);
    QVERIFY(created.profile.has_value());
    QVERIFY(!repository.set_active_profile(created.profile->id).has_value());

    QCOMPARE(repository.active_profile().id, created.profile->id);
}

void ProfileRepositoryTest::the_data_root_repository_lives_under_the_data_root() {
    const QByteArray data_home = qgetenv("XDG_DATA_HOME");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("XDG_DATA_HOME", directory.path().toUtf8());

    const auto repository = ProfileRepository::at_data_root();
    const std::filesystem::path root = niratan::storage::data_root();
    qputenv("XDG_DATA_HOME", data_home);

    QVERIFY(repository.has_value());
    QVERIFY(!root.empty());
    QVERIFY(std::filesystem::exists(root / "Profiles" / "profiles.json"));
}

void ProfileRepositoryTest::no_data_root_gives_no_repository() {
    const QByteArray data_home = qgetenv("XDG_DATA_HOME");
    const QByteArray home = qgetenv("HOME");
    qunsetenv("XDG_DATA_HOME");
    qunsetenv("HOME");

    const auto repository = ProfileRepository::at_data_root();
    qputenv("XDG_DATA_HOME", data_home);
    qputenv("HOME", home);
    if (data_home.isEmpty()) {
        qunsetenv("XDG_DATA_HOME");
    }

    QVERIFY(!repository.has_value());
}

// B8: removeDictionaryReferences drops the file name from all four arrays of
// every profile, renumbers each present array and rewrites the config; a
// collapsed set loses the title only when it contained it.
void ProfileRepositoryTest::removing_a_dictionary_cleans_every_profile() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));
    const auto other =
        repository.create_profile("Other", ContentLanguage::japanese, std::string("default-ja"));
    QVERIFY(other.profile.has_value());
    const auto default_directory = profile_directory(directory, "default-ja");
    const auto other_directory = profile_directory(directory, other.profile->id);

    write_text(
        default_directory / "dictionary_config.json",
        R"({"termDictionaries":[{"fileName":"A","isEnabled":true,"order":0},{"fileName":"B","isEnabled":true,"order":1}],"frequencyDictionaries":[{"fileName":"A","isEnabled":true,"order":3},{"fileName":"F","isEnabled":true,"order":4}],"pitchDictionaries":[{"fileName":"A","isEnabled":true,"order":2},{"fileName":"P","isEnabled":true,"order":3}],"kanjiDictionaries":[{"fileName":"A","isEnabled":true,"order":1},{"fileName":"K","isEnabled":true,"order":2}]})");
    write_text(
        other_directory / "dictionary_config.json",
        R"({"termDictionaries":[{"fileName":"A","isEnabled":true,"order":0},{"fileName":"B","isEnabled":true,"order":1},{"fileName":"C","isEnabled":true,"order":2}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    write_text(default_directory / "collapsed.json", R"(["A","Z"])");
    write_text(other_directory / "collapsed.json", R"(["B"])");

    repository.remove_dictionary_references("A", "A");

    const auto default_config =
        load_dictionary_config(default_directory / "dictionary_config.json");
    QVERIFY(default_config.has_value());
    QCOMPARE(default_config->term_dictionaries.size(), std::size_t(1));
    QCOMPARE(default_config->term_dictionaries.front().file_name, std::string("B"));
    QCOMPARE(default_config->term_dictionaries.front().order, std::int64_t(0));
    QCOMPARE(default_config->frequency_dictionaries.size(), std::size_t(1));
    QCOMPARE(default_config->frequency_dictionaries.front().file_name, std::string("F"));
    QCOMPARE(default_config->frequency_dictionaries.front().order, std::int64_t(0));
    QCOMPARE(default_config->pitch_dictionaries.size(), std::size_t(1));
    QCOMPARE(default_config->pitch_dictionaries.front().file_name, std::string("P"));
    QCOMPARE(default_config->pitch_dictionaries.front().order, std::int64_t(0));
    QVERIFY(default_config->kanji_dictionaries.has_value());
    QCOMPARE(default_config->kanji_dictionaries->size(), std::size_t(1));
    QCOMPARE(default_config->kanji_dictionaries->front().file_name, std::string("K"));
    QCOMPARE(default_config->kanji_dictionaries->front().order, std::int64_t(0));

    const auto other_config = load_dictionary_config(other_directory / "dictionary_config.json");
    QVERIFY(other_config.has_value());
    QCOMPARE(other_config->term_dictionaries.size(), std::size_t(2));
    QCOMPARE(other_config->term_dictionaries.at(0).file_name, std::string("B"));
    QCOMPARE(other_config->term_dictionaries.at(0).order, std::int64_t(0));
    QCOMPARE(other_config->term_dictionaries.at(1).file_name, std::string("C"));
    QCOMPARE(other_config->term_dictionaries.at(1).order, std::int64_t(1));

    QVERIFY(load_collapsed_dictionaries(default_directory / "collapsed.json") ==
            std::set<std::string>{"Z"});
    QCOMPARE(read_text(other_directory / "collapsed.json"), std::string(R"(["B"])"));
}

// B8: a decodable config is rewritten and renumbered even when no entry
// matches; a collapsed file without the title keeps its exact bytes.
void ProfileRepositoryTest::removing_a_dictionary_renumbers_even_without_a_match() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));
    const auto other =
        repository.create_profile("Other", ContentLanguage::japanese, std::string("default-ja"));
    QVERIFY(other.profile.has_value());
    const auto default_directory = profile_directory(directory, "default-ja");
    const auto other_directory = profile_directory(directory, other.profile->id);

    write_text(
        default_directory / "dictionary_config.json",
        R"({"termDictionaries":[{"fileName":"X","isEnabled":true,"order":5},{"fileName":"Y","isEnabled":true,"order":9}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    write_text(default_directory / "collapsed.json", R"(["Z"])");
    write_text(other_directory / "collapsed.json", R"(["B"])");

    repository.remove_dictionary_references("Nope", "Z");

    const auto config = load_dictionary_config(default_directory / "dictionary_config.json");
    QVERIFY(config.has_value());
    QCOMPARE(config->term_dictionaries.size(), std::size_t(2));
    QCOMPARE(config->term_dictionaries.at(0).file_name, std::string("X"));
    QCOMPARE(config->term_dictionaries.at(0).order, std::int64_t(0));
    QCOMPARE(config->term_dictionaries.at(1).file_name, std::string("Y"));
    QCOMPARE(config->term_dictionaries.at(1).order, std::int64_t(1));
    QVERIFY(!config->kanji_dictionaries.has_value());
    QVERIFY(load_collapsed_dictionaries(default_directory / "collapsed.json").empty());
    QCOMPARE(read_text(other_directory / "collapsed.json"), std::string(R"(["B"])"));
}

// B8: invalid files are left byte-identical and a missing config is not
// created.
void ProfileRepositoryTest::damaged_dictionary_files_are_left_untouched() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ProfileRepository repository(app_dir(directory));
    const auto other =
        repository.create_profile("Other", ContentLanguage::japanese, std::string("default-ja"));
    QVERIFY(other.profile.has_value());
    const auto default_directory = profile_directory(directory, "default-ja");
    const auto other_directory = profile_directory(directory, other.profile->id);

    write_text(default_directory / "dictionary_config.json", "{");
    write_text(default_directory / "collapsed.json", "{");

    repository.remove_dictionary_references("A", "A");

    QCOMPARE(read_text(default_directory / "dictionary_config.json"), std::string("{"));
    QCOMPARE(read_text(default_directory / "collapsed.json"), std::string("{"));
    QVERIFY(!std::filesystem::exists(other_directory / "dictionary_config.json"));
    QVERIFY(!std::filesystem::exists(other_directory / "collapsed.json"));
}

QTEST_APPLESS_MAIN(ProfileRepositoryTest)
#include "tst_profile_repository.moc"
