// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dictionary page's view model as QML uses it: an installed dictionary is
// looked up through the model and the resulting state and signals are
// observed. The data root points at a temporary directory, so the test never
// reads the developer's dictionaries or profile settings.

#include "data_root.h"

#include "models/profiles/dictionary_settings.h"
#include "services/anki/anki_config.h"
#include "services/dictionary/dictionary_store.h"
#include "services/profiles/profile_repository.h"
#include "services/settings/global_settings.h"
#include "services/storage/app_paths.h"
#include "viewmodels/dictionary/dictionary_popup_model.h"
#include "yomitan_fixture.h"

#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QJsonArray>
#include <QTest>

#include <filesystem>
#include <memory>
#include <optional>

using niratan::anki::AnkiConfigStore;
using niratan::dictionary::DictionaryStore;
using niratan::profiles::ProfileRepository;
using niratan::settings::GlobalSettings;
using niratan::settings::StoreError;
using niratan::test::minimal_index;
using niratan::test::minimal_term_bank;
using niratan::test::TempDataRoot;
using niratan::test::write_stored_zip;
using niratan::viewmodels::DictionaryPopupModel;

namespace {

// A profile config that enables the imported Probe term dictionary.
bool write_probe_config(const std::filesystem::path& directory) {
    QFile file(QString::fromStdString((directory / "dictionary_config.json").string()));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray bytes =
        R"({"termDictionaries":[{"fileName":"Probe","isEnabled":true,"order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})";
    return file.write(bytes) == bytes.size();
}

bool write_text(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(bytes) == bytes.size();
}

// The Probe entry enabled at order 0; category omit means DictionaryCategory.none.
QByteArray probe_config(const char* category = nullptr) {
    QByteArray json = R"({"termDictionaries":[{"fileName":"Probe","isEnabled":true,"order":0)";
    if (category != nullptr) {
        json += R"(,"category":")" + QByteArray(category) + "\"";
    }
    json += R"(}],"frequencyDictionaries":[],"pitchDictionaries":[]})";
    return json;
}

bool write_profile_settings(const std::filesystem::path& directory,
                            const std::string& collapse_mode,
                            std::optional<int> search_text_size = std::nullopt,
                            bool scan_non_japanese_text = true) {
    niratan::profiles::DictionaryProfileSettings settings =
        niratan::profiles::default_dictionary_settings();
    settings.collapse_mode = collapse_mode;
    if (search_text_size.has_value()) {
        settings.search_text_size = *search_text_size;
    }
    settings.scan_non_japanese_text = scan_non_japanese_text;
    return write_text(
        directory / "dictionary_settings.json",
        niratan::profiles::dictionary_settings_to_json(settings).toJson(QJsonDocument::Compact));
}

// The JSON value of a `window.<name> = <value>;` assignment in the shell.
QString array_section(const QString& html, const QString& name) {
    const QString marker = QStringLiteral("window.") + name + QStringLiteral(" = ");
    const qsizetype start = html.indexOf(marker);
    if (start < 0) {
        return {};
    }
    const qsizetype value_start = start + marker.size();
    const qsizetype end = html.indexOf(QStringLiteral(";\n"), value_start);
    return end < 0 ? QString() : html.mid(value_start, end - value_start);
}

QStringList string_array(const QString& json) {
    QStringList values;
    for (const QJsonValue& value : QJsonDocument::fromJson(json.toUtf8()).array()) {
        values.append(value.toString());
    }
    return values;
}

}  // namespace

class DictionaryPopupModelTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void lookup_builds_popup_html();
    void whitespace_clears_results();
    void matched_text_is_the_first_result();
    void invalid_audio_url_is_reported();
    void preferred_audio_source_precedes_the_default();
    void shell_globals_come_from_the_anki_configuration();
    void shell_globals_follow_the_active_profile();
    void the_query_follows_the_active_profile_configuration();
    void collapsed_and_excluded_dictionaries_reach_the_shell();
    void search_text_size_comes_from_the_profile();
    void page_query_heading_shows_the_query_and_size();
    void dictionary_file_edits_render_the_open_popup();
    void inline_lookup_requests_answer_from_the_current_entries();

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

void DictionaryPopupModelTest::preferred_audio_source_precedes_the_default() {
    auto settings = GlobalSettings::at_data_root();
    DictionaryPopupModel model;
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.has_results());
    QVERIFY(model.html().contains(QStringLiteral("window.audioSourceNames = [\"Default\"];")));
    QCOMPARE(settings.save_preferred_audio_source(
                 "http://127.0.0.1:5050/localaudio/get/?term={term}&reading={reading}"),
             StoreError::none);
    model.lookup(QStringLiteral("decide"));

    QVERIFY(model.html().contains(
        QStringLiteral("window.audioSources = [\"http://127.0.0.1:5050/localaudio/get/"
                       "?term={term}&reading={reading}\","
                       "\"https://hoshi-reader.manhhaoo-do.workers.dev/"
                       "?term={term}&reading={reading}\"];")));
    QVERIFY(model.html().contains(
        QStringLiteral("window.audioSourceNames = [\"Preferred\",\"Default\"];")));

    QCOMPARE(settings.save_preferred_audio_source({}), StoreError::none);
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.html().contains(QStringLiteral("window.audioSourceNames = [\"Default\"];")));
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
    const auto other_directory = profiles.profile_directory(other.profile->id);
    QVERIFY(other_directory.has_value());
    QVERIFY(write_probe_config(*other_directory));
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

// B7: the popup query follows the active profile's configuration, so a
// profile switch and a dictionary_config.json edit reach the next lookup on
// the same model.
void DictionaryPopupModelTest::the_query_follows_the_active_profile_configuration() {
    ProfileRepository profiles{niratan::storage::data_root()};
    QVERIFY(!profiles.set_active_profile("default-ja").has_value());

    DictionaryPopupModel model;
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.has_results());

    const auto english = profiles.create_profile(
        "English", niratan::profiles::ContentLanguage::english, std::string("default-ja"));
    QVERIFY(english.profile.has_value());
    QVERIFY(!profiles.set_active_profile(english.profile->id).has_value());
    model.lookup(QStringLiteral("decide"));
    QVERIFY(!model.has_results());

    const auto directory = profiles.profile_directory(english.profile->id);
    QVERIFY(directory.has_value());
    QVERIFY(write_probe_config(*directory));
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.has_results());
    QVERIFY(model.html().contains(QStringLiteral("window.hoshiSelection.language = 'en';")));

    QVERIFY(!profiles.set_active_profile("default-ja").has_value());
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.has_results());
}

// B1: PopupView.buildContent carries DictionaryManager.collapsedDictionaries
// only in Custom mode and DictionaryManager.excludedDictionaries always.
void DictionaryPopupModelTest::collapsed_and_excluded_dictionaries_reach_the_shell() {
    ProfileRepository profiles{niratan::storage::data_root()};
    QVERIFY(!profiles.set_active_profile("default-ja").has_value());
    const auto directory = profiles.profile_directory("default-ja");
    QVERIFY(directory.has_value());
    const auto twin_zip = std::filesystem::path(root_->path().toStdString()) / "twin.zip";
    write_stored_zip(
        twin_zip,
        {{"index.json", minimal_index("Twin")}, {"term_bank_1.json", minimal_term_bank()}});
    QVERIFY(DictionaryStore(niratan::storage::dictionaries_root()).import_zip(twin_zip).ok);
    QVERIFY(write_text(
        *directory / "dictionary_config.json",
        R"({"termDictionaries":[{"fileName":"Twin","isEnabled":true,"order":0,"category":"exclude"},{"fileName":"Probe","isEnabled":false,"order":1,"category":"exclude"}],"frequencyDictionaries":[],"pitchDictionaries":[]})"));
    QVERIFY(write_profile_settings(*directory, "Custom", std::nullopt, false));
    QVERIFY(write_text(*directory / "collapsed.json", R"(["Probe"])"));

    DictionaryPopupModel custom;
    custom.lookup(QStringLiteral("decide"));
    QVERIFY(custom.has_results());
    QVERIFY(custom.html().contains(QStringLiteral("window.scanNonJapaneseText = false;")));
    QCOMPARE(string_array(array_section(custom.html(), QStringLiteral("excludedDictionaries"))),
             (QStringList{QStringLiteral("Twin"), QStringLiteral("Probe")}));
    QCOMPARE(string_array(array_section(custom.html(), QStringLiteral("collapsedDictionaries"))),
             QStringList{QStringLiteral("Probe")});

    QVERIFY(write_profile_settings(*directory, "Expand All", std::nullopt, false));
    DictionaryPopupModel expanded;
    expanded.lookup(QStringLiteral("decide"));
    QVERIFY(expanded.has_results());
    QVERIFY(string_array(array_section(expanded.html(), QStringLiteral("collapsedDictionaries")))
                .isEmpty());
    QCOMPARE(string_array(array_section(expanded.html(), QStringLiteral("excludedDictionaries"))),
             (QStringList{QStringLiteral("Twin"), QStringLiteral("Probe")}));

    // A profile without a config keeps the default profile's fallback list,
    // whose entries carry no category, and has no collapse set.
    std::filesystem::remove(*directory / "dictionary_config.json");
    std::filesystem::remove(*directory / "dictionary_settings.json");
    std::filesystem::remove(*directory / "collapsed.json");
    DictionaryPopupModel bare;
    bare.lookup(QStringLiteral("decide"));
    QVERIFY(bare.has_results());
    QVERIFY(string_array(array_section(bare.html(), QStringLiteral("collapsedDictionaries")))
                .isEmpty());
    QVERIFY(
        string_array(array_section(bare.html(), QStringLiteral("excludedDictionaries"))).isEmpty());
}

// B3, Decision 7: UserConfig.searchTextSize sizes the page's search field.
void DictionaryPopupModelTest::search_text_size_comes_from_the_profile() {
    ProfileRepository profiles{niratan::storage::data_root()};
    QVERIFY(!profiles.set_active_profile("default-ja").has_value());
    const auto directory = profiles.profile_directory("default-ja");
    QVERIFY(directory.has_value());
    std::filesystem::remove(*directory / "dictionary_settings.json");

    DictionaryPopupModel defaults;
    QCOMPARE(defaults.search_text_size(), 26);

    QVERIFY(write_profile_settings(*directory, "Expand All", 30));
    DictionaryPopupModel sized;
    sized.lookup(QStringLiteral("decide"));
    QCOMPARE(sized.search_text_size(), 30);

    QSignalSpy size_spy(&sized, &DictionaryPopupModel::searchTextSizeChanged);
    QVERIFY(write_profile_settings(*directory, "Expand All", 31));
    QTRY_COMPARE_WITH_TIMEOUT(size_spy.count(), 1, 5000);
    QCOMPARE(sized.search_text_size(), 31);
}

// B3: the dictionary page host carries the trimmed query as the heading text
// and the resolved searchTextSize as its size; toggling the page property with
// a query displayed rerenders, and a saved size edit rerenders the open page.
void DictionaryPopupModelTest::page_query_heading_shows_the_query_and_size() {
    ProfileRepository profiles{niratan::storage::data_root()};
    QVERIFY(!profiles.set_active_profile("default-ja").has_value());
    const auto directory = profiles.profile_directory("default-ja");
    QVERIFY(directory.has_value());
    QVERIFY(write_probe_config(*directory));
    QVERIFY(write_profile_settings(*directory, "Expand All", 26));

    DictionaryPopupModel model;
    QVERIFY(!model.page());
    model.lookup(QStringLiteral("  decide  "));
    QVERIFY(model.has_results());
    QVERIFY(!model.html().contains(QStringLiteral("id=\"dictionary-query-source\"")));
    QVERIFY(array_section(model.html(), QStringLiteral("dictionaryQuerySource")).isEmpty());

    model.setPage(true);
    QVERIFY(model.page());
    QTRY_VERIFY_WITH_TIMEOUT(
        model.html().contains(QStringLiteral("id=\"dictionary-query-source\"")), 2000);

    model.lookup(QStringLiteral("  decide  "));
    QVERIFY(model.html().contains(QStringLiteral("id=\"dictionary-query-source\"")));
    QVERIFY(model.html().contains(QStringLiteral("font-size: 26px")));
    QVERIFY(model.html().contains(QStringLiteral("min-height: calc(26px * 1.4)")));
    QVERIFY(model.html().contains(QStringLiteral("window.dictionaryQuerySource = \"decide\";")));
    QVERIFY(
        !model.html().contains(QStringLiteral("window.dictionaryQuerySource = \"  decide  \";")));

    QSignalSpy size_spy(&model, &DictionaryPopupModel::searchTextSizeChanged);
    QVERIFY(write_profile_settings(*directory, "Expand All", 31));
    QTRY_COMPARE_WITH_TIMEOUT(size_spy.count(), 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(model.html().contains(QStringLiteral("font-size: 31px")), 5000);
    QVERIFY(model.html().contains(QStringLiteral("min-height: calc(31px * 1.4)")));
}

// B5: DictionaryManager.reloadActiveProfileDictionaryState; an open popup
// picks up dictionary_config.json and collapsed.json edits from another window.
void DictionaryPopupModelTest::dictionary_file_edits_render_the_open_popup() {
    ProfileRepository profiles{niratan::storage::data_root()};
    QVERIFY(!profiles.set_active_profile("default-ja").has_value());
    const auto directory = profiles.profile_directory("default-ja");
    QVERIFY(directory.has_value());
    QVERIFY(write_text(*directory / "dictionary_config.json", probe_config()));
    QVERIFY(write_profile_settings(*directory, "Expand All"));
    std::filesystem::remove(*directory / "collapsed.json");

    DictionaryPopupModel model;
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.has_results());
    QVERIFY(!array_section(model.html(), QStringLiteral("excludedDictionaries"))
                 .contains(QStringLiteral("Probe")));

    QVERIFY(write_text(*directory / "dictionary_config.json", probe_config("exclude")));
    QTRY_VERIFY_WITH_TIMEOUT(array_section(model.html(), QStringLiteral("excludedDictionaries"))
                                 .contains(QStringLiteral("Probe")),
                             5000);

    QVERIFY(write_profile_settings(*directory, "Custom"));
    QVERIFY(write_text(*directory / "collapsed.json", R"(["Probe"])"));
    QTRY_VERIFY_WITH_TIMEOUT(array_section(model.html(), QStringLiteral("collapsedDictionaries"))
                                 .contains(QStringLiteral("Probe")),
                             5000);

    // The rerun also rebuilds the query: a disabled dictionary contributes no
    // entries and the document clears.
    QVERIFY(write_text(
        *directory / "dictionary_config.json",
        R"({"termDictionaries":[{"fileName":"Probe","isEnabled":false,"order":0}],"frequencyDictionaries":[],"pitchDictionaries":[]})"));
    QTRY_VERIFY_WITH_TIMEOUT(!model.has_results(), 5000);
}

// B6: PopupWebView.Coordinator (getEntries, lookupRedirect, queryTextSelected)
// and DictionarySearchView.onRedirect/onQueryTextSelected; PopupModels.applyLookupMatch
// counts the matched text.
void DictionaryPopupModelTest::inline_lookup_requests_answer_from_the_current_entries() {
    ProfileRepository profiles{niratan::storage::data_root()};
    QVERIFY(!profiles.set_active_profile("default-ja").has_value());
    const auto directory = profiles.profile_directory("default-ja");
    QVERIFY(directory.has_value());
    QVERIFY(write_text(*directory / "dictionary_config.json", probe_config()));
    QVERIFY(write_profile_settings(*directory, "Expand All"));

    DictionaryPopupModel model;
    model.lookup(QStringLiteral("decide"));
    QVERIFY(model.has_results());

    const QVariantList slice = model.getEntries(0, 1);
    QCOMPARE(slice.size(), 1);
    QCOMPARE(slice.first().toMap().value(QStringLiteral("expression")).toString(),
             QStringLiteral("decide"));
    QVERIFY(model.getEntries(1, 1).isEmpty());
    QVERIFY(model.getEntries(-1, 1).isEmpty());
    QVERIFY(model.getEntries(0, 2).isEmpty());

    QCOMPARE(model.lookupRedirect(QStringLiteral("decide")), 1);

    const QVariantMap selected = model.queryTextSelected(QStringLiteral("decide"));
    QCOMPARE(selected.value(QStringLiteral("entryCount")).toInt(), 1);
    QCOMPARE(selected.value(QStringLiteral("matchedCharacterCount")).toInt(), 6);

    QCOMPARE(model.textSelected(QStringLiteral("decide")), 6);
    QVERIFY(model.html().contains(QStringLiteral("decide")));

    QCOMPARE(model.lookupRedirect(QStringLiteral("zzzz")), 0);
    QVERIFY(model.getEntries(0, 1).isEmpty());
    const QVariantMap empty = model.queryTextSelected(QStringLiteral("zzzz"));
    QCOMPARE(empty.value(QStringLiteral("entryCount")).toInt(), 0);
    QCOMPARE(empty.value(QStringLiteral("matchedCharacterCount")).toInt(), 0);
    const QVariantMap again = model.queryTextSelected(QStringLiteral("decide"));
    QCOMPARE(again.value(QStringLiteral("entryCount")).toInt(), 1);
    QCOMPARE(model.getEntries(0, 1).size(), 1);
    QCOMPARE(model.getEntries(0, 1).first().toMap().value(QStringLiteral("expression")).toString(),
             QStringLiteral("decide"));
    QCOMPARE(model.textSelected(QStringLiteral("zzzz")), 0);
    QVERIFY(!model.has_results());
    QVERIFY(model.html().isEmpty());
}

QTEST_MAIN(DictionaryPopupModelTest)
#include "tst_dictionary_popup_model.moc"
