// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary page's view model as QML uses it: an installed dictionary is
// looked up through the model and the resulting state and signals are
// observed. The data root points at a temporary directory, so the test never
// reads the developer's dictionaries or profile settings.

#include "data_root.h"

#include "services/dictionary/dictionary_store.h"
#include "services/storage/app_paths.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"
#include "yomitan_fixture.h"

#include <QSignalSpy>
#include <QtTest>

#include <filesystem>
#include <memory>

using niratan::dictionary::DictionaryStore;
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
    void invalid_audio_url_is_reported();

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

void DictionaryPopupModelTest::invalid_audio_url_is_reported() {
    DictionaryPopupModel model;
    QSignalSpy audio_error_spy(&model, &DictionaryPopupModel::audioErrorChanged);

    model.playWordAudio(QStringLiteral("file:///tmp/audio.mp3"));

    QCOMPARE(audio_error_spy.count(), 1);
    QVERIFY(!model.audio_error().isEmpty());
}

QTEST_MAIN(DictionaryPopupModelTest)
#include "tst_dictionary_popup_model.moc"
