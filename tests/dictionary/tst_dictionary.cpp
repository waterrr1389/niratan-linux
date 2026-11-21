// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/dictionary_media.h"
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
    void media_and_styles_are_available();
    void resolves_popup_media_by_dictionary_and_decoded_path();
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
    const niratan::dictionary::LookupEntry& entry = found.entries.front();
    QCOMPARE(entry.expression, std::string("decide"));
    QCOMPARE(entry.reading, std::string("decide"));
    QCOMPARE(entry.rules, std::string("v"));
    QCOMPARE(entry.glossaries.size(), 1u);
    QCOMPARE(entry.glossaries.front().dictionary, std::string("Probe"));
    const std::string expected_glossary = "[\"to make up one's mind\"]";
    QCOMPARE(entry.glossaries.front().content, expected_glossary);
    QCOMPARE(entry.glossaries.front().definition_tags, std::string(""));
    QCOMPARE(entry.glossaries.front().term_tags, std::string(""));
    QVERIFY(entry.frequencies.empty());
    QVERIFY(entry.pitches.empty());
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
    const niratan::dictionary::LookupEntry& entry = found.entries.front();
    QCOMPARE(entry.expression, std::string("decide"));
    QVERIFY(!entry.traces.empty());

    bool has_past_step = false;
    for (const auto& trace : entry.traces) {
        for (const auto& step : trace) {
            if (step.name == "past" && step.description == "Simple past tense of a verb") {
                has_past_step = true;
            }
        }
    }
    QVERIFY(has_past_step);
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

void DictionaryTest::media_and_styles_are_available() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto zip = std::filesystem::path(root.path().toStdString()) / "dict.zip";
    write_stored_zip(zip, {{"index.json", minimal_index("Probe")},
                           {"term_bank_1.json", minimal_term_bank()},
                           {"media.png", QByteArray("PNG")},
                           {"styles.css", QByteArray(".probe { color: red; }")}});

    DictionaryStore store(std::filesystem::path(root.path().toStdString()) / "installed");
    const auto imported = store.import_zip(zip);
    QVERIFY2(imported.ok, imported.error.c_str());

    DictionaryQueryService service("en", {imported.directory});
    QCOMPARE(service.media_file("Probe", "media.png"), std::string("PNG"));
    QVERIFY(service.media_file("Probe", "missing.png").empty());
    QVERIFY(service.media_file("Other", "media.png").empty());

    const auto styles = service.dictionary_styles();
    QCOMPARE(styles.size(), 1u);
    QCOMPARE(styles.front().first, std::string("Probe"));
    QVERIFY(styles.front().second.find(".probe { color: red; }") != std::string::npos);
}

// PopupWebView.swift: ImageHandler looks up the decoded dictionary and path.
void DictionaryTest::resolves_popup_media_by_dictionary_and_decoded_path() {
    using niratan::dictionary::DictionaryMediaService;
    using niratan::dictionary::MediaStatus;

    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto directory = std::filesystem::path(root.path().toStdString());
    const auto zip = directory / "dict.zip";
    const QByteArray audio("WAV\0bytes", 9);
    write_stored_zip(zip, {{"index.json", minimal_index("Probe + 名")},
                           {"term_bank_1.json", minimal_term_bank()},
                           {"assets/picture.png", QByteArray("PNG")},
                           {"assets/voice %20.WAV", audio}});
    const auto installed = directory / "installed";
    DictionaryMediaService media(installed);
    const QUrl image("dict://?dictionary=Probe%20%2B%20%E5%90%8D&path=assets%2Fpicture.png");
    QCOMPARE(media.get(image).status, MediaStatus::Missing);
    const auto imported = DictionaryStore(installed).import_zip(zip);
    QVERIFY2(imported.ok, imported.error.c_str());

    const auto image_response = media.get(image);
    QCOMPARE(image_response.status, MediaStatus::Found);
    QCOMPARE(image_response.mime_type, QByteArray("image/png"));
    QCOMPARE(image_response.bytes, QByteArray("PNG"));
    const auto audio_response = media.get(
        QUrl("dict://?dictionary=Probe%20%2B%20%E5%90%8D&path=assets%2Fvoice%20%2520.WAV"));
    QCOMPARE(audio_response.status, MediaStatus::Found);
    QCOMPARE(audio_response.mime_type, QByteArray("audio/wav"));
    QCOMPARE(audio_response.bytes, audio);
    QCOMPARE(media.get(QUrl("dict://?dictionary=Other&path=assets/picture.png")).status,
             MediaStatus::Missing);
    QCOMPARE(media.get(QUrl("dict://?dictionary=Probe&path=../../etc/passwd")).status,
             MediaStatus::Missing);
    QCOMPARE(media.get(QUrl("dict://?dictionary=Probe")).status, MediaStatus::Invalid);
}

QTEST_APPLESS_MAIN(DictionaryTest)
#include "tst_dictionary.moc"
