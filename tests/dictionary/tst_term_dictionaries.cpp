// SPDX-License-Identifier: GPL-3.0-or-later
//
// The term dictionary list the popup's mining add uses, following
// Core/DictionaryManager.swift (applyActiveProfileDictionaryConfiguration,
// collectDictionaries, loadDictionaryConfig, getDictionariesFromStorage),
// Models/Dictionary.swift (DictionaryConfig, DictionaryEntry,
// DictionaryCategory) and Core/AnkiManager.swift (categoryGlossary).

#include "services/dictionary/term_dictionaries.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <filesystem>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

using niratan::anki::DictionaryCategory;
using niratan::anki::TermDictionary;
using niratan::dictionary::term_dictionaries;

namespace {

std::filesystem::path base_directory(const QTemporaryDir& root) {
    return std::filesystem::path(root.path().toStdString());
}

QJsonObject entry(const QString& file_name, int order,
                  const QJsonValue& category = QJsonValue::Null, bool is_enabled = true) {
    QJsonObject object{{"fileName", file_name}, {"isEnabled", is_enabled}, {"order", order}};
    object.insert("category", category);
    return object;
}

QJsonObject config(const QJsonArray& terms) {
    return {{"termDictionaries", terms},
            {"frequencyDictionaries", QJsonArray{}},
            {"pitchDictionaries", QJsonArray{}}};
}

bool write_document(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

bool write_config(const std::filesystem::path& path, const QJsonObject& document) {
    return write_document(path, QJsonDocument(document).toJson(QJsonDocument::Compact));
}

// A stored dictionary is readable only when it has a decodable index.json.
std::filesystem::path make_dictionary(const std::filesystem::path& base, const char* name,
                                      const char* index_title = nullptr) {
    const auto directory = base / name;
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const QJsonObject index{
        {"title", QString::fromUtf8(index_title == nullptr ? name : index_title)},
        {"format", 3},
        {"revision", "1"},
        {"isUpdatable", false},
        {"indexUrl", ""},
        {"downloadUrl", ""},
    };
    write_document(directory / "index.json", QJsonDocument(index).toJson(QJsonDocument::Compact));
    return directory;
}

QString title(const TermDictionary& dictionary) {
    return QString::fromStdString(dictionary.title);
}

}  // namespace

class TermDictionariesTest : public QObject {
    Q_OBJECT

   private slots:
    void configured_entries_follow_order_and_keep_disabled_ones();
    void equal_orders_keep_the_config_file_order();
    void an_absent_or_null_category_is_none();
    void the_title_is_the_installed_directory_name();
    void an_unreadable_index_is_left_out();
    void a_missing_config_lists_installed_directories();
    void an_invalid_config_lists_installed_directories_data();
    void an_invalid_config_lists_installed_directories();
};

// B1, B2, B3: collectDictionaries sorts by order, matches fileName to the
// directory name, skips unmatched entries and keeps disabled dictionaries.
void TermDictionariesTest::configured_entries_follow_order_and_keep_disabled_ones() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto base = base_directory(root);
    const auto a = make_dictionary(base, "A");
    const auto b = make_dictionary(base, "B");
    const auto c = make_dictionary(base, "C");
    const auto d = make_dictionary(base, "D");
    const auto path = base / "dictionary_config.json";
    const QJsonArray terms{
        entry("C", 1, "monolingual", false),
        entry("A", 3, "bilingual"),
        entry("Missing", 0, "bilingual"),
        entry("B", 2, "monolingual"),
    };
    QVERIFY(write_config(path, config(terms)));
    const auto entries = term_dictionaries({a, b, c, d}, path);

    QCOMPARE(entries.size(), std::size_t(4));
    QCOMPARE(title(entries.at(0)), QStringLiteral("C"));
    QCOMPARE(entries.at(0).category, DictionaryCategory::monolingual);
    QCOMPARE(title(entries.at(1)), QStringLiteral("B"));
    QCOMPARE(entries.at(1).category, DictionaryCategory::monolingual);
    QCOMPARE(title(entries.at(2)), QStringLiteral("A"));
    QCOMPARE(entries.at(2).category, DictionaryCategory::bilingual);
    QCOMPARE(title(entries.at(3)), QStringLiteral("D"));
    QCOMPARE(entries.at(3).category, DictionaryCategory::none);
}

// B1: equal orders keep their config file order; Swift's order is undefined.
void TermDictionariesTest::equal_orders_keep_the_config_file_order() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto base = base_directory(root);
    const auto a = make_dictionary(base, "A");
    const auto b = make_dictionary(base, "B");
    const auto c = make_dictionary(base, "C");
    const auto path = base / "dictionary_config.json";
    const QJsonArray terms{entry("C", 0, "monolingual"), entry("A", 0, "monolingual"),
                           entry("B", 0, "monolingual")};
    QVERIFY(write_config(path, config(terms)));
    const auto entries = term_dictionaries({a, b, c}, path);

    QCOMPARE(entries.size(), std::size_t(3));
    QCOMPARE(title(entries.at(0)), QStringLiteral("C"));
    QCOMPARE(title(entries.at(1)), QStringLiteral("A"));
    QCOMPARE(title(entries.at(2)), QStringLiteral("B"));
}

// B2: an absent or null category decodes to none.
void TermDictionariesTest::an_absent_or_null_category_is_none() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto base = base_directory(root);
    const auto a = make_dictionary(base, "A");
    const auto b = make_dictionary(base, "B");
    const auto path = base / "dictionary_config.json";
    QJsonObject absent{{"fileName", "A"}, {"isEnabled", true}, {"order", 0}};
    const QJsonArray terms{absent, entry("B", 1)};
    QVERIFY(write_config(path, config(terms)));
    const auto entries = term_dictionaries({a, b}, path);

    QCOMPARE(entries.size(), std::size_t(2));
    for (const auto& dictionary : entries) {
        QCOMPARE(dictionary.category, DictionaryCategory::none);
    }
}

// B8: the matching title is the installed directory name, which is also the
// popup's singleGlossaries key; the index.json title is not used.
void TermDictionariesTest::the_title_is_the_installed_directory_name() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto base = base_directory(root);
    const auto directory = make_dictionary(base, "Title [2026]", "Index Title");
    const auto path = base / "dictionary_config.json";
    const QJsonArray terms{entry(QStringLiteral("Title [2026]"), 0, "exclude")};
    QVERIFY(write_config(path, config(terms)));
    const auto entries = term_dictionaries({directory}, path);

    QCOMPARE(entries.size(), std::size_t(1));
    QCOMPARE(title(entries.at(0)), QStringLiteral("Title [2026]"));
    QCOMPARE(entries.at(0).category, DictionaryCategory::exclude);
}

// B6, B8: a directory whose index.json is missing or unreadable is omitted,
// as getDictionariesFromStorage does, and a config entry naming it is skipped.
void TermDictionariesTest::an_unreadable_index_is_left_out() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto base = base_directory(root);
    const auto a = make_dictionary(base, "A");
    const auto b = base / "B";
    const auto c = base / "C";
    std::error_code error;
    std::filesystem::create_directories(b, error);
    std::filesystem::create_directories(c, error);
    QVERIFY(write_document(c / "index.json", QByteArray("{")));
    const auto path = base / "dictionary_config.json";
    const QJsonArray terms{entry("B", 0, "monolingual"), entry("C", 1, "monolingual")};
    QVERIFY(write_config(path, config(terms)));
    const auto entries = term_dictionaries({a, b, c}, path);

    QCOMPARE(entries.size(), std::size_t(1));
    QCOMPARE(title(entries.at(0)), QStringLiteral("A"));
    QCOMPARE(entries.at(0).category, DictionaryCategory::none);
}

// B6: a missing config keeps the scan order with category none.
void TermDictionariesTest::a_missing_config_lists_installed_directories() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto base = base_directory(root);
    const auto a = make_dictionary(base, "A");
    const auto b = make_dictionary(base, "B");
    const auto c = make_dictionary(base, "C");
    const auto path = base / "dictionary_config.json";
    const auto entries = term_dictionaries({a, b, c}, path);

    QCOMPARE(entries.size(), std::size_t(3));
    QCOMPARE(title(entries.at(0)), QStringLiteral("A"));
    QCOMPARE(title(entries.at(1)), QStringLiteral("B"));
    QCOMPARE(title(entries.at(2)), QStringLiteral("C"));
    for (const auto& dictionary : entries) {
        QCOMPARE(dictionary.category, DictionaryCategory::none);
    }
}

// B6: any bad JSON, wrong type or unknown category takes the no-config branch.
void TermDictionariesTest::an_invalid_config_lists_installed_directories_data() {
    QTest::addColumn<QByteArray>("document");
    QTest::newRow("bad json") << QByteArray("{");
    QTest::newRow("wrong root") << QByteArray("[]");
    QTest::newRow("terms not an array") << QByteArray(
        R"({"termDictionaries":{},"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("missing required array")
        << QByteArray(R"({"termDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("frequency entry wrong type") << QByteArray(
        R"({"termDictionaries":[],"frequencyDictionaries":[7],"pitchDictionaries":[]})");
    QTest::newRow("kanji wrong type") << QByteArray(
        R"({"termDictionaries":[],"frequencyDictionaries":[],"pitchDictionaries":[],"kanjiDictionaries":{}})");
    QTest::newRow("unknown category") << QByteArray(
        R"({"termDictionaries":[{"fileName":"A","isEnabled":true,"order":0,"category":"other"}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("order not an integer") << QByteArray(
        R"({"termDictionaries":[{"fileName":"A","isEnabled":true,"order":"0"}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
    QTest::newRow("missing isEnabled") << QByteArray(
        R"({"termDictionaries":[{"fileName":"A","order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})");
}

void TermDictionariesTest::an_invalid_config_lists_installed_directories() {
    QFETCH(QByteArray, document);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto base = base_directory(root);
    const auto a = make_dictionary(base, "A");
    const auto b = make_dictionary(base, "B");
    const auto path = base / "dictionary_config.json";
    QVERIFY(write_document(path, document));
    const auto entries = term_dictionaries({a, b}, path);

    QCOMPARE(entries.size(), std::size_t(2));
    QCOMPARE(title(entries.at(0)), QStringLiteral("A"));
    QCOMPARE(title(entries.at(1)), QStringLiteral("B"));
    for (const auto& dictionary : entries) {
        QCOMPARE(dictionary.category, DictionaryCategory::none);
    }
}

QTEST_APPLESS_MAIN(TermDictionariesTest)
#include "tst_term_dictionaries.moc"
