// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/dictionary_query.h"
#include "services/dictionary/dictionary_store.h"
#include "services/dictionary/language_id.h"
#include "yomitan_fixture.h"

#include <QtTest>

#include <filesystem>

using niratan::dictionary::DictionaryQueryService;
using niratan::dictionary::DictionaryStore;
using niratan::test::minimal_index;
using niratan::test::minimal_term_bank;
using niratan::test::write_stored_zip;

// Kept out of niratan::test: the fixture header opens that namespace, and
// moc would otherwise emit the meta-object inside it.
class DictionaryTest : public QObject {
    Q_OBJECT

   private slots:
    void language_falls_back_to_japanese();
    void imports_and_looks_up();
    void deinflects_inside_the_library();
    void rejects_zip_without_index();
};

void DictionaryTest::language_falls_back_to_japanese() {
    QCOMPARE(niratan::dictionary::normalize_language_id("en"), "en");
    QCOMPARE(niratan::dictionary::normalize_language_id("ja"), "ja");
    QCOMPARE(niratan::dictionary::normalize_language_id("zh"), "ja");
    QCOMPARE(niratan::dictionary::normalize_language_id(""), "ja");
}

void DictionaryTest::imports_and_looks_up() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto zip = std::filesystem::path(root.path().toStdString()) / "dict.zip";
    write_stored_zip(zip, {{"index.json", minimal_index("Probe")},
                           {"term_bank_1.json", minimal_term_bank()}});

    DictionaryStore store(std::filesystem::path(root.path().toStdString()) / "installed");
    const auto imported = store.import_zip(zip);

    QVERIFY2(imported.ok, imported.error.c_str());
    QCOMPARE(imported.term_count, 1);
    QCOMPARE(imported.directory.filename().string(), std::string("Probe"));
    QVERIFY(std::filesystem::exists(imported.directory / "blobs.bin"));
    QVERIFY(std::filesystem::exists(imported.directory / "hash.table"));
    QVERIFY(std::filesystem::exists(imported.directory / ".hoshidicts_2"));

    DictionaryQueryService service("en", {imported.directory});
    const auto found = service.lookup("decide", 8, 16);
    QVERIFY2(found.ok, found.error.c_str());
    QCOMPARE(found.entries.size(), 1u);
    QCOMPARE(found.entries.front().expression, std::string("decide"));
}

void DictionaryTest::deinflects_inside_the_library() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto zip = std::filesystem::path(root.path().toStdString()) / "dict.zip";
    write_stored_zip(zip, {{"index.json", minimal_index("Probe")},
                           {"term_bank_1.json", minimal_term_bank()}});

    DictionaryStore store(std::filesystem::path(root.path().toStdString()) / "installed");
    const auto imported = store.import_zip(zip);
    QVERIFY(imported.ok);

    DictionaryQueryService service("en", {imported.directory});
    const auto found = service.lookup("decided", 8, 16);
    QVERIFY2(found.ok, found.error.c_str());
    QCOMPARE(found.entries.size(), 1u);
    QCOMPARE(found.entries.front().expression, std::string("decide"));
}

void DictionaryTest::rejects_zip_without_index() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto zip = std::filesystem::path(root.path().toStdString()) / "broken.zip";
    write_stored_zip(zip, {{"term_bank_1.json", minimal_term_bank()}});

    const auto installed = std::filesystem::path(root.path().toStdString()) / "installed";
    DictionaryStore store(installed);
    const auto imported = store.import_zip(zip);

    QVERIFY(!imported.ok);
    QVERIFY(!imported.error.empty());
    QVERIFY(store.installed().empty());
}

QTEST_APPLESS_MAIN(DictionaryTest)
#include "tst_dictionary.moc"
