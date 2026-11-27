// SPDX-License-Identifier: GPL-3.0-or-later
//
// The lookup configuration a profile supplies, following
// Core/LookupEngine.swift (LookupFrequencySortOrder, QueryConfiguration,
// buildQuery), Core/DictionaryManager.swift
// (applyActiveProfileDictionaryConfiguration, refreshLookupQueryIfNeeded,
// activeLanguage) and Core/UserConfig.swift (apply(dictionaryProfileSettings:),
// the frequencySortOrder and frequencySortDictionary didSet observers).

#include "services/dictionary/lookup_configuration.h"

#include "../viewmodels/data_root.h"
#include "yomitan_fixture.h"

#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <string>

#include "models/dictionary/dictionary_config.h"
#include "models/profiles/dictionary_settings.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

using niratan::dictionary::active_lookup_configuration;
using niratan::dictionary::DictionaryType;
using niratan::dictionary::FrequencySortOrder;
using niratan::dictionary::LookupConfiguration;
using niratan::dictionary::parse_frequency_sort_order;
using niratan::profiles::ContentLanguage;
using niratan::profiles::default_dictionary_settings;
using niratan::profiles::DictionaryProfileSettings;
using niratan::profiles::ProfileRepository;
using niratan::profiles::resolved_dictionary_settings;
using niratan::test::minimal_index;
using niratan::test::TempDataRoot;

namespace {

bool write_dictionary(DictionaryType type, const QString& title) {
    const auto directory = niratan::storage::dictionary_type_directory(type) / title.toStdString();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return false;
    }
    QFile file(QString::fromStdString((directory / "index.json").string()));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray bytes = minimal_index(title);
    return file.write(bytes) == bytes.size();
}

bool write_file(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

}  // namespace

class LookupConfigurationTest : public QObject {
    Q_OBJECT

   private slots:
    void fallback_lists_every_installed_dictionary_of_the_default_profile();
    void the_active_profile_configuration_selects_enabled_dictionaries();
    void frequency_sort_settings_parse_like_swift();
};

// B1: no usable config lists one installed dictionary per type, enabled for
// the default profile.
void LookupConfigurationTest::fallback_lists_every_installed_dictionary_of_the_default_profile() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    QVERIFY(write_dictionary(DictionaryType::term, QStringLiteral("T")));
    QVERIFY(write_dictionary(DictionaryType::frequency, QStringLiteral("F")));
    QVERIFY(write_dictionary(DictionaryType::pitch, QStringLiteral("P")));
    QVERIFY(write_dictionary(DictionaryType::kanji, QStringLiteral("K")));

    ProfileRepository repository(niratan::storage::data_root());
    const LookupConfiguration configuration =
        active_lookup_configuration(repository, default_dictionary_settings());

    QCOMPARE(configuration.term_dictionaries.size(), std::size_t(1));
    QVERIFY(configuration.term_dictionaries.front() ==
            niratan::storage::dictionary_type_directory(DictionaryType::term) / "T");
    QCOMPARE(configuration.frequency_dictionaries.size(), std::size_t(1));
    QVERIFY(configuration.frequency_dictionaries.front() ==
            niratan::storage::dictionary_type_directory(DictionaryType::frequency) / "F");
    QCOMPARE(configuration.pitch_dictionaries.size(), std::size_t(1));
    QVERIFY(configuration.pitch_dictionaries.front() ==
            niratan::storage::dictionary_type_directory(DictionaryType::pitch) / "P");
    QCOMPARE(configuration.kanji_dictionaries.size(), std::size_t(1));
    QVERIFY(configuration.kanji_dictionaries.front() ==
            niratan::storage::dictionary_type_directory(DictionaryType::kanji) / "K");
    QCOMPARE(configuration.language_id, std::string("ja"));
    QCOMPARE(configuration.frequency_sort_order, FrequencySortOrder::automatic);
    QVERIFY(configuration.frequency_sort_dictionary.empty());
}

// B1: a profile's config selects enabled installed dictionaries in its order;
// disabled, missing and unconfigured entries are left out.
void LookupConfigurationTest::the_active_profile_configuration_selects_enabled_dictionaries() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    QVERIFY(write_dictionary(DictionaryType::term, QStringLiteral("A")));
    QVERIFY(write_dictionary(DictionaryType::term, QStringLiteral("B")));
    QVERIFY(write_dictionary(DictionaryType::frequency, QStringLiteral("F")));
    QVERIFY(write_dictionary(DictionaryType::pitch, QStringLiteral("P")));
    QVERIFY(write_dictionary(DictionaryType::kanji, QStringLiteral("K")));

    ProfileRepository repository(niratan::storage::data_root());
    const auto created =
        repository.create_profile("English", ContentLanguage::english, std::nullopt);
    QVERIFY(created.profile.has_value());
    QVERIFY(!repository.set_active_profile(created.profile->id).has_value());
    const auto directory = repository.profile_directory(created.profile->id);
    QVERIFY(directory.has_value());
    QVERIFY(write_file(
        *directory / "dictionary_config.json",
        R"({"termDictionaries":[{"fileName":"B","isEnabled":true,"order":0},{"fileName":"A","isEnabled":false,"order":1},{"fileName":"Missing","isEnabled":true,"order":2}],"frequencyDictionaries":[{"fileName":"F","isEnabled":false,"order":0}],"pitchDictionaries":[]})"));

    const LookupConfiguration configuration =
        active_lookup_configuration(repository, default_dictionary_settings());

    QCOMPARE(configuration.term_dictionaries.size(), std::size_t(1));
    QVERIFY(configuration.term_dictionaries.front() ==
            niratan::storage::dictionary_type_directory(DictionaryType::term) / "B");
    QVERIFY(configuration.frequency_dictionaries.empty());
    QVERIFY(configuration.pitch_dictionaries.empty());
    QVERIFY(configuration.kanji_dictionaries.empty());
    QCOMPARE(configuration.language_id, std::string("en"));
}

// B6: only Ascending and Descending select an order; the profile's resolved
// settings carry both sort values.
void LookupConfigurationTest::frequency_sort_settings_parse_like_swift() {
    QCOMPARE(parse_frequency_sort_order("Ascending"), FrequencySortOrder::ascending);
    QCOMPARE(parse_frequency_sort_order("Descending"), FrequencySortOrder::descending);
    QCOMPARE(parse_frequency_sort_order("Auto"), FrequencySortOrder::automatic);
    QCOMPARE(parse_frequency_sort_order(""), FrequencySortOrder::automatic);
    QCOMPARE(parse_frequency_sort_order("sideways"), FrequencySortOrder::automatic);

    TempDataRoot data;
    QVERIFY(data.isValid());
    ProfileRepository repository(niratan::storage::data_root());

    DictionaryProfileSettings settings = default_dictionary_settings();
    settings.frequency_sort_order = std::string("Descending");
    settings.frequency_sort_dictionary = std::string("Jiten");
    const LookupConfiguration configured =
        active_lookup_configuration(repository, resolved_dictionary_settings(settings));
    QCOMPARE(configured.frequency_sort_order, FrequencySortOrder::descending);
    QCOMPARE(configured.frequency_sort_dictionary, std::string("Jiten"));

    const LookupConfiguration absent =
        active_lookup_configuration(repository, DictionaryProfileSettings{});
    QCOMPARE(absent.frequency_sort_order, FrequencySortOrder::automatic);
    QVERIFY(absent.frequency_sort_dictionary.empty());
}

QTEST_APPLESS_MAIN(LookupConfigurationTest)
#include "tst_lookup_configuration.moc"
