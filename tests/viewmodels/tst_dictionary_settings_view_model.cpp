// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Dictionaries settings page, following Features/Settings/DictionaryView.swift
// (dictionaryTypePicker, deleteDictionary, reorderDictionary,
// DictionaryCategoryPicker) and Core/DictionaryManager.swift
// (loadDictionaries, applyActiveProfileDictionaryConfiguration,
// toggleDictionary, setDictionaryCategory, moveDictionary, updateOrder,
// deleteDictionary, importDictionary, enableDictionaries, saveDictionaryConfig),
// with Core/ProfileRepository.swift (removeDictionaryReferences) and
// Models/Dictionary.swift (DictionaryType, DictionaryCategory,
// DictionaryReorder.destinationOffset).

#include "viewmodels/dictionary/dictionary_settings_view_model.h"

#include "data_root.h"
#include "yomitan_fixture.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
#include <QtTest>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "models/dictionary/dictionary_config.h"
#include "services/dictionary/dictionary_config_store.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

using niratan::dictionary::DictionaryConfig;
using niratan::dictionary::DictionaryEntry;
using niratan::dictionary::DictionaryStore;
using niratan::dictionary::load_dictionary_config;
using niratan::dictionary::save_dictionary_config;
using niratan::profiles::ContentLanguage;
using niratan::profiles::ProfileRepository;
using niratan::test::minimal_index;
using niratan::test::minimal_term_bank;
using niratan::test::TempDataRoot;
using niratan::test::write_stored_zip;
using niratan::viewmodels::DictionarySettingsViewModel;

namespace {

const QByteArray kProbeTermBank =
    QByteArrayLiteral("[[\"decide\",\"\",\"\",\"v\",0,[\"to make up one's mind\"],1,\"\"]]");
const QByteArray kSecondTermBank =
    QByteArrayLiteral("[[\"second\",\"\",\"\",\"\",0,[\"second\"],2,\"\"]]");

std::filesystem::path path_in(const TempDataRoot& root, const char* name) {
    return std::filesystem::path(root.path().toStdString()) / name;
}

bool write_file(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

bool install_zip(const std::filesystem::path& zip) {
    return DictionaryStore(niratan::storage::dictionaries_root()).import_zip(zip).ok;
}

// Probe is a term dictionary with a frequency bank, so the typed store has a
// Term and a Frequency directory named Probe.
struct Fixture {
    TempDataRoot data;
    bool imported = false;

    Fixture() {
        const auto zip = path_in(data, "probe.zip");
        write_stored_zip(
            zip,
            {{"index.json", minimal_index(QStringLiteral("Probe"))},
             {"term_bank_1.json", kProbeTermBank},
             {"term_meta_bank_1.json", QByteArrayLiteral(R"([["decide","freq",100]])")}});
        imported = install_zip(zip);
    }
};

std::filesystem::path profile_directory(const std::string& profile_id) {
    ProfileRepository repository{niratan::storage::data_root()};
    const auto directory = repository.profile_directory(profile_id);
    return directory.value_or(std::filesystem::path{});
}

std::optional<DictionaryConfig> saved_config(const std::string& profile_id) {
    const auto directory = profile_directory(profile_id);
    if (directory.empty()) {
        return std::nullopt;
    }
    return load_dictionary_config(directory / "dictionary_config.json");
}

std::optional<QJsonObject> saved_config_object(const std::string& profile_id) {
    const auto directory = profile_directory(profile_id);
    if (directory.empty()) {
        return std::nullopt;
    }
    QFile file(QString::fromStdString((directory / "dictionary_config.json").string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

QString row_text(const QVariantList& rows, qsizetype index) {
    return rows.at(index).toMap().value(QStringLiteral("title")).toString();
}

bool row_enabled(const QVariantList& rows, qsizetype index) {
    return rows.at(index).toMap().value(QStringLiteral("enabled")).toBool();
}

// Holds the FIFO writer independently of the GUI thread. A synchronous catalog
// read finishes only through the watchdog, while an asynchronous one is
// released explicitly by the test. Destruction always releases and joins.
class HeldIndexWriter {
   public:
    HeldIndexWriter(QString path, QByteArray bytes)
        : path_(std::move(path)), bytes_(std::move(bytes)), worker_([this] { run(); }) {}
    ~HeldIndexWriter() { release(); }

    HeldIndexWriter(const HeldIndexWriter&) = delete;
    HeldIndexWriter& operator=(const HeldIndexWriter&) = delete;

    void release() {
        released_ = true;
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    bool watchdog_released() const { return watchdog_released_; }

   private:
    void run() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!released_ && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (!released_) {
            watchdog_released_ = true;
        }

        int writer = -1;
        for (int attempt = 0; attempt < 200 && writer < 0; ++attempt) {
            writer = open(path_.toLocal8Bit().constData(), O_WRONLY | O_NONBLOCK);
            if (writer < 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        if (writer < 0) {
            return;
        }
        qsizetype offset = 0;
        while (offset < bytes_.size()) {
            const ssize_t count =
                write(writer, bytes_.constData() + offset, bytes_.size() - offset);
            if (count <= 0) {
                break;
            }
            offset += count;
        }
        close(writer);
    }

    QString path_;
    QByteArray bytes_;
    std::atomic<bool> released_ = false;
    std::atomic<bool> watchdog_released_ = false;
    std::thread worker_;
};

}  // namespace

class DictionarySettingsViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void opening_returns_while_the_catalog_read_is_held();
    void opening_presents_the_profile_catalog_asynchronously();
    void a_late_open_result_cannot_override_a_newer_type();
    void a_late_open_result_cannot_override_a_mutation();
    void rows_are_the_selected_types_resolved_list();
    void enabling_a_title_writes_the_config();
    void a_category_change_is_term_only();
    void moving_reorders_and_renumbers_the_selected_list();
    void removing_deletes_only_the_selected_type();
    void importing_into_a_non_default_profile_enables_and_saves();
    void import_failures_are_reported();
    void frequency_dictionary_titles_are_the_enabled_titles();
};

// B1: opening returns while a worker is held on the catalog read, then
// publishes the completed rows on the GUI thread.
void DictionarySettingsViewModelTest::opening_returns_while_the_catalog_read_is_held() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    const auto index = niratan::storage::dictionaries_root() / "Term" / "Probe" / "index.json";
    const QString index_path = QString::fromStdString(index.string());
    QVERIFY(QFile::remove(index_path));
    QCOMPARE(mkfifo(index_path.toLocal8Bit().constData(), 0600), 0);

    HeldIndexWriter writer(index_path, minimal_index(QStringLiteral("Probe")));
    DictionarySettingsViewModel model;
    model.openAsync();
    QVERIFY(model.rows().isEmpty());
    QVERIFY(model.recommended().isEmpty());

    writer.release();
    QVERIFY(!writer.watchdog_released());
    QTRY_COMPARE(model.rows().size(), qsizetype(1));
    QCOMPARE(row_text(model.rows(), 0), QStringLiteral("Probe"));
}

// B1/B2: the completed result retains the active profile's configured
// enablement, category and recommendation language.
void DictionarySettingsViewModelTest::opening_presents_the_profile_catalog_asynchronously() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    ProfileRepository repository{niratan::storage::data_root()};
    const auto english =
        repository.create_profile("English", ContentLanguage::english, std::nullopt);
    QVERIFY(english.profile.has_value());
    QVERIFY(!repository.set_active_profile(english.profile->id).has_value());
    DictionaryConfig config;
    config.term_dictionaries = {
        DictionaryEntry{"Probe", true, 0, niratan::anki::DictionaryCategory::bilingual}};
    QVERIFY(save_dictionary_config(
        profile_directory(english.profile->id) / "dictionary_config.json", config));

    DictionarySettingsViewModel model;
    QSignalSpy changed(&model, &DictionarySettingsViewModel::rowsChanged);
    model.openAsync();
    QTRY_COMPARE(model.rows().size(), qsizetype(1));
    QCOMPARE(model.rows().at(0).toMap(),
             (QVariantMap{{"title", QStringLiteral("Probe")},
                          {"enabled", true},
                          {"category", QStringLiteral("bilingual")}}));
    QVERIFY(!model.recommended().isEmpty());
    for (const QVariant& value : model.recommended()) {
        QCOMPARE(value.toMap().value(QStringLiteral("language")).toString(), QStringLiteral("en"));
    }
    QCOMPARE(changed.count(), 1);
}

// A synchronous type selection supersedes an initial read whose result arrives
// later on the GUI thread.
void DictionarySettingsViewModelTest::a_late_open_result_cannot_override_a_newer_type() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    DictionarySettingsViewModel model;
    QSignalSpy changed(&model, &DictionarySettingsViewModel::rowsChanged);
    model.openAsync();
    QTRY_COMPARE(model.rows().size(), qsizetype(1));

    model.openAsync();
    model.selectType(QStringLiteral("Frequency"));
    QCOMPARE(model.dictionary_type(), QStringLiteral("Frequency"));
    QCOMPARE(row_text(model.rows(), 0), QStringLiteral("Probe"));
    QTest::qWait(50);
    QCOMPARE(model.dictionary_type(), QStringLiteral("Frequency"));
    QCOMPARE(row_text(model.rows(), 0), QStringLiteral("Probe"));
    QCOMPARE(changed.count(), 2);
}

// A command's newer resolved state and stored config cannot be replaced by the
// initial read that was already in flight.
void DictionarySettingsViewModelTest::a_late_open_result_cannot_override_a_mutation() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    DictionarySettingsViewModel model;
    QSignalSpy changed(&model, &DictionarySettingsViewModel::rowsChanged);
    model.openAsync();
    QTRY_COMPARE(model.rows().size(), qsizetype(1));

    model.openAsync();
    model.setEnabled(QStringLiteral("Probe"), false);
    QVERIFY(!row_enabled(model.rows(), 0));
    QTest::qWait(50);
    QVERIFY(!row_enabled(model.rows(), 0));
    const auto config = saved_config("default-ja");
    QVERIFY(config.has_value());
    QVERIFY(!config->term_dictionaries.front().is_enabled);
    QCOMPARE(changed.count(), 2);
}

// B2: rows are the active profile's resolved list for the selected type.
void DictionarySettingsViewModelTest::rows_are_the_selected_types_resolved_list() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    DictionarySettingsViewModel model;
    model.refresh();

    QCOMPARE(model.dictionary_type(), QStringLiteral("Term"));
    QCOMPARE(model.rows().size(), qsizetype(1));
    QCOMPARE(model.rows().at(0).toMap(),
             (QVariantMap{{"title", QStringLiteral("Probe")},
                          {"enabled", true},
                          {"category", QStringLiteral("none")}}));

    model.selectType(QStringLiteral("Frequency"));
    QCOMPARE(model.dictionary_type(), QStringLiteral("Frequency"));
    QCOMPARE(model.rows().size(), qsizetype(1));
    QCOMPARE(row_text(model.rows(), 0), QStringLiteral("Probe"));
    QVERIFY(row_enabled(model.rows(), 0));

    model.selectType(QStringLiteral("Nope"));
    QCOMPARE(model.dictionary_type(), QStringLiteral("Frequency"));

    model.selectType(QStringLiteral("Pitch"));
    QVERIFY(model.rows().isEmpty());
    model.selectType(QStringLiteral("Kanji"));
    QVERIFY(model.rows().isEmpty());
}

// B3: the enable flag changes in the selected list only and is saved.
void DictionarySettingsViewModelTest::enabling_a_title_writes_the_config() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    DictionarySettingsViewModel model;
    model.refresh();

    model.setEnabled(QStringLiteral("Probe"), false);
    const auto config = saved_config("default-ja");
    QVERIFY(config.has_value());
    QCOMPARE(config->term_dictionaries.size(), std::size_t(1));
    QCOMPARE(config->term_dictionaries.front().file_name, std::string("Probe"));
    QCOMPARE(config->term_dictionaries.front().is_enabled, false);
    QCOMPARE(config->term_dictionaries.front().order, std::int64_t(0));
    QCOMPARE(config->frequency_dictionaries.size(), std::size_t(1));
    QCOMPARE(config->frequency_dictionaries.front().is_enabled, true);
    QCOMPARE(row_enabled(model.rows(), 0), false);

    model.setEnabled(QStringLiteral("Probe"), true);
    const auto enabled = saved_config("default-ja");
    QVERIFY(enabled.has_value());
    QVERIFY(enabled->term_dictionaries.front().is_enabled);
    QVERIFY(row_enabled(model.rows(), 0));
}

// B4: only term entries carry a category.
void DictionarySettingsViewModelTest::a_category_change_is_term_only() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    DictionarySettingsViewModel model;
    model.refresh();

    model.setCategory(QStringLiteral("Probe"), QStringLiteral("monolingual"));
    auto config = saved_config_object("default-ja");
    QVERIFY(config.has_value());
    QCOMPARE(
        config->value("termDictionaries").toArray().at(0).toObject().value("category").toString(),
        QStringLiteral("monolingual"));

    model.selectType(QStringLiteral("Frequency"));
    model.setCategory(QStringLiteral("Probe"), QStringLiteral("bilingual"));
    config = saved_config_object("default-ja");
    QVERIFY(config.has_value());
    QCOMPARE(
        config->value("termDictionaries").toArray().at(0).toObject().value("category").toString(),
        QStringLiteral("monolingual"));
    QVERIFY(
        !config->value("frequencyDictionaries").toArray().at(0).toObject().contains("category"));
}

// B5: with Probe configured, importing Second appends it at order 1;
// destinationOffset moves Probe after Second and the selected list is
// renumbered; a move onto itself changes nothing.
void DictionarySettingsViewModelTest::moving_reorders_and_renumbers_the_selected_list() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    const auto second_zip = path_in(fixture.data, "second.zip");
    write_stored_zip(second_zip,
                     {{"index.json", minimal_index(QStringLiteral("Second"))},
                      {"term_bank_1.json", kSecondTermBank}});

    const auto default_directory = profile_directory("default-ja");
    QVERIFY(!default_directory.empty());
    QVERIFY(write_file(
        default_directory / "dictionary_config.json",
        R"({"termDictionaries":[{"fileName":"Probe","isEnabled":true,"order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})"));

    DictionarySettingsViewModel model;
    model.refresh();
    model.importFiles({QUrl::fromLocalFile(QString::fromStdString(second_zip.string()))});

    QCOMPARE(model.rows().size(), qsizetype(2));
    QCOMPARE(row_text(model.rows(), 0), QStringLiteral("Probe"));
    QCOMPARE(row_text(model.rows(), 1), QStringLiteral("Second"));
    QVERIFY(row_enabled(model.rows(), 0));
    QVERIFY(row_enabled(model.rows(), 1));
    auto imported = saved_config("default-ja");
    QVERIFY(imported.has_value());
    QCOMPARE(imported->term_dictionaries.size(), std::size_t(2));
    QCOMPARE(imported->term_dictionaries.at(0).file_name, std::string("Probe"));
    QCOMPARE(imported->term_dictionaries.at(0).order, std::int64_t(0));
    QVERIFY(imported->term_dictionaries.at(0).is_enabled);
    QCOMPARE(imported->term_dictionaries.at(1).file_name, std::string("Second"));
    QCOMPARE(imported->term_dictionaries.at(1).order, std::int64_t(1));
    QVERIFY(imported->term_dictionaries.at(1).is_enabled);

    model.move(QStringLiteral("Probe"), QStringLiteral("Second"));
    QCOMPARE(model.rows().size(), qsizetype(2));
    QCOMPARE(row_text(model.rows(), 0), QStringLiteral("Second"));
    QCOMPARE(row_text(model.rows(), 1), QStringLiteral("Probe"));
    auto config = saved_config("default-ja");
    QVERIFY(config.has_value());
    QCOMPARE(config->term_dictionaries.size(), std::size_t(2));
    QCOMPARE(config->term_dictionaries.at(0).file_name, std::string("Second"));
    QCOMPARE(config->term_dictionaries.at(0).order, std::int64_t(0));
    QCOMPARE(config->term_dictionaries.at(1).file_name, std::string("Probe"));
    QCOMPARE(config->term_dictionaries.at(1).order, std::int64_t(1));

    const auto before = saved_config_object("default-ja");
    model.move(QStringLiteral("Second"), QStringLiteral("Second"));
    QCOMPARE(saved_config_object("default-ja"), before);
}

// B6: delete removes every reference but only the selected type's directory.
void DictionarySettingsViewModelTest::removing_deletes_only_the_selected_type() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    DictionarySettingsViewModel model;
    model.refresh();

    const auto types = niratan::storage::dictionaries_root();
    QVERIFY(std::filesystem::exists(types / "Term" / "Probe"));
    QVERIFY(std::filesystem::exists(types / "Frequency" / "Probe"));

    model.remove(QStringLiteral("Probe"));

    QVERIFY(!std::filesystem::exists(types / "Term" / "Probe"));
    QVERIFY(std::filesystem::exists(types / "Frequency" / "Probe"));
    QVERIFY(model.rows().isEmpty());

    const auto config = saved_config("default-ja");
    QVERIFY(config.has_value());
    QVERIFY(config->term_dictionaries.empty());
    QCOMPARE(config->frequency_dictionaries.size(), std::size_t(1));
    QCOMPARE(config->frequency_dictionaries.front().file_name, std::string("Probe"));
    QCOMPARE(config->frequency_dictionaries.front().is_enabled, true);
    QCOMPARE(config->frequency_dictionaries.front().order, std::int64_t(0));
}

// B7: a non-default profile disables unconfigured dictionaries until an import
// enables its own.
void DictionarySettingsViewModelTest::importing_into_a_non_default_profile_enables_and_saves() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    ProfileRepository repository{niratan::storage::data_root()};
    const auto english =
        repository.create_profile("English", ContentLanguage::english, std::nullopt);
    QVERIFY(english.profile.has_value());
    QVERIFY(!repository.set_active_profile(english.profile->id).has_value());

    DictionarySettingsViewModel model;
    model.refresh();
    QCOMPARE(model.rows().size(), qsizetype(1));
    QCOMPARE(row_text(model.rows(), 0), QStringLiteral("Probe"));
    QVERIFY(!row_enabled(model.rows(), 0));

    const auto second_zip = path_in(fixture.data, "second.zip");
    write_stored_zip(second_zip,
                     {{"index.json", minimal_index(QStringLiteral("Second"))},
                      {"term_bank_1.json", kSecondTermBank}});
    model.importFiles({QUrl::fromLocalFile(QString::fromStdString(second_zip.string()))});

    const auto config = saved_config(english.profile->id);
    QVERIFY(config.has_value());
    bool found_second = false;
    bool found_probe = false;
    for (const auto& entry : config->term_dictionaries) {
        if (entry.file_name == "Second") {
            found_second = true;
            QVERIFY(entry.is_enabled);
            QCOMPARE(entry.order, std::int64_t(0));
        } else if (entry.file_name == "Probe") {
            found_probe = true;
            QVERIFY(!entry.is_enabled);
            QCOMPARE(entry.order, std::int64_t(0));
        }
    }
    QVERIFY(found_second);
    QVERIFY(found_probe);
    QCOMPARE(model.rows().size(), qsizetype(2));
    bool second_row = false;
    bool probe_row = false;
    for (const QVariant& value : model.rows()) {
        const QVariantMap row = value.toMap();
        if (row.value(QStringLiteral("title")) == QStringLiteral("Second")) {
            second_row = true;
            QVERIFY(row.value(QStringLiteral("enabled")).toBool());
        } else if (row.value(QStringLiteral("title")) == QStringLiteral("Probe")) {
            probe_row = true;
            QVERIFY(!row.value(QStringLiteral("enabled")).toBool());
        }
    }
    QVERIFY(second_row);
    QVERIFY(probe_row);
}

// B8: every failure is collected; a success in the same call is still enabled.
void DictionarySettingsViewModelTest::import_failures_are_reported() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    const auto text_file = path_in(fixture.data, "not-a-dictionary.txt");
    QVERIFY(write_file(text_file, "not a zip"));

    DictionarySettingsViewModel model;
    model.refresh();
    model.importFiles({QUrl::fromLocalFile(QString::fromStdString(text_file.string()))});
    QVERIFY(model.error().contains(QStringLiteral("Failed to import dictionary")));
    QVERIFY(model.error().contains(QStringLiteral("not-a-dictionary.txt")));
    QVERIFY(!saved_config("default-ja").has_value());
    QCOMPARE(model.rows().size(), qsizetype(1));
    QCOMPARE(row_text(model.rows(), 0), QStringLiteral("Probe"));

    const auto second_zip = path_in(fixture.data, "second.zip");
    write_stored_zip(second_zip,
                     {{"index.json", minimal_index(QStringLiteral("Second"))},
                      {"term_bank_1.json", kSecondTermBank}});
    model.importFiles({QUrl::fromLocalFile(QString::fromStdString(second_zip.string())),
                       QUrl::fromLocalFile(QString::fromStdString(text_file.string()))});
    QVERIFY(model.error().contains(QStringLiteral("Some dictionaries could not be imported")));
    QVERIFY(model.error().contains(QStringLiteral("not-a-dictionary.txt")));

    const auto config = saved_config("default-ja");
    QVERIFY(config.has_value());
    bool found_second = false;
    for (const auto& entry : config->term_dictionaries) {
        if (entry.file_name == "Second") {
            found_second = true;
            QVERIFY(entry.is_enabled);
        }
    }
    QVERIFY(found_second);
    QCOMPARE(model.rows().size(), qsizetype(2));
}

// B4: DictionaryView.frequencyDictionaryTitles is the resolved Frequency list
// filtered to enabled entries, in list order, as titles.
void DictionarySettingsViewModelTest::frequency_dictionary_titles_are_the_enabled_titles() {
    Fixture fixture;
    QVERIFY(fixture.imported);
    const auto alpha_zip = path_in(fixture.data, "alpha.zip");
    write_stored_zip(alpha_zip,
                     {{"index.json", minimal_index(QStringLiteral("Alpha"))},
                      {"term_bank_1.json", kSecondTermBank},
                      {"term_meta_bank_1.json", QByteArrayLiteral(R"([["second","freq",50]])")}});
    QVERIFY(install_zip(alpha_zip));
    DictionaryConfig config;
    config.frequency_dictionaries = {DictionaryEntry{"Alpha", true, 0},
                                     DictionaryEntry{"Probe", true, 1}};
    QVERIFY(
        save_dictionary_config(profile_directory("default-ja") / "dictionary_config.json", config));
    DictionarySettingsViewModel model;
    model.refresh();
    QCOMPARE(model.frequencyDictionaryTitles(),
             (QStringList{QStringLiteral("Alpha"), QStringLiteral("Probe")}));
    model.selectType(QStringLiteral("Frequency"));
    model.setEnabled(QStringLiteral("Probe"), false);
    QCOMPARE(model.frequencyDictionaryTitles(), QStringList{QStringLiteral("Alpha")});
    const auto disabled = saved_config("default-ja");
    QVERIFY(disabled.has_value());
    QCOMPARE(disabled->frequency_dictionaries.size(), std::size_t(2));
    QCOMPARE(disabled->frequency_dictionaries.at(1).file_name, std::string("Probe"));
    QCOMPARE(disabled->frequency_dictionaries.at(1).is_enabled, false);
    model.setEnabled(QStringLiteral("Probe"), true);
    QCOMPARE(model.frequencyDictionaryTitles(),
             (QStringList{QStringLiteral("Alpha"), QStringLiteral("Probe")}));
    model.refresh();
    QCOMPARE(model.frequencyDictionaryTitles(),
             (QStringList{QStringLiteral("Alpha"), QStringLiteral("Probe")}));
    model.remove(QStringLiteral("Alpha"));
    QCOMPARE(model.frequencyDictionaryTitles(), QStringList{QStringLiteral("Probe")});
    model.importFiles({QUrl::fromLocalFile(QString::fromStdString(alpha_zip.string()))});
    QCOMPARE(model.frequencyDictionaryTitles(),
             (QStringList{QStringLiteral("Probe"), QStringLiteral("Alpha")}));
}

QTEST_GUILESS_MAIN(DictionarySettingsViewModelTest)
#include "tst_dictionary_settings_view_model.moc"
