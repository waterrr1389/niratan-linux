// SPDX-License-Identifier: GPL-3.0-or-later
//
// The per-profile dictionary configuration, following Models/Dictionary.swift
// (DictionaryConfig, DictionaryConfig.DictionaryEntry, DictionaryCategory,
// DictionaryInfo) and Core/DictionaryManager.swift (collectDictionaries,
// applyActiveProfileDictionaryConfiguration, loadDictionaryConfig,
// saveDictionaryConfig, loadCollapsedDictionaries, saveCollapsedDictionaries,
// toggleCollapsedDictionary).

#include "models/dictionary/dictionary_config.h"
#include "services/dictionary/dictionary_config_store.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <set>
#include <string>
#include <vector>

using niratan::anki::DictionaryCategory;
using niratan::dictionary::collect_dictionaries;
using niratan::dictionary::decode_dictionary_config;
using niratan::dictionary::DictionaryConfig;
using niratan::dictionary::DictionaryEntry;
using niratan::dictionary::DictionaryInfo;
using niratan::dictionary::DictionaryType;
using niratan::dictionary::load_collapsed_dictionaries;
using niratan::dictionary::load_dictionary_config;
using niratan::dictionary::resolved_dictionary_list;
using niratan::dictionary::save_collapsed_dictionaries;
using niratan::dictionary::save_dictionary_config;
using niratan::dictionary::toggle_collapsed_dictionary;

namespace {

std::filesystem::path path_in(const QTemporaryDir& root, const std::string& name) {
    return std::filesystem::path(root.path().toStdString()) / name;
}

bool write_bytes(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray read_bytes(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

DictionaryInfo installed(const std::string& title) {
    return DictionaryInfo{std::filesystem::path("/dictionaries/Term") / title, title};
}

DictionaryEntry entry(std::string file_name, std::int64_t order,
                      DictionaryCategory category = DictionaryCategory::none,
                      bool is_enabled = true) {
    return DictionaryEntry{std::move(file_name), is_enabled, order, category};
}

QByteArray config_with_terms(const QByteArray& terms) {
    return QByteArray(R"({"termDictionaries":)") + terms +
        QByteArray(R"(,"frequencyDictionaries":[],"pitchDictionaries":[]})");
}

}  // namespace

class DictionaryConfigTest : public QObject {
    Q_OBJECT

   private slots:
    void valid_config_decodes();
    void category_values_decode_data();
    void category_values_decode();
    void invalid_documents_are_rejected_data();
    void invalid_documents_are_rejected();
    void collection_orders_by_config();
    void collection_keeps_equal_orders_and_duplicates();
    void fallback_lists_installed_dictionaries();
    void saving_writes_the_four_arrays();
    void collapsed_dictionaries_round_trip();
};

// B3: the four arrays, the optional kanji array and the entry fields.
void DictionaryConfigTest::valid_config_decodes() {
    const auto config = decode_dictionary_config(
        R"({"termDictionaries":[{"fileName":"A","isEnabled":true,"order":3,"category":"monolingual"}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QVERIFY(config.has_value());
    QVERIFY(!config->kanji_dictionaries.has_value());
    QCOMPARE(config->term_dictionaries.size(), std::size_t(1));
    QCOMPARE(config->term_dictionaries.front(),
             (DictionaryEntry{"A", true, 3, DictionaryCategory::monolingual}));
    QVERIFY(config->frequency_dictionaries.empty());
    QVERIFY(config->pitch_dictionaries.empty());

    const auto with_kanji = decode_dictionary_config(
        R"({"termDictionaries":[],"frequencyDictionaries":[],"pitchDictionaries":[],"kanjiDictionaries":[]})");
    QVERIFY(with_kanji.has_value());
    QVERIFY(with_kanji->kanji_dictionaries.has_value());
    QVERIFY(with_kanji->kanji_dictionaries->empty());

    const auto null_kanji = decode_dictionary_config(
        R"({"termDictionaries":[],"frequencyDictionaries":[],"pitchDictionaries":[],"kanjiDictionaries":null})");
    QVERIFY(null_kanji.has_value());
    QVERIFY(!null_kanji->kanji_dictionaries.has_value());
}

// B3: every DictionaryCategory raw value, plus absent and null meaning none.
void DictionaryConfigTest::category_values_decode_data() {
    QTest::addColumn<QByteArray>("category_json");
    QTest::addColumn<DictionaryCategory>("expected");
    QTest::newRow("none") << QByteArray(R"("category":"none")") << DictionaryCategory::none;
    QTest::newRow("monolingual") << QByteArray(R"("category":"monolingual")")
                                 << DictionaryCategory::monolingual;
    QTest::newRow("bilingual") << QByteArray(R"("category":"bilingual")")
                               << DictionaryCategory::bilingual;
    QTest::newRow("exclude") << QByteArray(R"("category":"exclude")")
                             << DictionaryCategory::exclude;
    QTest::newRow("absent") << QByteArray("") << DictionaryCategory::none;
    QTest::newRow("null") << QByteArray(R"("category":null)") << DictionaryCategory::none;
}

void DictionaryConfigTest::category_values_decode() {
    QFETCH(QByteArray, category_json);
    QFETCH(DictionaryCategory, expected);
    QByteArray entry_json = R"({"fileName":"A","isEnabled":true,"order":0)";
    if (!category_json.isEmpty()) {
        entry_json += QByteArray(",") + category_json;
    }
    entry_json += "}";
    const auto config = decode_dictionary_config(config_with_terms("[" + entry_json + "]"));
    QVERIFY(config.has_value());
    QCOMPARE(config->term_dictionaries.size(), std::size_t(1));
    QCOMPARE(config->term_dictionaries.front().category, expected);
}

// B3: a bad document rejects the whole config, as Codable decoding does.
void DictionaryConfigTest::invalid_documents_are_rejected_data() {
    QTest::addColumn<QByteArray>("document");
    QTest::newRow("bad json") << QByteArray("{");
    QTest::newRow("not an object") << QByteArray("[]");
    QTest::newRow("missing frequency")
        << QByteArray(R"({"termDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("missing pitch")
        << QByteArray(R"({"termDictionaries":[],"frequencyDictionaries":[]})");
    QTest::newRow("missing terms")
        << QByteArray(R"({"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("entry without fileName") << QByteArray(
        R"({"termDictionaries":[{"isEnabled":true,"order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("entry without isEnabled") << QByteArray(
        R"({"termDictionaries":[{"fileName":"A","order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("entry without order") << QByteArray(
        R"({"termDictionaries":[{"fileName":"A","isEnabled":true}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("terms not an array") << QByteArray(
        R"({"termDictionaries":{},"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("fileName not a string") << QByteArray(
        R"({"termDictionaries":[{"fileName":1,"isEnabled":true,"order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("isEnabled not a bool") << QByteArray(
        R"({"termDictionaries":[{"fileName":"A","isEnabled":"true","order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("fractional order") << QByteArray(
        R"({"termDictionaries":[{"fileName":"A","isEnabled":true,"order":1.5}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("order not a number") << QByteArray(
        R"({"termDictionaries":[{"fileName":"A","isEnabled":true,"order":"1"}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("unknown category") << QByteArray(
        R"({"termDictionaries":[{"fileName":"A","isEnabled":true,"order":0,"category":"unknown"}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("kanji order is invalid") << QByteArray(
        R"({"termDictionaries":[],"frequencyDictionaries":[],"pitchDictionaries":[],"kanjiDictionaries":[{"fileName":"K","isEnabled":true,"order":1.5}]})");
}

void DictionaryConfigTest::invalid_documents_are_rejected() {
    QFETCH(QByteArray, document);
    QVERIFY(!decode_dictionary_config(document).has_value());
}

// B4: configured entries by order, then the remaining installed ones.
void DictionaryConfigTest::collection_orders_by_config() {
    const std::vector<DictionaryInfo> stored = {installed("A"), installed("B"), installed("C"),
                                                installed("D")};
    const std::vector<DictionaryEntry> configured = {
        entry("Missing", 0, DictionaryCategory::bilingual),
        entry("C", 1, DictionaryCategory::monolingual, false),
        entry("B", 2, DictionaryCategory::monolingual),
        entry("A", 3, DictionaryCategory::bilingual),
    };

    const auto collected = collect_dictionaries(stored, configured, false);
    QCOMPARE(collected.size(), std::size_t(4));
    QCOMPARE(collected.at(0).title, std::string("C"));
    QCOMPARE(collected.at(0).is_enabled, false);
    QCOMPARE(collected.at(0).order, std::int64_t(1));
    QCOMPARE(collected.at(0).category, DictionaryCategory::monolingual);
    QCOMPARE(collected.at(1).title, std::string("B"));
    QCOMPARE(collected.at(1).is_enabled, true);
    QCOMPARE(collected.at(1).order, std::int64_t(2));
    QCOMPARE(collected.at(1).category, DictionaryCategory::monolingual);
    QCOMPARE(collected.at(2).title, std::string("A"));
    QCOMPARE(collected.at(2).is_enabled, true);
    QCOMPARE(collected.at(2).order, std::int64_t(3));
    QCOMPARE(collected.at(2).category, DictionaryCategory::bilingual);
    QCOMPARE(collected.at(3).title, std::string("D"));
    QCOMPARE(collected.at(3).is_enabled, false);
    QCOMPARE(collected.at(3).order, std::int64_t(3));
    QCOMPARE(collected.at(3).category, DictionaryCategory::none);
}

// B4: equal orders keep config order, duplicates each produce a result, and
// enable_unconfigured controls the trailing entries.
void DictionaryConfigTest::collection_keeps_equal_orders_and_duplicates() {
    const std::vector<DictionaryInfo> stored = {installed("A"), installed("B"), installed("C"),
                                                installed("D")};
    const std::vector<DictionaryEntry> equal = {
        entry("C", 0, DictionaryCategory::monolingual),
        entry("A", 0, DictionaryCategory::monolingual),
        entry("B", 0, DictionaryCategory::monolingual),
    };
    const auto collected = collect_dictionaries(stored, equal, false);
    QCOMPARE(collected.size(), std::size_t(4));
    QCOMPARE(collected.at(0).title, std::string("C"));
    QCOMPARE(collected.at(1).title, std::string("A"));
    QCOMPARE(collected.at(2).title, std::string("B"));
    QCOMPARE(collected.at(3).title, std::string("D"));
    QCOMPARE(collected.at(3).order, std::int64_t(3));
    QCOMPARE(collected.at(3).is_enabled, false);

    const std::vector<DictionaryEntry> duplicate = {entry("A", 0), entry("A", 1)};
    const auto twice = collect_dictionaries({installed("A"), installed("B")}, duplicate, false);
    QCOMPARE(twice.size(), std::size_t(3));
    QCOMPARE(twice.at(0).title, std::string("A"));
    QCOMPARE(twice.at(1).title, std::string("A"));
    QCOMPARE(twice.at(2).title, std::string("B"));
    QCOMPARE(twice.at(2).order, std::int64_t(2));
    QCOMPARE(twice.at(2).is_enabled, false);

    const auto enabled = collect_dictionaries({installed("A"), installed("B")}, duplicate, true);
    QCOMPARE(enabled.size(), std::size_t(3));
    QCOMPARE(enabled.at(2).is_enabled, true);
}

// B5: no usable config lists every installed dictionary with
// isEnabled = (default profile); a valid empty config does not.
void DictionaryConfigTest::fallback_lists_installed_dictionaries() {
    const std::vector<DictionaryInfo> stored = {installed("A"), installed("B")};

    const auto fallback =
        resolved_dictionary_list(stored, std::nullopt, DictionaryType::term, true);
    QCOMPARE(fallback.size(), std::size_t(2));
    for (const DictionaryInfo& dictionary : fallback) {
        QVERIFY(dictionary.is_enabled);
        QCOMPARE(dictionary.order, std::int64_t(0));
        QCOMPARE(dictionary.category, DictionaryCategory::none);
    }

    const auto other = resolved_dictionary_list(stored, std::nullopt, DictionaryType::term, false);
    QCOMPARE(other.size(), std::size_t(2));
    for (const DictionaryInfo& dictionary : other) {
        QVERIFY(!dictionary.is_enabled);
    }

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto invalid_path = path_in(root, "dictionary_config.json");
    QVERIFY(write_bytes(invalid_path, "{"));
    const auto invalid = load_dictionary_config(invalid_path);
    QVERIFY(!invalid.has_value());
    const auto invalid_fallback =
        resolved_dictionary_list(stored, invalid, DictionaryType::term, true);
    QCOMPARE(invalid_fallback.size(), std::size_t(2));
    QVERIFY(invalid_fallback.at(0).is_enabled);
    QVERIFY(invalid_fallback.at(1).is_enabled);

    const DictionaryConfig empty = {};
    const auto configured = resolved_dictionary_list(stored, empty, DictionaryType::term, true);
    QCOMPARE(configured.size(), std::size_t(2));
    QVERIFY(!configured.at(0).is_enabled);
    QVERIFY(!configured.at(1).is_enabled);
    QCOMPARE(configured.at(0).order, std::int64_t(0));
    QCOMPARE(configured.at(1).order, std::int64_t(1));

    // The type selects its own array: B is configured and enabled, A follows
    // as an unconfigured dictionary of the same frequency scan.
    DictionaryConfig frequency_config = {};
    frequency_config.frequency_dictionaries = {entry("B", 0)};
    const auto frequency =
        resolved_dictionary_list(stored, frequency_config, DictionaryType::frequency, false);
    QCOMPARE(frequency.size(), std::size_t(2));
    QCOMPARE(frequency.at(0).title, std::string("B"));
    QVERIFY(frequency.at(0).is_enabled);
    QCOMPARE(frequency.at(1).title, std::string("A"));
    QVERIFY(!frequency.at(1).is_enabled);
}

// B6: four arrays, category omitted for none and never on kanji entries.
void DictionaryConfigTest::saving_writes_the_four_arrays() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto config_path = std::filesystem::path(root.path().toStdString()) / "Profiles" / "p1" /
        "dictionary_config.json";

    DictionaryConfig config;
    config.term_dictionaries = {entry("A", 0, DictionaryCategory::monolingual),
                                entry("B", 5, DictionaryCategory::none, false)};
    config.frequency_dictionaries = {entry("F", 0)};
    config.pitch_dictionaries = {};
    config.kanji_dictionaries = std::vector<DictionaryEntry>{entry("K", 0)};

    QVERIFY(save_dictionary_config(config_path, config));
    QVERIFY(std::filesystem::exists(config_path));

    std::size_t siblings = 0;
    for (const auto& ignored : std::filesystem::directory_iterator(config_path.parent_path())) {
        static_cast<void>(ignored);
        ++siblings;
    }
    QCOMPARE(siblings, std::size_t(1));
    QVERIFY(!std::filesystem::exists(std::filesystem::path(root.path().toStdString()) /
                                     "Dictionaries" / "config.json"));

    const QJsonDocument document = QJsonDocument::fromJson(read_bytes(config_path));
    QVERIFY(document.isObject());
    const QJsonObject object = document.object();
    const QJsonArray terms = object.value("termDictionaries").toArray();
    const QJsonArray frequencies = object.value("frequencyDictionaries").toArray();
    const QJsonArray pitches = object.value("pitchDictionaries").toArray();
    const QJsonArray kanji = object.value("kanjiDictionaries").toArray();
    QCOMPARE(terms.size(), 2);
    QCOMPARE(frequencies.size(), 1);
    QCOMPARE(pitches.size(), 0);
    QCOMPARE(kanji.size(), 1);

    QCOMPARE(
        terms.at(0).toObject(),
        (QJsonObject{
            {"fileName", "A"}, {"isEnabled", true}, {"order", 0}, {"category", "monolingual"}}));
    QVERIFY(!terms.at(1).toObject().contains("category"));
    QCOMPARE(terms.at(1).toObject().value("order").toInt(), 5);
    QVERIFY(!frequencies.at(0).toObject().contains("category"));
    QVERIFY(!kanji.at(0).toObject().contains("category"));
    QCOMPARE(kanji.at(0).toObject().keys(), (QStringList{"fileName", "isEnabled", "order"}));
}

// B7: collapsed.json is a JSON array of titles; invalid content is empty.
void DictionaryConfigTest::collapsed_dictionaries_round_trip() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto collapsed_path = path_in(root, "collapsed.json");

    QVERIFY(load_collapsed_dictionaries(collapsed_path).empty());

    QVERIFY(write_bytes(collapsed_path, R"(["A","B","A"])"));
    QCOMPARE(load_collapsed_dictionaries(collapsed_path), (std::set<std::string>{"A", "B"}));

    QVERIFY(write_bytes(collapsed_path, R"(["A",1])"));
    QVERIFY(load_collapsed_dictionaries(collapsed_path).empty());

    QVERIFY(write_bytes(collapsed_path, R"({"A":true})"));
    QVERIFY(load_collapsed_dictionaries(collapsed_path).empty());

    QVERIFY(save_collapsed_dictionaries(collapsed_path, {"B", "A"}));
    const QJsonDocument saved = QJsonDocument::fromJson(read_bytes(collapsed_path));
    QVERIFY(saved.isArray());
    std::set<std::string> written;
    for (const QJsonValue& value : saved.array()) {
        QVERIFY(value.isString());
        written.insert(value.toString().toStdString());
    }
    QCOMPARE(written, (std::set<std::string>{"A", "B"}));

    const auto toggled_path = path_in(root, "toggle.json");
    QCOMPARE(toggle_collapsed_dictionary(toggled_path, "A"), (std::set<std::string>{"A"}));
    QCOMPARE(QJsonDocument::fromJson(read_bytes(toggled_path)).array(), QJsonArray{"A"});
    QVERIFY(toggle_collapsed_dictionary(toggled_path, "A").empty());
    QCOMPARE(QJsonDocument::fromJson(read_bytes(toggled_path)).array(), QJsonArray{});
}

QTEST_APPLESS_MAIN(DictionaryConfigTest)
#include "tst_dictionary_config.moc"
