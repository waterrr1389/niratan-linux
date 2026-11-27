// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/profiles/dictionary_settings.h"

#include <QJsonObject>
#include <QTest>

#include <optional>
#include <string>

using niratan::profiles::DictionaryProfileSettings;
using niratan::profiles::default_dictionary_settings;
using niratan::profiles::dictionary_settings_from_json;
using niratan::profiles::dictionary_settings_to_json;
using niratan::profiles::resolved_dictionary_settings;

// Models/Profile.swift (DictionaryProfileSettings, defaults) defines the values;
// Core/UserConfig.swift (apply(dictionaryProfileSettings:)) defines the applied
// fallbacks; Core/ProfileSettingsStore.swift defines when defaults are written.
namespace {

std::optional<DictionaryProfileSettings> from_text(const char* text) {
    return dictionary_settings_from_json(QJsonDocument::fromJson(QByteArray(text)));
}

// A document with only the non-optional keys, as written before the optional
// frequency and layout settings existed.
const char* const required_only_document =
    "{\"dictionaryTabDefault\":false,\"scanNonJapaneseText\":true,\"maxResults\":16,"
    "\"scanLength\":16,\"collapseMode\":\"Expand All\",\"expandFirstDictionary\":false,"
    "\"compactGlossaries\":true,\"showExpressionTags\":false,\"harmonicFrequency\":false,"
    "\"deduplicatePitchAccents\":false,\"compactPitchAccents\":true,\"customCSS\":\"\"}";

}  // namespace

class DictionarySettingsTest : public QObject {
    Q_OBJECT

   private slots:
    void the_defaults_match_the_swift_defaults();
    void the_defaults_round_trip();
    void every_field_round_trips();
    void an_engaged_false_optional_is_written();
    void the_document_uses_the_swift_keys();
    void missing_optional_keys_stay_empty();
    void null_optional_keys_stay_empty();
    void a_missing_or_wrong_required_key_is_rejected();
    void a_wrong_optional_type_is_rejected();
    void extra_keys_are_ignored();
    void resolved_settings_fill_the_missing_optionals();
    void resolved_settings_keep_present_values();
    void resolved_settings_replace_unknown_raw_values();
};

void DictionarySettingsTest::the_defaults_match_the_swift_defaults() {
    const DictionaryProfileSettings defaults = default_dictionary_settings();

    QVERIFY(!defaults.dictionary_tab_default);
    QVERIFY(defaults.scan_non_japanese_text);
    QCOMPARE(defaults.max_results, 16);
    QCOMPARE(defaults.scan_length, 16);
    QCOMPARE(defaults.collapse_mode, std::string("Expand All"));
    QVERIFY(!defaults.expand_first_dictionary);
    QVERIFY(defaults.two_column_layout.has_value());
    QCOMPARE(*defaults.two_column_layout, false);
    QVERIFY(defaults.compact_glossaries);
    QVERIFY(!defaults.show_expression_tags);
    QVERIFY(!defaults.harmonic_frequency);
    QVERIFY(!defaults.deduplicate_pitch_accents);
    QVERIFY(defaults.compact_pitch_accents);
    QVERIFY(defaults.custom_css.empty());
    QVERIFY(defaults.frequency_sort_order.has_value());
    QCOMPARE(*defaults.frequency_sort_order, std::string("Auto"));
    QVERIFY(defaults.frequency_sort_dictionary.has_value());
    QVERIFY(defaults.frequency_sort_dictionary->empty());
    QVERIFY(defaults.search_text_size.has_value());
    QCOMPARE(*defaults.search_text_size, 26);
}

void DictionarySettingsTest::the_defaults_round_trip() {
    const DictionaryProfileSettings defaults = default_dictionary_settings();
    const auto decoded = dictionary_settings_from_json(dictionary_settings_to_json(defaults));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, defaults);
}

void DictionarySettingsTest::every_field_round_trips() {
    DictionaryProfileSettings settings = default_dictionary_settings();
    settings.dictionary_tab_default = true;
    settings.scan_non_japanese_text = false;
    settings.max_results = 8;
    settings.scan_length = 32;
    settings.collapse_mode = "Custom";
    settings.expand_first_dictionary = true;
    settings.two_column_layout = false;
    settings.compact_glossaries = false;
    settings.show_expression_tags = true;
    settings.harmonic_frequency = true;
    settings.deduplicate_pitch_accents = true;
    settings.compact_pitch_accents = false;
    settings.custom_css = ".entry { color: red; }";
    settings.frequency_sort_order = "Descending";
    settings.frequency_sort_dictionary = "JPDB";
    settings.search_text_size = 30;

    const auto decoded = dictionary_settings_from_json(dictionary_settings_to_json(settings));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, settings);
}

void DictionarySettingsTest::an_engaged_false_optional_is_written() {
    DictionaryProfileSettings settings = default_dictionary_settings();
    settings.two_column_layout = false;

    const QJsonObject object = dictionary_settings_to_json(settings).object();
    QVERIFY(object.contains("twoColumnLayout"));
    QVERIFY(object.value("twoColumnLayout").isBool());
    QCOMPARE(object.value("twoColumnLayout").toBool(true), false);
}

void DictionarySettingsTest::the_document_uses_the_swift_keys() {
    const QJsonObject object = dictionary_settings_to_json(default_dictionary_settings()).object();

    QVERIFY(object.contains("dictionaryTabDefault"));
    QVERIFY(object.contains("scanNonJapaneseText"));
    QVERIFY(object.contains("maxResults"));
    QVERIFY(object.contains("scanLength"));
    QVERIFY(object.contains("collapseMode"));
    QVERIFY(object.contains("expandFirstDictionary"));
    QVERIFY(object.contains("twoColumnLayout"));
    QVERIFY(object.contains("compactGlossaries"));
    QVERIFY(object.contains("showExpressionTags"));
    QVERIFY(object.contains("harmonicFrequency"));
    QVERIFY(object.contains("deduplicatePitchAccents"));
    QVERIFY(object.contains("compactPitchAccents"));
    QVERIFY(object.contains("customCSS"));
    QVERIFY(object.contains("frequencySortOrder"));
    QVERIFY(object.contains("frequencySortDictionary"));
    QVERIFY(object.contains("searchTextSize"));

    QCOMPARE(object.value("maxResults").toInt(), 16);
    QCOMPARE(object.value("collapseMode").toString(), QString("Expand All"));
    QCOMPARE(object.value("frequencySortOrder").toString(), QString("Auto"));
    QCOMPARE(object.value("searchTextSize").toInt(), 26);
}

void DictionarySettingsTest::missing_optional_keys_stay_empty() {
    const auto settings = from_text(required_only_document);

    QVERIFY(settings.has_value());
    QVERIFY(!settings->two_column_layout.has_value());
    QVERIFY(!settings->frequency_sort_order.has_value());
    QVERIFY(!settings->frequency_sort_dictionary.has_value());
    QVERIFY(!settings->search_text_size.has_value());
    QCOMPARE(settings->max_results, 16);
    QCOMPARE(settings->collapse_mode, std::string("Expand All"));
}

void DictionarySettingsTest::null_optional_keys_stay_empty() {
    const auto settings = from_text(
        "{\"dictionaryTabDefault\":false,\"scanNonJapaneseText\":true,\"maxResults\":16,"
        "\"scanLength\":16,\"collapseMode\":\"Expand All\",\"expandFirstDictionary\":false,"
        "\"twoColumnLayout\":null,\"compactGlossaries\":true,\"showExpressionTags\":false,"
        "\"harmonicFrequency\":false,\"deduplicatePitchAccents\":false,"
        "\"compactPitchAccents\":true,\"customCSS\":\"\",\"frequencySortOrder\":null,"
        "\"frequencySortDictionary\":null,\"searchTextSize\":null}");

    QVERIFY(settings.has_value());
    QVERIFY(!settings->two_column_layout.has_value());
    QVERIFY(!settings->frequency_sort_order.has_value());
    QVERIFY(!settings->frequency_sort_dictionary.has_value());
    QVERIFY(!settings->search_text_size.has_value());
}

void DictionarySettingsTest::a_missing_or_wrong_required_key_is_rejected() {
    QVERIFY(!from_text("{}").has_value());
    QVERIFY(!from_text("[]").has_value());
    QVERIFY(from_text(required_only_document).has_value());

    QJsonObject object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.remove("maxResults");
    QVERIFY(!dictionary_settings_from_json(QJsonDocument(object)).has_value());

    object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.insert("scanNonJapaneseText", "yes");
    QVERIFY(!dictionary_settings_from_json(QJsonDocument(object)).has_value());

    object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.insert("maxResults", "many");
    QVERIFY(!dictionary_settings_from_json(QJsonDocument(object)).has_value());

    object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.insert("collapseMode", 5);
    QVERIFY(!dictionary_settings_from_json(QJsonDocument(object)).has_value());

    object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.insert("customCSS", false);
    QVERIFY(!dictionary_settings_from_json(QJsonDocument(object)).has_value());

    object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.insert("collapseMode", QJsonValue::Null);
    QVERIFY(!dictionary_settings_from_json(QJsonDocument(object)).has_value());
}

void DictionarySettingsTest::a_wrong_optional_type_is_rejected() {
    QVERIFY(from_text(required_only_document).has_value());

    QJsonObject object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.insert("twoColumnLayout", "yes");
    QVERIFY(!dictionary_settings_from_json(QJsonDocument(object)).has_value());

    object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.insert("searchTextSize", "large");
    QVERIFY(!dictionary_settings_from_json(QJsonDocument(object)).has_value());

    object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.insert("frequencySortDictionary", 1);
    QVERIFY(!dictionary_settings_from_json(QJsonDocument(object)).has_value());
}

void DictionarySettingsTest::extra_keys_are_ignored() {
    QJsonObject object = dictionary_settings_to_json(default_dictionary_settings()).object();
    object.insert("unexpected", true);

    const auto settings = dictionary_settings_from_json(QJsonDocument(object));
    QVERIFY(settings.has_value());
    QCOMPARE(*settings, default_dictionary_settings());
}

void DictionarySettingsTest::resolved_settings_fill_the_missing_optionals() {
    const auto stored = from_text(required_only_document);
    QVERIFY(stored.has_value());

    const DictionaryProfileSettings resolved = resolved_dictionary_settings(*stored);

    QVERIFY(resolved.two_column_layout.has_value());
    QCOMPARE(*resolved.two_column_layout, false);
    QVERIFY(resolved.frequency_sort_order.has_value());
    QCOMPARE(*resolved.frequency_sort_order, std::string("Auto"));
    QVERIFY(resolved.frequency_sort_dictionary.has_value());
    QVERIFY(resolved.frequency_sort_dictionary->empty());
    QVERIFY(resolved.search_text_size.has_value());
    QCOMPARE(*resolved.search_text_size, 26);
}

void DictionarySettingsTest::resolved_settings_keep_present_values() {
    DictionaryProfileSettings settings = default_dictionary_settings();
    settings.two_column_layout = true;
    settings.frequency_sort_order = "Ascending";
    settings.frequency_sort_dictionary = "JMdict";
    settings.search_text_size = 32;
    settings.collapse_mode = "Collapse All";

    const DictionaryProfileSettings resolved = resolved_dictionary_settings(settings);

    QVERIFY(resolved.two_column_layout.has_value());
    QCOMPARE(*resolved.two_column_layout, true);
    QVERIFY(resolved.frequency_sort_order.has_value());
    QCOMPARE(*resolved.frequency_sort_order, std::string("Ascending"));
    QVERIFY(resolved.frequency_sort_dictionary.has_value());
    QCOMPARE(*resolved.frequency_sort_dictionary, std::string("JMdict"));
    QVERIFY(resolved.search_text_size.has_value());
    QCOMPARE(*resolved.search_text_size, 32);
    QCOMPARE(resolved.collapse_mode, std::string("Collapse All"));
    QCOMPARE(resolved, settings);
}

void DictionarySettingsTest::resolved_settings_replace_unknown_raw_values() {
    DictionaryProfileSettings settings = default_dictionary_settings();
    settings.collapse_mode = "Sideways";
    settings.frequency_sort_order = "Sideways";

    const DictionaryProfileSettings resolved = resolved_dictionary_settings(settings);

    QCOMPARE(resolved.collapse_mode, std::string("Expand All"));
    QVERIFY(resolved.frequency_sort_order.has_value());
    QCOMPARE(*resolved.frequency_sort_order, std::string("Auto"));

    settings.collapse_mode = "Custom";
    settings.frequency_sort_order = "Descending";
    const DictionaryProfileSettings kept = resolved_dictionary_settings(settings);
    QCOMPARE(kept.collapse_mode, std::string("Custom"));
    QVERIFY(kept.frequency_sort_order.has_value());
    QCOMPARE(*kept.frequency_sort_order, std::string("Descending"));
}

QTEST_APPLESS_MAIN(DictionarySettingsTest)
#include "tst_dictionary_settings.moc"
