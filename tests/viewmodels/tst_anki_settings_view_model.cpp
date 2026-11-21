// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Settings/AnkiView.swift and AnkiConnectView.swift, Core/AnkiManager.swift,
// Models/Anki.swift and script/test_anki_settings_persistence.swift: settings
// commands save through the real store and use a loopback AnkiConnect stub.

#include "viewmodels/anki/anki_settings_view_model.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

#include <filesystem>

#include "anki_stub.h"
#include "data_root.h"
#include "services/anki/anki_config.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "viewmodels/anki/mining_view_model.h"

using niratan::anki::AnkiConfigStore;
using niratan::anki::AnkiSettings;
using niratan::anki::DuplicateScope;
using niratan::profiles::ContentLanguage;
using niratan::profiles::ProfileRepository;
using niratan::storage::data_root;
using niratan::test::AnkiStub;
using niratan::test::TempDataRoot;
using niratan::viewmodels::AnkiSettingsViewModel;
using niratan::viewmodels::MiningViewModel;

namespace {

const QString default_address = QStringLiteral("http://127.0.0.1:8765");
const QString fetch_not_connected =
    QStringLiteral("AnkiConnect is not connected. Please start Anki and try again.");
const QString empty_metadata = QStringLiteral("No decks or models were returned from Anki.");

struct Fixture {
    TempDataRoot data;
    AnkiStub anki;
    ProfileRepository profiles{data_root()};
    AnkiConfigStore store{data_root(), profiles};

    Fixture() {
        auto& settings = store.settings();
        settings.selected_deck = "Reading";
        settings.selected_note_type = "Custom";
        settings.available_decks = {"Default", "Reading"};
        settings.available_note_types = {{"Custom", {"Word", "Sentence"}}};
        settings.field_mappings = {{"Word", "{expression}"}};
        settings.transport->url = anki.url();
        anki.responses["version"] = {{"result", 6}};
    }

    bool save() { return data.isValid() && anki.server.isListening() && store.save().ok; }

    AnkiSettings saved() const {
        ProfileRepository current(data_root());
        AnkiConfigStore reader(data_root(), current);
        reader.load();
        return reader.settings();
    }
};

bool write_bytes(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

}  // namespace

class AnkiSettingsViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void open_projects_profile_settings_and_shared_lists();
    void open_repairs_missing_and_whitespace_addresses();
    void text_edits_persist_immediately();
    void next_open_loads_new_active_profile_but_keeps_shared_values();
    void connect_repairs_blank_address_before_the_request();
    void open_and_connect_ping_without_retries();
    void failed_fetch_ping_keeps_metadata_and_shows_error();
    void fetch_short_circuits_on_a_bad_list_data();
    void fetch_short_circuits_on_a_bad_list();
    void fetch_requests_fields_in_order_and_applies_usable_metadata();
    void fetch_with_no_usable_models_keeps_saved_metadata();
    void fetch_request_error_is_shown_until_dismissed();
    void selecting_deck_saves_without_touching_mappings();
    void selecting_model_autofills_template_and_keeps_custom_mappings();
    void toggles_and_duplicate_scope_save_immediately();
    void mapping_menu_uses_active_dictionaries_and_saves_choices();
    void defaults_require_a_supported_model_and_save_only_changes();
    void next_settings_and_mining_requests_use_saved_transport();
    void a_failed_save_shows_the_store_error();
    void closing_saves_again();
    void open_saves_only_changed_autofill_mappings();
};

// VM1: AnkiManager.loadProfile, apply, loadTransport and AnkiView bindings.
void AnkiSettingsViewModelTest::open_projects_profile_settings_and_shared_lists() {
    Fixture f;
    auto& settings = f.store.settings();
    settings.tags = "日本語 custom";
    settings.allow_dupes = true;
    settings.compress_images = false;
    settings.compact_glossaries = true;
    settings.transport->api_key = "key";
    settings.transport->duplicate_scope = DuplicateScope::deck_root;
    settings.transport->check_all_models = true;
    QVERIFY(f.save());
    AnkiSettingsViewModel model;
    QSignalSpy changed(&model, &AnkiSettingsViewModel::settingsChanged);
    model.open();

    QVERIFY(!changed.isEmpty());
    QCOMPARE(model.address(), QString::fromStdString(f.anki.url()));
    QCOMPARE(model.api_key(), QStringLiteral("key"));
    QCOMPARE(model.selected_deck(), QStringLiteral("Reading"));
    QCOMPARE(model.selected_note_type(), QStringLiteral("Custom"));
    QCOMPARE(model.available_decks(), (QStringList{"Default", "Reading"}));
    QCOMPARE(model.available_note_types(), QStringList{"Custom"});
    QVERIFY(model.has_selected_note_type());
    QCOMPARE(model.fields(), (QStringList{"Word", "Sentence"}));
    QCOMPARE(model.field_mappings(), (QVariantMap{{"Word", "{expression}"}}));
    QCOMPARE(model.tags(), QStringLiteral("日本語 custom"));
    QVERIFY(model.allow_duplicates());
    QCOMPARE(model.duplicate_scope(), QStringLiteral("deckroot"));
    QVERIFY(model.check_all_models());
    QVERIFY(!model.compress_images());
    QVERIFY(model.compact_glossaries());
}

// VM1: AnkiManager.ensureAnkiConnectURL; Swift's default Address binding.
void AnkiSettingsViewModelTest::open_repairs_missing_and_whitespace_addresses() {
    Fixture f;
    QVERIFY(f.data.isValid());
    AnkiSettingsViewModel model;
    model.open();
    QCOMPARE(model.address(), default_address);
    QVERIFY(model.selected_deck().isEmpty());
    QVERIFY(model.selected_note_type().isEmpty());
    QVERIFY(model.field_mappings().isEmpty());
    QVERIFY(model.tags().isEmpty());
    QVERIFY(!model.allow_duplicates());
    QCOMPARE(model.duplicate_scope(), QStringLiteral("collection"));
    QVERIFY(!model.check_all_models());
    QVERIFY(model.compress_images());
    QVERIFY(model.available_decks().isEmpty());
    QVERIFY(model.available_note_types().isEmpty());
    model.close();

    f.store.settings().transport->url = "　 \t";
    QVERIFY(f.save());
    model.open();
    QCOMPARE(model.address(), default_address);
    model.close();
}

// VM2: AnkiManager text setters and testTextBindingsPersistEveryEdit.
void AnkiSettingsViewModelTest::text_edits_persist_immediately() {
    Fixture f;
    QVERIFY(f.save());
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);

    model.setAddress(QStringLiteral(" \thttp://127.0.0.1:1234/ \n"));
    auto persisted = f.saved();
    QVERIFY(persisted.transport);
    QCOMPARE(persisted.transport->url, std::optional<std::string>(" \thttp://127.0.0.1:1234/ \n"));
    model.setApiKey(QStringLiteral("  日本語 key  "));
    persisted = f.saved();
    QVERIFY(persisted.transport);
    QCOMPARE(persisted.transport->api_key, std::optional<std::string>("  日本語 key  "));
    model.setTags(QStringLiteral("  tag-one　タグ \t"));
    QCOMPARE(f.saved().tags, std::string("  tag-one　タグ \t"));
    model.setFieldMapping("Word", QStringLiteral("  {expression} 日本語 \n"));
    persisted = f.saved();
    QVERIFY(persisted.field_mappings.contains("Word"));
    QCOMPARE(persisted.field_mappings.at("Word"), std::string("  {expression} 日本語 \n"));
    model.setFieldMapping("Word", QStringLiteral(" \t　\n"));
    persisted = f.saved();
    QVERIFY(persisted.field_mappings.contains("Word"));
    QCOMPARE(persisted.field_mappings.at("Word"), std::string{});
}

// VM3: AnkiManager.loadProfile, activateProfile, loadTransport and
// ProfileRepository.ankiConfigURL; Linux applies the change on the next open.
void AnkiSettingsViewModelTest::next_open_loads_new_active_profile_but_keeps_shared_values() {
    Fixture f;
    f.store.settings().tags = "original tags";
    f.store.settings().transport->api_key = "shared";
    QVERIFY(f.save());
    const auto initial = f.profiles.active_profile().id;
    const auto other = f.profiles.create_profile("Other", ContentLanguage::japanese, initial);
    QVERIFY(other.profile);
    AnkiConfigStore other_store(data_root(), f.profiles);
    other_store.load();
    other_store.activate_profile(other.profile->id);
    other_store.settings().tags = "other tags";
    other_store.settings().selected_deck = "Default";
    other_store.settings().field_mappings = {{"Sentence", "{sentence}"}};
    other_store.settings().transport->duplicate_scope = DuplicateScope::deck;
    QVERIFY(other_store.save().ok);
    QVERIFY(!f.profiles.set_active_profile(initial));

    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    QCOMPARE(model.tags(), QStringLiteral("original tags"));
    QVERIFY(!f.profiles.set_active_profile(other.profile->id));
    QCOMPARE(model.tags(), QStringLiteral("original tags"));
    model.close();
    model.open();
    QCOMPARE(model.tags(), QStringLiteral("other tags"));
    QCOMPARE(model.selected_deck(), QStringLiteral("Default"));
    QCOMPARE(model.field_mappings(), (QVariantMap{{"Sentence", "{sentence}"}}));
    QCOMPARE(model.duplicate_scope(), QStringLiteral("deck"));
    QCOMPARE(model.address(), QString::fromStdString(f.anki.url()));
    QCOMPARE(model.api_key(), QStringLiteral("shared"));
    QCOMPARE(model.available_decks(), (QStringList{"Default", "Reading"}));
    QCOMPARE(model.available_note_types(), QStringList{"Custom"});
}

// VM4: AnkiView Connect button, AnkiManager.ensureAnkiConnectURL and save.
void AnkiSettingsViewModelTest::connect_repairs_blank_address_before_the_request() {
    Fixture f;
    QVERIFY(f.save());
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    model.setAddress(QStringLiteral("　\t"));
    QCOMPARE(model.address(), QStringLiteral("　\t"));
    model.connectToAnki();
    QCOMPARE(model.address(), default_address);
    QFile global(QString::fromStdString(f.store.global_config_path().string()));
    QVERIFY(global.open(QIODevice::ReadOnly));
    const auto contents = QJsonDocument::fromJson(global.readAll()).object();
    QCOMPARE(contents.value("ankiConnectConfig").toObject().value("url").toString(),
             default_address);
    model.close();
}

// VM4–VM5: AnkiView.onAppear and AnkiManager.refreshAnkiConnectStatus: even a
// null version result counts as success; Linux does not schedule retries.
void AnkiSettingsViewModelTest::open_and_connect_ping_without_retries() {
    Fixture f;
    QVERIFY(f.save());
    f.anki.responses["version"] = {{"error", "offline"}};
    f.anki.deferred_action = "version";
    AnkiSettingsViewModel model;
    QSignalSpy status(&model, &AnkiSettingsViewModel::connectedChanged);
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(f.anki.deferred_socket, 1500);
    QVERIFY(!model.connected());
    f.anki.release_response();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
    QTest::qWait(2100);
    QCOMPARE(f.anki.actions(), QStringList{"version"});
    QVERIFY(!model.connected());
    QVERIFY(status.isEmpty());

    f.anki.responses["version"] = {{"result", QJsonValue::Null}, {"error", QJsonValue::Null}};
    f.anki.deferred_action = "version";
    model.connectToAnki();
    QTRY_VERIFY_WITH_TIMEOUT(f.anki.deferred_socket, 1500);
    const auto profile_path = f.store.profile_config_path(f.profiles.active_profile().id);
    QVERIFY(profile_path);
    QVERIFY(std::filesystem::remove(*profile_path));
    f.anki.release_response();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    QTRY_VERIFY_WITH_TIMEOUT(std::filesystem::is_regular_file(*profile_path), 1500);
    QCOMPARE(f.anki.actions(), (QStringList{"version", "version"}));
    QVERIFY(!f.anki.requests.at(1).contains("params"));
    QCOMPARE(status.size(), 1);

    f.anki.deferred_action.clear();
    f.anki.responses["version"] = {{"error", "offline again"}};
    model.connectToAnki();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
    QVERIFY(!model.connected());
    QCOMPARE(status.size(), 2);
}

// VM6: AnkiManager.fetchAnkiConnect exits after the failed status request.
void AnkiSettingsViewModelTest::failed_fetch_ping_keeps_metadata_and_shows_error() {
    Fixture f;
    QVERIFY(f.save());
    const auto initial = f.saved();
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    f.anki.requests.clear();
    f.anki.responses["version"] = {{"error", "offline"}};
    model.fetchMetadata();
    QTRY_COMPARE_WITH_TIMEOUT(model.error_message(), fetch_not_connected, 1500);
    QCOMPARE(f.anki.actions(), QStringList{"version"});
    QVERIFY(!model.connected());
    QCOMPARE(f.saved(), initial);
}

// VM7: AnkiManager.fetchAnkiConnect short-circuits the two `[String]` casts.
void AnkiSettingsViewModelTest::fetch_short_circuits_on_a_bad_list_data() {
    QTest::addColumn<bool>("bad_decks");
    QTest::newRow("invalid decks stop before models") << true;
    QTest::newRow("invalid models stop before field requests") << false;
}

// VM7: AnkiManager.fetchAnkiConnect.
void AnkiSettingsViewModelTest::fetch_short_circuits_on_a_bad_list() {
    QFETCH(bool, bad_decks);
    Fixture f;
    QVERIFY(f.save());
    const auto initial = f.saved();
    f.anki.responses["deckNames"] = {
        {"result", bad_decks ? QJsonArray{"New", 3} : QJsonArray{"New"}}};
    f.anki.responses["modelNames"] = {{"result", QJsonObject{}}};
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    f.anki.requests.clear();
    model.fetchMetadata();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
    QCOMPARE(f.anki.actions(),
             bad_decks ? (QStringList{"version", "deckNames"})
                       : (QStringList{"version", "deckNames", "modelNames"}));
    QVERIFY(model.connected());
    QVERIFY(model.error_message().isEmpty());
    QCOMPARE(f.saved(), initial);
}

// VM7–VM10: AnkiManager.fetchAnkiConnect and applyFetchedAnkiMetadata.
void AnkiSettingsViewModelTest::fetch_requests_fields_in_order_and_applies_usable_metadata() {
    Fixture f;
    f.store.settings().field_mappings = {
        {"Sentence", ""}, {"Word", "{expression}"}, {"ExpressionReading", "　 "}};
    QVERIFY(f.save());
    f.anki.responses["deckNames"] = {{"result", QJsonArray{"dEfAuLt", "New", "Third"}}};
    f.anki.responses["modelNames"] = {{"result", QJsonArray{"Bad", "Empty", "Lapis", "Other"}}};
    f.anki.model_field_responses["Bad"] = {{"result", QJsonArray{"X", 1}}};
    f.anki.model_field_responses["Empty"] = {{"result", QJsonArray{}}};
    f.anki.model_field_responses["Lapis"] = {
        {"result", QJsonArray{"Sentence", "Expression", "ExpressionReading"}}};
    f.anki.model_field_responses["Other"] = {{"result", QJsonArray{"Other Field"}}};
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    f.anki.requests.clear();
    model.fetchMetadata();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
    QCOMPARE(f.anki.actions(),
             (QStringList{"version", "deckNames", "modelNames", "modelFieldNames",
                          "modelFieldNames", "modelFieldNames", "modelFieldNames"}));
    for (qsizetype i = 0; i < 3; ++i) {
        QVERIFY(!f.anki.requests.at(i).contains("params"));
    }
    const QStringList models{"Bad", "Empty", "Lapis", "Other"};
    for (qsizetype i = 0; i < models.size(); ++i) {
        QCOMPARE(f.anki.requests.at(i + 3).value("params").toObject(),
                 (QJsonObject{{"modelName", models.at(i)}}));
    }
    QCOMPARE(model.available_decks(), (QStringList{"dEfAuLt", "New", "Third"}));
    QCOMPARE(model.available_note_types(), (QStringList{"Lapis", "Other"}));
    QCOMPARE(model.selected_deck(), QStringLiteral("New"));
    QCOMPARE(model.selected_note_type(), QStringLiteral("Lapis"));
    QCOMPARE(model.fields(), (QStringList{"Sentence", "Expression", "ExpressionReading"}));
    const auto persisted = f.saved();
    QCOMPARE(persisted.available_decks, (std::vector<std::string>{"dEfAuLt", "New", "Third"}));
    QCOMPARE(persisted.available_note_types,
             (std::vector<niratan::anki::NoteType>{
                 {"Lapis", {"Sentence", "Expression", "ExpressionReading"}},
                 {"Other", {"Other Field"}}}));
    QCOMPARE(persisted.selected_deck, std::optional<std::string>("New"));
    QCOMPARE(persisted.selected_note_type, std::optional<std::string>("Lapis"));
    QCOMPARE(persisted.field_mappings,
             (decltype(persisted.field_mappings){{"Sentence", ""},
                                                 {"Expression", "{expression}"},
                                                 {"ExpressionReading", "{reading}"}}));
}

// VM8: AnkiManager.applyFetchedAnkiMetadata rejects metadata with no usable models.
void AnkiSettingsViewModelTest::fetch_with_no_usable_models_keeps_saved_metadata() {
    Fixture f;
    QVERIFY(f.save());
    const auto initial = f.saved();
    f.anki.responses["deckNames"] = {{"result", QJsonArray{"New"}}};
    f.anki.responses["modelNames"] = {{"result", QJsonArray{"Empty"}}};
    f.anki.model_field_responses["Empty"] = {{"result", QJsonArray{}}};
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    model.fetchMetadata();
    QTRY_COMPARE_WITH_TIMEOUT(model.error_message(), empty_metadata, 1500);
    QVERIFY(model.connected());
    QCOMPARE(model.available_decks(), (QStringList{"Default", "Reading"}));
    QCOMPARE(model.available_note_types(), QStringList{"Custom"});
    QCOMPARE(f.saved(), initial);
}

// VM21: AnkiManager.fetchAnkiConnect's catch and AnkiView's Error alert.
void AnkiSettingsViewModelTest::fetch_request_error_is_shown_until_dismissed() {
    Fixture f;
    QVERIFY(f.save());
    f.anki.responses["deckNames"] = {{"error", "Deck lookup failed."}};
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    model.fetchMetadata();
    QTRY_COMPARE_WITH_TIMEOUT(model.error_message(), QStringLiteral("Deck lookup failed."), 1500);
    QVERIFY(!model.connected());
    model.connectToAnki();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    QCOMPARE(model.error_message(), QStringLiteral("Deck lookup failed."));
    model.dismissError();
    QVERIFY(model.error_message().isEmpty());
}

// VM11: AnkiView's Deck picker calls AnkiManager.save without autofilling.
void AnkiSettingsViewModelTest::selecting_deck_saves_without_touching_mappings() {
    Fixture f;
    QVERIFY(f.save());
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    model.setSelectedDeck("Default");
    const auto persisted = f.saved();
    QCOMPARE(persisted.selected_deck, std::optional<std::string>("Default"));
    QCOMPARE(persisted.field_mappings, f.store.settings().field_mappings);
}

// VM12: AnkiView's Model picker, AnkiManager.autofillFieldMappings and
// AnkiFieldTemplate.autofilledMappings, including its unknown-template return.
void AnkiSettingsViewModelTest::selecting_model_autofills_template_and_keeps_custom_mappings() {
    Fixture f;
    f.store.settings().available_note_types.push_back(
        {"Lapis", {"Sentence", "Expression", "ExpressionReading", "Picture"}});
    f.store.settings().field_mappings = {
        {"Word", "old"}, {"Expression", "custom"}, {"ExpressionReading", "　\t"}, {"Sentence", ""}};
    QVERIFY(f.save());
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    model.setSelectedNoteType("Lapis");
    const auto lapis = f.saved();
    QCOMPARE(lapis.selected_note_type, std::optional<std::string>("Lapis"));
    QCOMPARE(lapis.field_mappings,
             (decltype(lapis.field_mappings){{"Sentence", ""},
                                             {"Expression", "custom"},
                                             {"ExpressionReading", "{reading}"},
                                             {"Picture", "{book-cover}"}}));
    model.setSelectedNoteType("Custom");
    const auto other = f.saved();
    QCOMPARE(other.selected_note_type, std::optional<std::string>("Custom"));
    QCOMPARE(other.field_mappings, lapis.field_mappings);
}

// VM13–VM16: AnkiView checkbox and DuplicateScope picker save bindings,
// including Compact Glossaries under the accepted media-switch decision.
void AnkiSettingsViewModelTest::toggles_and_duplicate_scope_save_immediately() {
    Fixture f;
    QVERIFY(f.save());
    QFile profile;
    const auto path = f.store.profile_config_path(f.profiles.active_profile().id);
    QVERIFY(path);
    profile.setFileName(QString::fromStdString(path->string()));
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    model.setAllowDuplicates(true);
    QVERIFY(f.saved().allow_dupes);
    for (const QString scope : {"deck", "deckroot", "collection"}) {
        model.setDuplicateScope(scope);
        QVERIFY(profile.open(QIODevice::ReadOnly));
        const auto saved = QJsonDocument::fromJson(profile.readAll()).object();
        QCOMPARE(saved.value("duplicateScope").toString(), scope);
        profile.close();
    }
    model.setCheckAllModels(true);
    const auto persisted = f.saved();
    QVERIFY(persisted.transport);
    QVERIFY(persisted.transport->check_all_models);
    model.setCompressImages(false);
    QVERIFY(!f.saved().compress_images);
    model.setCompactGlossaries(true);
    QVERIFY(f.saved().compact_glossaries);
}

// VM17–VM18: AnkiView.availableHandlebars, its field menu and the clear button.
void AnkiSettingsViewModelTest::mapping_menu_uses_active_dictionaries_and_saves_choices() {
    Fixture f;
    QVERIFY(f.save());
    const auto dictionary = niratan::storage::dictionaries_root() / "国語 [A]";
    QVERIFY(std::filesystem::create_directories(dictionary));
    QVERIFY(write_bytes(
        dictionary / "index.json",
        R"({"title":"国語 [A]","format":3,"revision":"1","isUpdatable":false,"indexUrl":"","downloadUrl":""})"));
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    const auto options = model.mapping_options();
    QVERIFY(!options.isEmpty());
    const QString option = QStringLiteral("{single-glossary-国語 [A]}");
    QCOMPARE(options.last(), option);
    model.setFieldMapping("Word", option);
    auto persisted = f.saved();
    QVERIFY(persisted.field_mappings.contains("Word"));
    QCOMPARE(persisted.field_mappings.at("Word"), option.toStdString());
    model.setFieldMapping("Word", {});
    persisted = f.saved();
    QVERIFY(persisted.field_mappings.contains("Word"));
    QVERIFY(persisted.field_mappings.at("Word").empty());
}

// VM19: AnkiView Apply Defaults alert, AnkiManager.applyDefaultFieldMappings,
// and AnkiFieldTemplate.hasDefaults, appliedDefaultMappings and clearsMapping.
void AnkiSettingsViewModelTest::defaults_require_a_supported_model_and_save_only_changes() {
    Fixture f;
    f.store.settings().available_note_types.insert(
        f.store.settings().available_note_types.end(),
        {{"Lapis", {"Expression", "DefinitionPicture", "Word"}},
         {"Kiku", {"Expression"}},
         {"Senren", {"word"}},
         {"lapis", {"Expression"}}});
    QVERIFY(f.save());
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    for (const QString name : {"Kiku", "Senren", "Lapis"}) {
        model.setSelectedNoteType(name);
        QVERIFY(model.has_field_defaults());
    }
    for (const QString name : {"lapis", "Custom"}) {
        model.setSelectedNoteType(name);
        QVERIFY(!model.has_field_defaults());
    }
    model.setSelectedNoteType("Lapis");
    model.setFieldMapping("Expression", "override");
    model.setFieldMapping("DefinitionPicture", "legacy");
    model.setFieldMapping("Word", "custom field");
    model.setFieldMapping("Unlisted", "stale");
    model.applyDefaults();
    const auto persisted = f.saved();
    QCOMPARE(persisted.field_mappings,
             (decltype(persisted.field_mappings){{"Expression", "{expression}"},
                                                 {"Word", "custom field"}}));

    const auto path = f.store.profile_config_path(f.profiles.active_profile().id);
    QVERIFY(path);
    QVERIFY(write_bytes(*path, "{"));
    model.applyDefaults();
    QVERIFY(model.error_message().isEmpty());
}

// VM20: AnkiManager.ankiConnectRequest, duplicateLookup and addNoteAnkiConnect
// use the same saved transport as the settings request.
void AnkiSettingsViewModelTest::next_settings_and_mining_requests_use_saved_transport() {
    Fixture f;
    QVERIFY(f.save());
    AnkiStub updated;
    QVERIFY(updated.server.isListening());
    updated.responses["version"] = {{"result", 6}};
    MiningViewModel mining;
    QSignalSpy replies(&mining, &MiningViewModel::replyReady);
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    f.anki.requests.clear();
    model.setAddress(QString::fromStdString(updated.url()));
    model.setApiKey(QStringLiteral(" key 日本語 "));
    model.connectToAnki();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
    QCOMPARE(updated.actions(), QStringList{"version"});
    QCOMPARE(updated.requests.at(0).value("key").toString(), QStringLiteral(" key 日本語 "));

    mining.duplicateCheck(1, QStringLiteral("読む"));
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    QCOMPARE(updated.actions(), (QStringList{"version", "canAddNotesWithErrorDetail"}));
    QCOMPARE(updated.requests.at(1).value("key").toString(), QStringLiteral(" key 日本語 "));

    model.setApiKey(QStringLiteral("next key"));
    model.setAllowDuplicates(true);
    mining.mineEntry(2, QVariantMap{{"expression", "読む"}, {"matched", "読む"}});
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 2, 1500);
    QCOMPARE(updated.actions(), (QStringList{"version", "canAddNotesWithErrorDetail", "addNote"}));
    QCOMPARE(updated.requests.at(2).value("key").toString(), QStringLiteral("next key"));
    QVERIFY(f.anki.requests.isEmpty());
}

// VM21: AnkiView error presentation with the accepted Linux save-failure policy.
void AnkiSettingsViewModelTest::a_failed_save_shows_the_store_error() {
    Fixture f;
    QVERIFY(f.save());
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    const auto path = f.store.profile_config_path(f.profiles.active_profile().id);
    QVERIFY(path);
    QVERIFY(write_bytes(*path, "{"));
    const auto expected = f.store.save();
    QVERIFY(!expected.ok);
    QVERIFY(!expected.error.empty());
    QSignalSpy errors(&model, &AnkiSettingsViewModel::errorMessageChanged);
    model.setTags("cannot save");
    QCOMPARE(model.error_message(), QString::fromStdString(expected.error));
    QVERIFY(!errors.isEmpty());
    model.dismissError();
    QVERIFY(model.error_message().isEmpty());
}

// VM22: AnkiView.onDisappear calls AnkiManager.save even without a new edit.
void AnkiSettingsViewModelTest::closing_saves_again() {
    Fixture f;
    QVERIFY(f.save());
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(model.connected(), 1500);
    const auto path = f.store.profile_config_path(f.profiles.active_profile().id);
    QVERIFY(path);
    QVERIFY(std::filesystem::remove(*path));
    model.close();
    QVERIFY(std::filesystem::is_regular_file(*path));
    QCOMPARE(f.saved().selected_deck, std::optional<std::string>("Reading"));
}

// VM23: AnkiManager.init and activateProfile autofillFieldMappings, saving
// only if it returns true. A failed version cannot account for that save.
void AnkiSettingsViewModelTest::open_saves_only_changed_autofill_mappings() {
    Fixture f;
    f.store.settings().selected_note_type = "Lapis";
    f.store.settings().available_note_types = {{"Lapis", {"Expression", "Sentence"}}};
    f.store.settings().field_mappings = {{"Sentence", ""}};
    f.anki.responses["version"] = {{"error", "offline"}};
    QVERIFY(f.save());
    AnkiSettingsViewModel model;
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
    QCOMPARE(f.anki.actions(), QStringList{"version"});
    QVERIFY(!model.connected());
    const auto filled = f.saved();
    QCOMPARE(filled.field_mappings,
             (decltype(filled.field_mappings){{"Expression", "{expression}"}, {"Sentence", ""}}));
    QCOMPARE(model.field_mappings(),
             (QVariantMap{{"Expression", "{expression}"}, {"Sentence", ""}}));
    model.close();

    const auto initial = f.profiles.active_profile().id;
    const auto profile = f.profiles.create_profile("Other", ContentLanguage::japanese, initial);
    QVERIFY(profile.profile);
    const auto other_path = f.store.profile_config_path(profile.profile->id);
    QVERIFY(other_path);
    QFile other(QString::fromStdString(other_path->string()));
    QVERIFY(other.open(QIODevice::ReadOnly));
    const QByteArray unchanged = other.readAll() + "\n  \n";
    other.close();
    QVERIFY(write_bytes(*other_path, unchanged));
    QVERIFY(!f.profiles.set_active_profile(profile.profile->id));
    model.open();
    QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 1500);
    QCOMPARE(f.anki.actions(), (QStringList{"version", "version"}));
    QVERIFY(!model.connected());
    QCOMPARE(f.saved().field_mappings, filled.field_mappings);
    QVERIFY(other.open(QIODevice::ReadOnly));
    QCOMPARE(other.readAll(), unchanged);
}

QTEST_GUILESS_MAIN(AnkiSettingsViewModelTest)
#include "tst_anki_settings_view_model.moc"
