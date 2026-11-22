// SPDX-License-Identifier: GPL-3.0-or-later
//
// PopupWebView.swift, PopupView.swift, AnkiMining.swift and AnkiManager.swift:
// exercise the host's commands through the view model and observe AnkiConnect
// traffic, replies and toast state without a real Anki instance.

#include "viewmodels/anki/mining_view_model.h"

#include <QFile>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
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

bool write_file(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(bytes) == bytes.size();
}

QString fake_ffmpeg(const QString& directory) {
    const QString path = QDir(directory).filePath(QStringLiteral("ffmpeg"));
    write_file(path,
               "#!/bin/sh\n"
               "for last; do :; done\n"
               "printf '%s\\n' \"$@\" > \"$ARGS_FILE\"\n"
               "printf '%s' \"$FAKE_OUTPUT\" > \"$last\"\n"
               "exit ${FAKE_EXIT:-0}\n");
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

QString sha1_hex(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
}

QString deterministic_name(const QString& prefix, const QString& path, qint64 start_ms,
                           qint64 end_ms, const QString& tail) {
    return prefix + sha1_hex(path.toUtf8()) + "_" + QString::number(start_ms) + "-" +
        QString::number(end_ms) + tail;
}

QVariantMap video_context(const QString& path, const QString& title) {
    return {{"path", path},
            {"title", title},
            {"audioStreamIndex", 3},
            {"cueText", QString::fromUtf8("現在。")},
            {"cueStart", 3.0},
            {"cueEnd", 4.0},
            {"previousCueText", QString::fromUtf8("前。")},
            {"nextCueText", QString::fromUtf8("次。")},
            {"subtitleDelay", 0.0},
            {"duration", 30.0}};
}

QStringList recorded_arguments(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    if (!lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }
    return lines;
}

QString value_after(const QStringList& arguments, const QString& flag) {
    const auto index = arguments.indexOf(flag);
    return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString{};
}

// Answers screenshotRequested the way the video page does: write the still and
// report success or failure.
struct ScreenshotWriter {
    QByteArray bytes = QByteArray("frame");
    bool ok = true;
    QString path;
    int calls = 0;

    void attach(MiningViewModel& model) {
        QObject::connect(&model, &MiningViewModel::screenshotRequested, &model,
                         [this, &model](const QString& requested) {
                             ++calls;
                             path = requested;
                             if (ok) {
                                 write_file(requested, bytes);
                             }
                             model.screenshotFinished(ok);
                         });
    }
};

}  // namespace

class MiningViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void init() {
        qputenv("FAKE_OUTPUT", "clip-bytes");
        qputenv("FAKE_EXIT", "0");
    }

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
    void video_screenshot_is_written_to_the_media_directory_data();
    void video_screenshot_is_written_to_the_media_directory();
    void video_audio_clip_is_exported_and_attached();
    void video_capture_needs_flags_data();
    void video_capture_needs_flags();
    void video_preflight_precedes_capture_data();
    void video_preflight_precedes_capture();
    void video_invalid_audio_range_fails_the_add();
    void video_screenshot_failure_fails_the_add();
    void video_audio_failure_fails_the_add();
    void video_capture_falls_back_to_uploads();
    void video_direct_media_is_reused();
    void video_context_fields_use_the_title();
    void video_context_is_cleared_by_reader_context();
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

void MiningViewModelTest::video_screenshot_is_written_to_the_media_directory_data() {
    QTest::addColumn<bool>("compress");
    QTest::addColumn<int>("format");
    QTest::addColumn<QString>("suffix");
    QTest::newRow("compressed jpeg")
        << true << int(niratan::anki::ImageCompressionFormat::jpeg) << ".jpg";
    QTest::newRow("compressed avif")
        << true << int(niratan::anki::ImageCompressionFormat::avif) << ".jpg";
    QTest::newRow("uncompressed") << false << int(niratan::anki::ImageCompressionFormat::jpeg)
                                  << ".png";
}

void MiningViewModelTest::video_screenshot_is_written_to_the_media_directory() {
    QFETCH(bool, compress);
    QFETCH(int, format);
    QFETCH(QString, suffix);
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    f.anki.responses["getMediaDirPath"] = {{"result", media.path()}};
    auto& settings = f.config.settings();
    settings.compress_images = compress;
    settings.image_compression_format = static_cast<niratan::anki::ImageCompressionFormat>(format);
    settings.field_mappings = {{"Word", "{expression}"}, {"Frame", "{video-screenshot}"}};
    QVERIFY(f.save());

    MiningViewModel model;
    ScreenshotWriter writer;
    writer.attach(model);
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(21, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(writer.calls, 1);
    QVERIFY(writer.path.endsWith(suffix));
    const auto expected = deterministic_name("hoshi_video_frame_", path, 3000, 4000, suffix);
    QVERIFY(QFileInfo::exists(media.filePath(expected)));
    QVERIFY(!QFileInfo::exists(writer.path));
    const auto fields =
        f.anki.params("addNote").value("note").toObject().value("fields").toObject();
    QCOMPARE(fields.value("Frame").toString(), QStringLiteral("<img src=\"%1\">").arg(expected));
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 1);
}

void MiningViewModelTest::video_audio_clip_is_exported_and_attached() {
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    f.anki.responses["getMediaDirPath"] = {{"result", media.path()}};
    const QString args_file = f.data.path() + QStringLiteral("/args.txt");
    qputenv("ARGS_FILE", args_file.toUtf8());
    auto& settings = f.config.settings();
    settings.field_mappings = {{"Word", "{expression}"}, {"Clip", "{video-audio-clip}"}};
    settings.audio_compression_format = niratan::anki::AudioCompressionFormat::mp3;
    settings.audio_compression_bitrate_kbps = 96;
    QVERIFY(f.save());

    MiningViewModel model;
    model.set_ffmpeg_path(fake_ffmpeg(f.data.path()));
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(22, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    const auto expected = deterministic_name("hoshi_video_audio_", path, 2880, 4120, "_96k.mp3");
    QVERIFY(QFileInfo::exists(media.filePath(expected)));
    const auto arguments = recorded_arguments(args_file);
    QCOMPARE(value_after(arguments, QStringLiteral("-ss")), QStringLiteral("2.880000"));
    QCOMPARE(value_after(arguments, QStringLiteral("-t")), QStringLiteral("1.240000"));
    QCOMPARE(value_after(arguments, QStringLiteral("-map")), QStringLiteral("0:3"));
    QCOMPARE(value_after(arguments, QStringLiteral("-c:a")), QStringLiteral("libmp3lame"));
    QCOMPARE(value_after(arguments, QStringLiteral("-b:a")), QStringLiteral("96k"));
    QVERIFY(arguments.last().endsWith(QStringLiteral(".mp3")));
    const auto fields =
        f.anki.params("addNote").value("note").toObject().value("fields").toObject();
    QCOMPARE(fields.value("Clip").toString(), QStringLiteral("[sound:%1]").arg(expected));
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 1);
}

void MiningViewModelTest::video_capture_needs_flags_data() {
    QTest::addColumn<QString>("mapping");
    QTest::newRow("screenshot") << "screenshot";
    QTest::newRow("audio") << "audio";
    QTest::newRow("neither") << "neither";
}

void MiningViewModelTest::video_capture_needs_flags() {
    QFETCH(QString, mapping);
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    f.anki.responses["getMediaDirPath"] = {{"result", media.path()}};
    const QString args_file = f.data.path() + QStringLiteral("/args.txt");
    qputenv("ARGS_FILE", args_file.toUtf8());
    auto& settings = f.config.settings();
    settings.field_mappings = {{"Word", "{expression}"}};
    if (mapping == "screenshot") {
        settings.field_mappings["Frame"] = "{video-screenshot}";
    } else if (mapping == "audio") {
        settings.field_mappings["Clip"] = "{video-audio-clip}";
    }
    QVERIFY(f.save());

    MiningViewModel model;
    model.set_ffmpeg_path(fake_ffmpeg(f.data.path()));
    ScreenshotWriter writer;
    writer.attach(model);
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(23, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(writer.calls, mapping == "screenshot" ? 1 : 0);
    QCOMPARE(QFileInfo::exists(args_file), mapping == "audio");
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), mapping == "neither" ? 0 : 1);
}

void MiningViewModelTest::video_preflight_precedes_capture_data() {
    QTest::addColumn<QString>("verdict");
    QTest::newRow("duplicate") << "duplicate";
    QTest::newRow("missing-deck") << "missing-deck";
}

void MiningViewModelTest::video_preflight_precedes_capture() {
    QFETCH(QString, verdict);
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    f.anki.responses["getMediaDirPath"] = {{"result", media.path()}};
    auto& settings = f.config.settings();
    settings.field_mappings = {{"Word", "{expression}"}, {"Frame", "{video-screenshot}"}};
    if (verdict == "duplicate") {
        f.anki.responses["canAddNotesWithErrorDetail"] = {
            {"result", QJsonArray{QJsonObject{{"canAdd", false}}}}};
    } else {
        settings.selected_deck.reset();
    }
    QVERIFY(f.save());

    MiningViewModel model;
    ScreenshotWriter writer;
    writer.attach(model);
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(24, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             verdict == "duplicate"
                 ? mining_payload("duplicate", "Already exists in Anki.", added_id_text)
                 : mining_payload("failed", "Configure Anki deck and model first."));
    QCOMPARE(writer.calls, 0);
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 0);

    // A passing preflight reaches the capture.
    f.anki.responses["canAddNotesWithErrorDetail"] = {
        {"result", QJsonArray{QJsonObject{{"canAdd", true}}}}};
    if (verdict != "duplicate") {
        settings.selected_deck = "Reading";
    }
    QVERIFY(f.save());
    f.anki.requests.clear();
    replies.clear();
    writer.calls = 0;
    model.mineEntry(25, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(writer.calls, 1);
}

void MiningViewModelTest::video_invalid_audio_range_fails_the_add() {
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    f.anki.responses["getMediaDirPath"] = {{"result", media.path()}};
    const QString args_file = f.data.path() + QStringLiteral("/args.txt");
    qputenv("ARGS_FILE", args_file.toUtf8());
    auto& settings = f.config.settings();
    settings.field_mappings = {{"Word", "{expression}"}, {"Clip", "{video-audio-clip}"}};
    QVERIFY(f.save());

    MiningViewModel model;
    model.set_ffmpeg_path(fake_ffmpeg(f.data.path()));
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    auto context = video_context(path, QStringLiteral("Episode 1"));
    context.insert("duration", 2.5);
    model.setVideoContext(context);
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(26, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("failed", "Unable to capture the subtitle audio clip."));
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 1);
    QVERIFY(!f.anki.actions().contains("addNote"));
    QVERIFY(!QFileInfo::exists(args_file));

    // A range that survives the padding exports as usual.
    replies.clear();
    f.anki.requests.clear();
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    model.mineEntry(27, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QVERIFY(QFileInfo::exists(args_file));
}

void MiningViewModelTest::video_screenshot_failure_fails_the_add() {
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    f.anki.responses["getMediaDirPath"] = {{"result", media.path()}};
    auto& settings = f.config.settings();
    settings.field_mappings = {{"Word", "{expression}"}, {"Frame", "{video-screenshot}"}};
    QVERIFY(f.save());

    MiningViewModel model;
    ScreenshotWriter writer;
    writer.ok = false;
    writer.attach(model);
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(28, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("failed", "Unable to capture the video screenshot."));
    QCOMPARE(writer.calls, 1);
    QVERIFY(!f.anki.actions().contains("addNote"));

    writer.ok = true;
    replies.clear();
    f.anki.requests.clear();
    model.mineEntry(29, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(writer.calls, 2);
}

void MiningViewModelTest::video_audio_failure_fails_the_add() {
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    f.anki.responses["getMediaDirPath"] = {{"result", media.path()}};
    const QString args_file = f.data.path() + QStringLiteral("/args.txt");
    qputenv("ARGS_FILE", args_file.toUtf8());
    qputenv("FAKE_EXIT", "1");
    auto& settings = f.config.settings();
    settings.field_mappings = {{"Word", "{expression}"}, {"Clip", "{video-audio-clip}"}};
    QVERIFY(f.save());

    MiningViewModel model;
    model.set_ffmpeg_path(fake_ffmpeg(f.data.path()));
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(30, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("failed", "Unable to capture the subtitle audio clip."));
    QVERIFY(!f.anki.actions().contains("addNote"));

    qputenv("FAKE_EXIT", "0");
    replies.clear();
    f.anki.requests.clear();
    model.mineEntry(31, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
}

void MiningViewModelTest::video_capture_falls_back_to_uploads() {
    Fixture f;
    const QString args_file = f.data.path() + QStringLiteral("/args.txt");
    qputenv("ARGS_FILE", args_file.toUtf8());
    auto& settings = f.config.settings();
    settings.field_mappings = {
        {"Word", "{expression}"}, {"Frame", "{video-screenshot}"}, {"Clip", "{video-audio-clip}"}};
    QVERIFY(f.save());

    MiningViewModel model;
    model.set_ffmpeg_path(fake_ffmpeg(f.data.path()));
    ScreenshotWriter writer;
    writer.attach(model);
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(32, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 1);
    QCOMPARE(writer.calls, 1);
    const auto note = f.anki.params("addNote").value("note").toObject();
    const auto picture = note.value("picture").toArray();
    QCOMPARE(picture.size(), 1);
    QCOMPARE(picture.at(0).toObject().value("filename").toString(),
             QStringLiteral("hoshi_video_frame_") + sha1_hex(QByteArray("frame")) +
                 QStringLiteral(".jpg"));
    QCOMPARE(picture.at(0).toObject().value("fields").toArray(), QJsonArray{"Frame"});
    const auto audio = note.value("audio").toArray();
    QCOMPARE(audio.size(), 1);
    QCOMPARE(audio.at(0).toObject().value("filename").toString(),
             QStringLiteral("hoshi_video_audio_") + sha1_hex(QByteArray("clip-bytes")) +
                 QStringLiteral(".m4a"));
    QCOMPARE(audio.at(0).toObject().value("fields").toArray(), QJsonArray{"Clip"});
}

void MiningViewModelTest::video_direct_media_is_reused() {
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    f.anki.responses["getMediaDirPath"] = {{"result", media.path()}};
    const QString args_file = f.data.path() + QStringLiteral("/args.txt");
    qputenv("ARGS_FILE", args_file.toUtf8());
    auto& settings = f.config.settings();
    settings.field_mappings = {
        {"Word", "{expression}"}, {"Frame", "{video-screenshot}"}, {"Clip", "{video-audio-clip}"}};
    QVERIFY(f.save());

    MiningViewModel model;
    model.set_ffmpeg_path(fake_ffmpeg(f.data.path()));
    ScreenshotWriter writer;
    writer.attach(model);
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(33, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    const auto frame_name = deterministic_name("hoshi_video_frame_", path, 3000, 4000, ".jpg");
    const auto audio_name = deterministic_name("hoshi_video_audio_", path, 2880, 4120, ".m4a");
    auto first_fields =
        f.anki.params("addNote").value("note").toObject().value("fields").toObject();
    QCOMPARE(first_fields.value("Frame").toString(),
             QStringLiteral("<img src=\"%1\">").arg(frame_name));
    QCOMPARE(first_fields.value("Clip").toString(), QStringLiteral("[sound:%1]").arg(audio_name));

    f.anki.requests.clear();
    replies.clear();
    writer.calls = 0;
    QVERIFY(QFile::remove(args_file));
    model.mineEntry(34, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(writer.calls, 0);
    QVERIFY(!QFileInfo::exists(args_file));
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 0);
    QCOMPARE(f.anki.actions().count("addNote"), 1);
    const auto second_fields =
        f.anki.params("addNote").value("note").toObject().value("fields").toObject();
    QCOMPARE(second_fields.value("Frame").toString(),
             QStringLiteral("<img src=\"%1\">").arg(frame_name));
    QCOMPARE(second_fields.value("Clip").toString(), QStringLiteral("[sound:%1]").arg(audio_name));
}

void MiningViewModelTest::video_context_fields_use_the_title() {
    Fixture f;
    auto& settings = f.config.settings();
    settings.field_mappings = {
        {"Word", "{expression}"},         {"Sentence", "{sentence}"},
        {"Title", "{document-title}"},    {"File", "{video-file-name}"},
        {"Sub", "{video-subtitle}"},      {"Start", "{video-cue-start}"},
        {"End", "{video-cue-end}"},       {"Prev", "{video-previous-subtitle}"},
        {"Next", "{video-next-subtitle}"}};
    QVERIFY(f.save());

    MiningViewModel model;
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    QSignalSpy replies(&model, &MiningViewModel::replyReady);
    model.mineEntry(35, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 0);
    const auto fields =
        f.anki.params("addNote").value("note").toObject().value("fields").toObject();
    QCOMPARE(fields.value("Sentence").toString(), QString::fromUtf8("現在。"));
    QCOMPARE(fields.value("Title").toString(), QStringLiteral("Episode 1"));
    QCOMPARE(fields.value("File").toString(), QStringLiteral("Episode 1"));
    QCOMPARE(fields.value("Sub").toString(), QString::fromUtf8("現在。"));
    QCOMPARE(fields.value("Start").toString(), QStringLiteral("0:00:03.000"));
    QCOMPARE(fields.value("End").toString(), QStringLiteral("0:00:04.000"));
    QCOMPARE(fields.value("Prev").toString(), QString::fromUtf8("前。"));
    QCOMPARE(fields.value("Next").toString(), QString::fromUtf8("次。"));
    QVERIFY(!fields.value("File").toString().contains(QStringLiteral(".mkv")));
}

void MiningViewModelTest::video_context_is_cleared_by_reader_context() {
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    f.anki.responses["getMediaDirPath"] = {{"result", media.path()}};
    auto& settings = f.config.settings();
    settings.field_mappings = {
        {"Word", "{expression}"}, {"Frame", "{video-screenshot}"}, {"Sentence", "{sentence}"}};
    QVERIFY(f.save());

    MiningViewModel model;
    ScreenshotWriter writer;
    writer.attach(model);
    const QString path = f.data.path() + QStringLiteral("/Show 01.mkv");
    QSignalSpy replies(&model, &MiningViewModel::replyReady);

    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    model.mineEntry(36, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(writer.calls, 1);
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 1);

    // The reader/dictionary context clears the video context.
    f.anki.requests.clear();
    replies.clear();
    writer.calls = 0;
    model.setContext(QStringLiteral("reader sentence"), QStringLiteral("Book"), {}, {});
    model.mineEntry(37, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(writer.calls, 0);
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 0);
    auto note = f.anki.params("addNote").value("note").toObject();
    auto fields = note.value("fields").toObject();
    QCOMPARE(fields.value("Sentence").toString(), QStringLiteral("reader sentence"));
    QVERIFY(!note.contains("picture"));

    f.anki.requests.clear();
    replies.clear();
    model.setVideoContext(video_context(path, QStringLiteral("Episode 1")));
    model.clearVideoContext();
    model.mineEntry(38, content());
    QTRY_COMPARE_WITH_TIMEOUT(replies.size(), 1, 5000);
    QCOMPARE(replies.first().at(1).toMap(),
             mining_payload("added", "Added to Anki.", added_id_text));
    QCOMPARE(writer.calls, 0);
    QCOMPARE(f.anki.actions().count("getMediaDirPath"), 0);
}

QTEST_GUILESS_MAIN(MiningViewModelTest)
#include "tst_mining_view_model.moc"
