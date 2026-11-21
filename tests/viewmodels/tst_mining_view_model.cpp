// SPDX-License-Identifier: GPL-3.0-or-later
//
// PopupWebView.swift, PopupView.swift, AnkiMining.swift and AnkiManager.swift:
// exercise the host's commands through the view model and observe AnkiConnect
// traffic, replies and toast state without a real Anki instance.

#include "viewmodels/anki/mining_view_model.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include <filesystem>

#include "anki_stub.h"
#include "data_root.h"
#include "services/anki/anki_config.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"

using niratan::anki::AnkiConfigStore;
using niratan::profiles::ProfileRepository;
using niratan::test::added_id;
using niratan::test::AnkiStub;
using niratan::test::TempDataRoot;
using niratan::viewmodels::MiningViewModel;

namespace {

const QString added_id_text = QStringLiteral("9007199254740993");
const QString expression = QString::fromUtf8("読む");

struct Fixture {
    TempDataRoot data;
    AnkiStub anki;
    ProfileRepository profiles{niratan::storage::data_root()};
    AnkiConfigStore config{niratan::storage::data_root(), profiles};

    Fixture() {
        auto& settings = config.settings();
        settings.selected_deck = "Reading";
        settings.selected_note_type = "Model";
        settings.available_note_types = {{"Model", {"Word"}}};
        settings.field_mappings = {{"Word", "{expression}"}};
        settings.transport->url = anki.url();
    }

    bool save() { return data.isValid() && anki.server.isListening() && config.save().ok; }
};

QVariantMap content() {
    return {{"expression", expression}, {"matched", expression}};
}

QVariantMap mining_payload(const QString& status, const QString& message, const QString& id = {}) {
    QVariantMap payload{{"status", status}, {"message", message}};
    if (!id.isEmpty()) {
        payload.insert("noteID", id);
    }
    return payload;
}

QJsonObject dictionary_entry(const QString& file_name, int order, const QString& category,
                             bool is_enabled = true) {
    return {{"fileName", file_name},
            {"isEnabled", is_enabled},
            {"order", order},
            {"category", category}};
}

QJsonObject dictionary_config(const QJsonArray& terms) {
    return {{"termDictionaries", terms},
            {"frequencyDictionaries", QJsonArray{}},
            {"pitchDictionaries", QJsonArray{}}};
}

bool write_dictionary_config(const std::filesystem::path& path, const QJsonObject& document) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    QFile file(QString::fromStdString(path.string()));
    const QByteArray bytes = QJsonDocument(document).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

std::filesystem::path dictionary_config_path(const ProfileRepository& profiles,
                                             std::string_view profile_id) {
    const auto directory = profiles.profile_directory(profile_id);
    return directory ? *directory / "dictionary_config.json" : std::filesystem::path{};
}

}  // namespace

class MiningViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void malformed_request_bodies_have_no_payload();
    void duplicate_lookup_returns_decimal_ids();
    void open_note_filters_ids_in_first_seen_order();
    void missing_deck_or_model_short_circuits_data();
    void missing_deck_or_model_short_circuits();
    void preflight_duplicate_uses_first_id_data();
    void preflight_duplicate_uses_first_id();
    void profile_change_during_preflight_aborts_add();
    void add_uses_host_context_and_preflights_once();
    void add_failure_replies_with_service_message_data();
    void add_failure_replies_with_service_message();
    void feedback_updates_toast_and_ignores_malformed_input();
    void newer_toast_restarts_the_dismissal_timer();
    void context_confirmation_publishes_decimal_note_command();
    void popup_options_come_from_anki_configuration();
    void add_uses_ordered_profile_term_dictionaries();
    void term_dictionaries_refresh_with_the_active_profile();
    void popup_options_defaults_match_the_anki_manager();
    void popup_options_track_each_profile_value();
    void popup_options_follow_the_active_profile();
};

// B2, B3: PopupWebView.Coordinator.userContentController (async).
void MiningViewModelTest::malformed_request_bodies_have_no_payload() {
    Fixture f;
    QVERIFY(f.save());
    MiningViewModel model;
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    QSignalSpy toasts(&model, &MiningViewModel::toastChanged);
    model.mineEntry(1, QVariantMap{{"expression", expression}, {"reading", 7}});
    model.duplicateCheck(2, QVariantMap{{"expression", expression}});

    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 2, 1200);
    for (const auto& reply : replies) {
        QVERIFY(!reply.at(1).isValid() || reply.at(1).isNull());
    }
    QSet<int> ids{replies.at(0).at(0).toInt(), replies.at(1).at(0).toInt()};
    QCOMPARE(ids, (QSet<int>{1, 2}));
    QCOMPARE(toasts.size(), 0);
    QVERIFY(f.anki.requests.isEmpty());
}

// B3: AnkiManager.duplicateLookup(word:) and AnkiDuplicateLookupResult.webPayload.
void MiningViewModelTest::duplicate_lookup_returns_decimal_ids() {
    Fixture f;
    f.anki.responses["canAddNotesWithErrorDetail"] = {
        {"result", QJsonArray{QJsonObject{{"canAdd", false}}}}};
    QVERIFY(f.save());
    MiningViewModel model;
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.duplicateCheck(17, expression);

    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    QCOMPARE(replies.first().at(0).toInt(), 17);
    const QJsonObject expected{{"isDuplicate", true}, {"noteIDs", QJsonArray{added_id_text, "37"}}};
    QCOMPARE(QJsonValue::fromVariant(replies.first().at(1)).toObject(), expected);
    QCOMPARE(f.anki.actions(), (QStringList{"canAddNotesWithErrorDetail", "findNotes"}));
    QCOMPARE(f.anki.params("findNotes").value("query").toString(),
             QStringLiteral("\"note:Model\" \"word:") + expression + QStringLiteral("\""));
    const auto notes = f.anki.params("canAddNotesWithErrorDetail").value("notes").toArray();
    QCOMPARE(notes.size(), 1);
    QCOMPARE(notes.at(0).toObject().value("fields").toObject(),
             (QJsonObject{{"Word", expression}}));
}

// B4: PopupWebView.Coordinator.userContentController and AnkiManager.openNotesInAnki.
void MiningViewModelTest::open_note_filters_ids_in_first_seen_order() {
    Fixture f;
    QVERIFY(f.save());
    MiningViewModel model;
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.openAnkiNote(QStringLiteral("9"));
    model.openAnkiNote(QVariantList{QStringLiteral("42"), 7});
    model.openAnkiNote(QStringList{"junk", "0", "-1"});
    model.openAnkiNote(QStringList{"0037", "junk", added_id_text, "37", "-1", "+12", " 7", "0",
                                   "9223372036854775808", "0x10"});

    QTRY_COMPARE_WITH_TIMEOUT(f.anki.actions(), QStringList{"guiBrowse"}, 1500);
    QCOMPARE(f.anki.params("guiBrowse"), (QJsonObject{{"query", "nid:37,9007199254740993,12"}}));
    QTest::qWait(100);
    QCOMPARE(f.anki.actions(), QStringList{"guiBrowse"});
    QCOMPARE(replies.size(), 0);
}

// B6, B7, B11: preflightAnkiMining, AnkiMiningResult.webPayload and PopupView.onMine.
void MiningViewModelTest::missing_deck_or_model_short_circuits_data() {
    QTest::addColumn<bool>("missingDeck");
    QTest::newRow("missing deck") << true;
    QTest::newRow("missing model") << false;
}

void MiningViewModelTest::missing_deck_or_model_short_circuits() {
    QFETCH(bool, missingDeck);
    Fixture f;
    auto& settings = f.config.settings();
    (missingDeck ? settings.selected_deck : settings.selected_note_type).reset();
    QVERIFY(f.save());
    MiningViewModel model;
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    QString status_at_reply;
    connect(&model, &MiningViewModel::replyReady, &model,
            [&] { status_at_reply = model.toast_status(); });
    model.mineEntry(8, content());

    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1200);
    QCOMPARE(replies.first().at(0).toInt(), 8);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("failed", "Configure Anki deck and model first."));
    QCOMPARE(status_at_reply, QStringLiteral("failed"));
    QVERIFY(f.anki.requests.isEmpty());
}

// B2, B6, B7, B8: preflightAnkiMining/configured empty strings and absent expression.
void MiningViewModelTest::preflight_duplicate_uses_first_id_data() {
    QTest::addColumn<bool>("withIds");
    QTest::newRow("first ID") << true;
    QTest::newRow("no IDs") << false;
}

void MiningViewModelTest::preflight_duplicate_uses_first_id() {
    QFETCH(bool, withIds);
    Fixture f;
    f.config.settings().selected_deck = "";
    f.config.settings().selected_note_type = "";
    f.config.settings().available_note_types = {{"", {"Word"}}};
    f.anki.responses["canAddNotesWithErrorDetail"] = {
        {"result", QJsonArray{QJsonObject{{"canAdd", false}}}}};
    if (!withIds) {
        f.anki.responses["findNotes"] = {{"result", QJsonArray{}}};
    }
    QVERIFY(f.save());
    MiningViewModel model;
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(9, QVariantMap{{"extra", QStringLiteral("accepted")}});

    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("duplicate", "Already exists in Anki.", withIds ? added_id_text : ""));
    QCOMPARE(f.anki.actions(), (QStringList{"canAddNotesWithErrorDetail", "findNotes"}));
    const auto notes = f.anki.params("canAddNotesWithErrorDetail").value("notes").toArray();
    QCOMPARE(notes.size(), 1);
    auto note = notes.at(0).toObject();
    QCOMPARE(note.value("fields").toObject(), (QJsonObject{{"Word", "Entry"}}));
    QCOMPARE(note.value("deckName"), QJsonValue(""));
    QCOMPARE(note.value("modelName"), QJsonValue(""));
}

// B8, B9: PopupView.mineEntry and preflightAnkiMining recheck the captured profile.
void MiningViewModelTest::profile_change_during_preflight_aborts_add() {
    Fixture f;
    QVERIFY(f.save());
    const auto other = f.profiles.create_profile(
        "Other", niratan::profiles::ContentLanguage::japanese, f.profiles.active_profile().id);
    QVERIFY(other.profile.has_value());
    f.anki.deferred_action = "canAddNotesWithErrorDetail";
    MiningViewModel model;
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(3, content());
    QTRY_VERIFY_WITH_TIMEOUT(f.anki.deferred_socket, 1500);
    QVERIFY(!f.profiles.set_active_profile(other.profile->id).has_value());
    f.anki.release_response();

    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("failed", "The active Profile changed. Try adding the card again."));
    QCOMPARE(f.anki.actions(), QStringList{"canAddNotesWithErrorDetail"});
}

// B2, B6, B9–B11, B25: PopupView.mineEntry/onMine, mineAnkiEntry and webPayload.
void MiningViewModelTest::add_uses_host_context_and_preflights_once() {
    Fixture f;
    f.config.settings().field_mappings = {{"Word", "{expression}"},
                                          {"Selection", "{popup-selection-text}"},
                                          {"Sentence", "{sentence}"},
                                          {"Book", "{document-title}"},
                                          {"Cover text", "Cover={book-cover}"},
                                          {"Cover image", "{book-cover}"}};
    QVERIFY(f.save());
    const auto other = f.profiles.create_profile(
        "Other", niratan::profiles::ContentLanguage::japanese, std::nullopt);
    QVERIFY(other.profile.has_value());
    f.anki.deferred_action = "getMediaDirPath";
    const auto cover_path = f.data.path() + "/cover.png";
    {
        QFile cover(cover_path);
        QVERIFY(cover.open(QIODevice::WriteOnly));
        QCOMPARE(cover.write("cover"), 5);
    }
    MiningViewModel model;
    model.setContext(expression + QString::fromUtf8("。"), "Book", "http://localhost/cover.png",
                     cover_path);
    const QVariantMap pending = mining_payload("pending", QString::fromUtf8("Preparing card…"));
    model.miningFeedback(pending);
    QCOMPARE(model.toast_status(), QStringLiteral("pending"));

    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    QVariantMap toast_at_reply;
    connect(&model, &MiningViewModel::replyReady, &model, [&] {
        toast_at_reply =
            mining_payload(model.toast_status(), model.toast_message(), model.toast_note_id());
    });
    auto body = content();
    body.insert("popupSelectionText", QString::fromUtf8("選択"));
    model.mineEntry(4, body);
    QTRY_VERIFY_WITH_TIMEOUT(f.anki.deferred_socket, 1500);
    QVERIFY(!f.profiles.set_active_profile(other.profile->id).has_value());
    f.anki.release_response();

    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    const auto expected = mining_payload("added", "Added to Anki.", added_id_text);
    QCOMPARE(replies.first().at(0).toInt(), 4);
    QCOMPARE(replies.first().at(1).toMap(), expected);
    QCOMPARE(toast_at_reply, expected);
    QCOMPARE(f.anki.actions(),
             (QStringList{"canAddNotesWithErrorDetail", "getMediaDirPath", "addNote"}));
    const auto note = f.anki.params("addNote").value("note").toObject();
    const auto fields = note.value("fields").toObject();
    QCOMPARE(note.value("deckName").toString(), QStringLiteral("Reading"));
    QCOMPARE(fields.value("Word").toString(), expression);
    QCOMPARE(fields.value("Selection").toString(), QString::fromUtf8("選択"));
    QCOMPARE(fields.value("Sentence").toString(), "<b>" + expression + QString::fromUtf8("</b>。"));
    QCOMPARE(fields.value("Book").toString(), QStringLiteral("Book"));
    QCOMPARE(fields.value("Cover text").toString(),
             QStringLiteral("Cover=http://localhost/cover.png"));
    const auto picture = note.value("picture").toArray();
    QCOMPARE(picture.size(), 1);
    QCOMPARE(picture.at(0).toObject().value("data").toString(), QStringLiteral("Y292ZXI="));
    QCOMPARE(picture.at(0).toObject().value("fields").toArray(), QJsonArray{"Cover image"});
}

// B6, B8, B10: mineAnkiEntry and AnkiManager.addNoteAnkiConnect; allowDupes bypasses lookup.
void MiningViewModelTest::add_failure_replies_with_service_message_data() {
    QTest::addColumn<QJsonObject>("response");
    QTest::addColumn<QString>("message");
    QTest::newRow("AnkiConnect error")
        << QJsonObject{{"error", "deck is unavailable"}} << QStringLiteral("deck is unavailable");
    QTest::newRow("no note ID") << QJsonObject{{"result", QJsonValue::Null}}
                                << QStringLiteral("AnkiConnect did not return the added note ID.");
}

void MiningViewModelTest::add_failure_replies_with_service_message() {
    QFETCH(QJsonObject, response);
    QFETCH(QString, message);
    Fixture f;
    f.config.settings().allow_dupes = true;
    f.anki.responses["addNote"] = response;
    QVERIFY(f.save());
    MiningViewModel model;
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(5, content());

    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    QCOMPARE(replies.first().at(1).toMap(), mining_payload("failed", message));
    QCOMPARE(model.toast_status(), QStringLiteral("failed"));
    QCOMPARE(model.toast_message(), message);
    QVERIFY(model.toast_note_id().isEmpty());
    QCOMPARE(f.anki.actions(), QStringList{"addNote"});
}

// B5, B11, B24: PopupWebView.Coordinator.userContentController, PopupView.onMiningFeedback.
void MiningViewModelTest::feedback_updates_toast_and_ignores_malformed_input() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    MiningViewModel model;
    QSignalSpy toasts(&model, &MiningViewModel::toastChanged);
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.miningFeedback(QStringLiteral("added"));
    model.miningFeedback(QVariantMap{{"status", QStringLiteral("unknown")}});
    model.miningFeedback(QVariantMap{{"status", QStringLiteral("added")}, {"message", 7}});
    QCOMPARE(toasts.size(), 0);

    const auto preparing = QString::fromUtf8("Preparing card…");
    model.miningFeedback(mining_payload("pending", preparing));
    QCOMPARE(model.toast_status(), QStringLiteral("pending"));
    QCOMPARE(model.toast_message(), preparing);
    QCOMPARE(toasts.size(), 1);

    model.miningFeedback(QVariantMap{{"status", QStringLiteral("duplicate")},
                                     {"noteID", QStringLiteral("+09007199254740993")}});
    QCOMPARE(model.toast_status(), QStringLiteral("duplicate"));
    QVERIFY(model.toast_message().isEmpty());
    QCOMPARE(model.toast_note_id(), added_id_text);

    const auto failure = QStringLiteral("Unable to prepare the card: Unknown error");
    model.miningFeedback(mining_payload("failed", failure, "9223372036854775808"));
    QCOMPARE(model.toast_status(), QStringLiteral("failed"));
    QCOMPARE(model.toast_message(), failure);
    QVERIFY(model.toast_note_id().isEmpty());
    QCOMPARE(toasts.size(), 3);
    QCOMPARE(replies.size(), 0);
}

// B14: PopupView.showMiningToast cancels the earlier 2.2-second dismissal.
void MiningViewModelTest::newer_toast_restarts_the_dismissal_timer() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    MiningViewModel model;
    model.miningFeedback(mining_payload("pending", QString::fromUtf8("Preparing card…")));
    QCOMPARE(model.toast_status(), QStringLiteral("pending"));
    QTest::qWait(1400);
    model.miningFeedback(mining_payload("added", "Added to Anki.", added_id_text));
    QTest::qWait(1050);
    QCOMPARE(model.toast_status(), QStringLiteral("added"));
    QCOMPARE(model.toast_note_id(), added_id_text);
    QTRY_VERIFY_WITH_TIMEOUT(model.toast_status().isEmpty(), 2000);
}

// B19: PopupView.showAnkiNoteButton and PopupWebView.updateUIView.
void MiningViewModelTest::context_confirmation_publishes_decimal_note_command() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    MiningViewModel model;
    QSignalSpy commands(&model, &MiningViewModel::ankiNoteButtonRequested);
    model.show_anki_note_button(2, added_id);
    model.show_anki_note_button(2, added_id);
    QCOMPARE(commands.size(), 2);
    for (const auto& command : commands) {
        QCOMPARE(command.at(0).toInt(), 2);
        QCOMPARE(command.at(1).toString(), added_id_text);
    }
}

// B17, B23: PopupView.buildContent, AnkiManager.needsAudio/useAnkiConnect,
// and popup.js applyAnkiDuplicateLookup/miningContent use these globals.
void MiningViewModelTest::popup_options_come_from_anki_configuration() {
    Fixture f;
    auto& settings = f.config.settings();
    settings.allow_dupes = true;
    settings.embed_media = true;
    settings.compact_glossaries = true;
    settings.field_mappings["Audio"] = "{audio}";
    QVERIFY(f.save());
    MiningViewModel model;
    const auto options = model.popupMiningOptions();
    for (const auto& key :
         {"needsAudio", "allowDupes", "useAnkiConnect", "embedMedia", "compactGlossariesAnki"}) {
        QCOMPARE(options.value(key), QVariant(true));
    }
}

// B1, B2, B3, B7: DictionaryManager.collectDictionaries,
// AnkiManager.categoryGlossary and MiningAttempt.dictionaries.
void MiningViewModelTest::add_uses_ordered_profile_term_dictionaries() {
    Fixture f;
    f.config.settings().allow_dupes = true;
    f.config.settings().field_mappings = {{"Word", "{expression}"},
                                          {"Definition", "{monolingual-definition}"}};
    QVERIFY(f.save());

    std::error_code error;
    for (const char* name : {"A", "B", "C"}) {
        const auto directory = niratan::storage::dictionaries_root() / name;
        std::filesystem::create_directories(directory, error);
        QVERIFY(!error);
        const QJsonObject index{
            {"title", QString::fromUtf8(name)},
            {"format", 3},
            {"revision", "1"},
            {"isUpdatable", false},
            {"indexUrl", ""},
            {"downloadUrl", ""},
        };
        QFile index_file(QString::fromStdString((directory / "index.json").string()));
        QVERIFY(index_file.open(QIODevice::WriteOnly));
        const QByteArray bytes = QJsonDocument(index).toJson(QJsonDocument::Compact);
        QVERIFY(index_file.write(bytes) == bytes.size());
    }
    const auto config = dictionary_config(QJsonArray{
        dictionary_entry("C", 1, "monolingual", false),
        dictionary_entry("B", 2, "monolingual"),
        dictionary_entry("A", 3, "bilingual"),
    });
    QVERIFY(write_dictionary_config(
        dictionary_config_path(f.profiles, f.profiles.active_profile().id), config));

    MiningViewModel model;
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    QVariantMap body = content();
    body.insert("singleGlossaries",
                QString::fromStdString(R"({"A":"alpha","B":"beta","C":"gamma"})"));
    model.mineEntry(11, body);

    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(f.anki.actions(), QStringList{"addNote"});
    const auto note = f.anki.params("addNote").value("note").toObject();
    const auto fields = note.value("fields").toObject();
    QCOMPARE(fields.value("Word").toString(), expression);
    QCOMPARE(fields.value("Definition").toString(), QStringLiteral("gamma"));
}

// B5, B7: activateProfile and the per-profile dictionary_config.json; the list
// is read again before the next add.
void MiningViewModelTest::term_dictionaries_refresh_with_the_active_profile() {
    Fixture f;
    f.config.settings().allow_dupes = true;
    f.config.settings().field_mappings = {{"Word", "{expression}"},
                                          {"Definition", "{monolingual-definition}"}};
    QVERIFY(f.save());

    const std::string default_id = f.profiles.active_profile().id;
    const auto other = f.profiles.create_profile(
        "Other", niratan::profiles::ContentLanguage::japanese, default_id);
    QVERIFY(other.profile.has_value());
    f.config.activate_profile(other.profile->id);
    QVERIFY(f.save());
    QVERIFY(!f.profiles.set_active_profile(default_id).has_value());

    std::error_code error;
    for (const char* name : {"A", "B", "C"}) {
        const auto directory = niratan::storage::dictionaries_root() / name;
        std::filesystem::create_directories(directory, error);
        QVERIFY(!error);
        const QJsonObject index{
            {"title", QString::fromUtf8(name)},
            {"format", 3},
            {"revision", "1"},
            {"isUpdatable", false},
            {"indexUrl", ""},
            {"downloadUrl", ""},
        };
        QFile index_file(QString::fromStdString((directory / "index.json").string()));
        QVERIFY(index_file.open(QIODevice::WriteOnly));
        const QByteArray bytes = QJsonDocument(index).toJson(QJsonDocument::Compact);
        QVERIFY(index_file.write(bytes) == bytes.size());
    }
    QVERIFY(write_dictionary_config(dictionary_config_path(f.profiles, default_id),
                                    dictionary_config(QJsonArray{
                                        dictionary_entry("A", 0, "monolingual"),
                                    })));
    QVERIFY(write_dictionary_config(dictionary_config_path(f.profiles, other.profile->id),
                                    dictionary_config(QJsonArray{
                                        dictionary_entry("C", 0, "monolingual"),
                                    })));

    MiningViewModel model;
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    QVariantMap body = content();
    body.insert("singleGlossaries",
                QString::fromStdString(R"({"A":"alpha","B":"beta","C":"gamma"})"));
    model.mineEntry(12, body);
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    QCOMPARE(f.anki.params("addNote")
                 .value("note")
                 .toObject()
                 .value("fields")
                 .toObject()
                 .value("Definition")
                 .toString(),
             QStringLiteral("alpha"));

    QVERIFY(write_dictionary_config(dictionary_config_path(f.profiles, default_id),
                                    dictionary_config(QJsonArray{
                                        dictionary_entry("B", 0, "monolingual"),
                                    })));
    f.anki.requests.clear();
    replies.clear();
    model.mineEntry(14, body);
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    QCOMPARE(f.anki.params("addNote")
                 .value("note")
                 .toObject()
                 .value("fields")
                 .toObject()
                 .value("Definition")
                 .toString(),
             QStringLiteral("beta"));

    QVERIFY(!f.profiles.set_active_profile(other.profile->id).has_value());
    f.anki.requests.clear();
    replies.clear();
    model.mineEntry(13, body);
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 1500);
    QCOMPARE(f.anki.params("addNote")
                 .value("note")
                 .toObject()
                 .value("fields")
                 .toObject()
                 .value("Definition")
                 .toString(),
             QStringLiteral("gamma"));
}

// B17: AnkiManager's defaults when no Anki configuration was saved.
void MiningViewModelTest::popup_options_defaults_match_the_anki_manager() {
    TempDataRoot data;
    QVERIFY(data.isValid());
    MiningViewModel model;

    const auto options = model.popupMiningOptions();
    QCOMPARE(options.value("needsAudio"), QVariant(false));
    QCOMPARE(options.value("allowDupes"), QVariant(false));
    QCOMPARE(options.value("useAnkiConnect"), QVariant(true));
    QCOMPARE(options.value("embedMedia"), QVariant(false));
    QCOMPARE(options.value("compactGlossariesAnki"), QVariant(false));
}

// B9–B13: each global follows its own source; {sasayaki-audio} is not audio.
void MiningViewModelTest::popup_options_track_each_profile_value() {
    Fixture f;
    auto& settings = f.config.settings();
    settings.allow_dupes = true;
    settings.embed_media = true;
    settings.compact_glossaries = true;
    settings.field_mappings = {{"Audio", "{sasayaki-audio}"}};
    QVERIFY(f.save());
    MiningViewModel model;

    const auto options = model.popupMiningOptions();
    QCOMPARE(options.value("needsAudio"), QVariant(false));
    QCOMPARE(options.value("allowDupes"), QVariant(true));
    QCOMPARE(options.value("useAnkiConnect"), QVariant(true));
    QCOMPARE(options.value("embedMedia"), QVariant(true));
    QCOMPARE(options.value("compactGlossariesAnki"), QVariant(true));
}

// B14: activateProfile reloads the profile before popupMiningOptions computes
// the globals; a page never keeps the previous profile's values.
void MiningViewModelTest::popup_options_follow_the_active_profile() {
    Fixture f;
    auto& settings = f.config.settings();
    settings.allow_dupes = true;
    settings.embed_media = true;
    settings.compact_glossaries = true;
    settings.field_mappings = {{"Audio", "{audio}"}};
    QVERIFY(f.save());

    const auto other = f.profiles.create_profile(
        "Other", niratan::profiles::ContentLanguage::japanese, f.profiles.active_profile().id);
    QVERIFY(other.profile.has_value());
    f.config.activate_profile(other.profile->id);
    auto& other_settings = f.config.settings();
    other_settings.allow_dupes = false;
    other_settings.embed_media = false;
    other_settings.compact_glossaries = false;
    other_settings.field_mappings = {{"Word", "{expression}"}};
    QVERIFY(f.save());
    QVERIFY(!f.profiles.set_active_profile(other.profile->id).has_value());

    MiningViewModel model;
    const auto options = model.popupMiningOptions();
    QCOMPARE(options.value("needsAudio"), QVariant(false));
    QCOMPARE(options.value("allowDupes"), QVariant(false));
    QCOMPARE(options.value("useAnkiConnect"), QVariant(true));
    QCOMPARE(options.value("embedMedia"), QVariant(false));
    QCOMPARE(options.value("compactGlossariesAnki"), QVariant(false));
}

QTEST_GUILESS_MAIN(MiningViewModelTest)
#include "tst_mining_view_model.moc"
