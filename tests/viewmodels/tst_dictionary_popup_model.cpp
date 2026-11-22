// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary page's view model as QML uses it: an installed dictionary is
// looked up through the model and the resulting state and signals are
// observed. The data root points at a temporary directory, so the test never
// reads the developer's dictionaries or profile settings.

#include "data_root.h"

#include "services/anki/anki_config.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"
#include "yomitan_fixture.h"

#include <QSignalSpy>
#include <QtTest>

#include <filesystem>
#include <memory>

using niratan::anki::AnkiConfigStore;
using niratan::dictionary::DictionaryStore;
using niratan::profiles::ProfileRepository;
using niratan::test::minimal_index;
using niratan::test::minimal_term_bank;
using niratan::test::TempDataRoot;
using niratan::test::write_stored_zip;
using niratan::viewmodels::DictionaryPopupModel;

class DictionaryPopupModelTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void lookup_builds_popup_html();
    void whitespace_clears_results();
    void matched_text_is_the_first_result();
    void invalid_audio_url_is_reported();
    void shell_globals_come_from_the_anki_configuration();
    void shell_globals_follow_the_active_profile();

   private:
    std::unique_ptr<TempDataRoot> root_;
};

void DictionaryPopupModelTest::initTestCase() {
    root_ = std::make_unique<TempDataRoot>();
    QVERIFY(root_->isValid());

    const auto zip = std::filesystem::path(root_->path().toStdString()) / "dictionary.zip";
    write_stored_zip(
        zip, {{"index.json", minimal_index("Probe")}, {"term_bank_1.json", minimal_term_bank()}});

    const auto dictionaries = niratan::storage::dictionaries_root();
    QVERIFY(!dictionaries.empty());
    const auto imported = DictionaryStore(dictionaries).import_zip(zip);
    QVERIFY2(imported.ok, imported.error.c_str());
}

void DictionaryPopupModelTest::lookup_builds_popup_html() {
    DictionaryPopupModel model;
    QSignalSpy html_spy(&model, &DictionaryPopupModel::htmlChanged);
    QSignalSpy error_spy(&model, &DictionaryPopupModel::errorChanged);

    model.lookup(QStringLiteral("  decide  "));

    QCOMPARE(html_spy.count(), 1);
    QCOMPARE(error_spy.count(), 1);
    QVERIFY(model.has_results());
    QVERIFY(model.error().isEmpty());
    // The shell embeds the matched expression for the popup script.
    QVERIFY(model.html().contains(QStringLiteral("decide")));
}

void DictionaryPopupModelTest::whitespace_clears_results() {
    DictionaryPopupModel model;
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.has_results());

    QSignalSpy html_spy(&model, &DictionaryPopupModel::htmlChanged);
    model.lookup(QStringLiteral("   "));

    QCOMPARE(html_spy.count(), 1);
    QVERIFY(!model.has_results());
    QVERIFY(model.html().isEmpty());
}

void DictionaryPopupModelTest::matched_text_is_the_first_result() {
    DictionaryPopupModel model;
    QVERIFY(model.matched_text().isEmpty());
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.has_results());
    QCOMPARE(model.matched_text(), QStringLiteral("decide"));
    model.clear();
    QVERIFY(model.matched_text().isEmpty());
}

void DictionaryPopupModelTest::invalid_audio_url_is_reported() {
    DictionaryPopupModel model;
    QSignalSpy audio_error_spy(&model, &DictionaryPopupModel::audioErrorChanged);

    model.playWordAudio(QStringLiteral("file:///tmp/audio.mp3"));

    QCOMPARE(audio_error_spy.count(), 1);
    QVERIFY(!model.audio_error().isEmpty());
}

// B9–B13, B15: the shell carries the five mining globals from the active
// profile; useAnkiConnect is the computed constant true.
void DictionaryPopupModelTest::shell_globals_come_from_the_anki_configuration() {
    ProfileRepository profiles{niratan::storage::data_root()};
    AnkiConfigStore config{niratan::storage::data_root(), profiles};
    auto& settings = config.settings();
    settings.field_mappings = {{"Audio", "{audio}"}};
    settings.allow_dupes = true;
    settings.embed_media = true;
    settings.compact_glossaries = true;
    QVERIFY(config.save().ok);

    DictionaryPopupModel model;
    model.lookup(QStringLiteral("decide"));

    QVERIFY(model.has_results());
    QVERIFY(model.html().contains(QStringLiteral("window.needsAudio = true;")));
    QVERIFY(model.html().contains(QStringLiteral("window.allowDupes = true;")));
    QVERIFY(model.html().contains(QStringLiteral("window.useAnkiConnect = true;")));
    QVERIFY(model.html().contains(QStringLiteral("window.embedMedia = true;")));
    QVERIFY(model.html().contains(QStringLiteral("window.compactGlossariesAnki = true;")));
}

// B14: a profile change changes the globals before the next render.
void DictionaryPopupModelTest::shell_globals_follow_the_active_profile() {
    ProfileRepository profiles{niratan::storage::data_root()};
    AnkiConfigStore config{niratan::storage::data_root(), profiles};
    auto& defaults = config.settings();
    defaults.field_mappings = {{"Audio", "{audio}"}};
    defaults.allow_dupes = true;
    defaults.embed_media = true;
    defaults.compact_glossaries = true;
    QVERIFY(config.save().ok);

    DictionaryPopupModel model;
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.html().contains(QStringLiteral("window.needsAudio = true;")));

    const auto other = profiles.create_profile(
        "Other", niratan::profiles::ContentLanguage::japanese, profiles.active_profile().id);
    QVERIFY(other.profile.has_value());
    config.activate_profile(other.profile->id);
    auto& settings = config.settings();
    settings.field_mappings = {{"Word", "{expression}"}};
    settings.allow_dupes = false;
    settings.embed_media = false;
    settings.compact_glossaries = false;
    QVERIFY(config.save().ok);
    QVERIFY(!profiles.set_active_profile(other.profile->id).has_value());

    model.lookup(QStringLiteral("decide"));

    QVERIFY(model.html().contains(QStringLiteral("window.needsAudio = false;")));
    QVERIFY(model.html().contains(QStringLiteral("window.allowDupes = false;")));
    QVERIFY(model.html().contains(QStringLiteral("window.useAnkiConnect = true;")));
    QVERIFY(model.html().contains(QStringLiteral("window.embedMedia = false;")));
    QVERIFY(model.html().contains(QStringLiteral("window.compactGlossariesAnki = false;")));
}

QTEST_MAIN(DictionaryPopupModelTest)
#include "tst_dictionary_popup_model.moc"
