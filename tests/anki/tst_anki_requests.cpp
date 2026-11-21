// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/anki_requests.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QtTest>

#include <string>
#include <vector>

using niratan::anki::AddNoteOutcome;
using niratan::anki::AnkiConnectResult;
using niratan::anki::CanAddVerdict;
using niratan::anki::DuplicateScope;
using niratan::anki::parse_add_note_result;
using niratan::anki::parse_can_add_result;
using niratan::anki::parse_find_notes_result;
using niratan::anki::parse_media_dir_path_result;
using niratan::anki::parse_string_array_result;
using niratan::anki::Request;
using niratan::anki::request_succeeded;

// Core/AnkiManager.swift (ankiConnectRequest(action:params:) and its call
// sites) has no test script for the wire, so the cases follow the request and
// result handling directly. Models/Anki.swift supplies DuplicateScope,
// AnkiConnectConfig, AnkiNoteType and AnkiDuplicateLookupResult.
namespace {

AnkiConnectResult ok_result(QJsonValue value) {
    AnkiConnectResult result;
    result.ok = true;
    result.result = std::move(value);
    return result;
}

AnkiConnectResult failed_result(std::string error) {
    AnkiConnectResult result;
    result.ok = false;
    result.error = std::move(error);
    return result;
}

QJsonObject can_add_note(const Request& request) {
    if (!request.params.has_value()) {
        return {};
    }
    const QJsonArray notes = request.params->value("notes").toArray();
    if (notes.isEmpty()) {
        return {};
    }
    return notes.at(0).toObject();
}

QJsonObject can_add_options(DuplicateScope scope, bool check_all_models) {
    const Request request = niratan::anki::can_add_notes_request(
        "日本語::語彙::N1", "Lapis", "Expression", "決定", scope, check_all_models);
    return can_add_note(request).value("options").toObject();
}

}  // namespace

class AnkiRequestsTest : public QObject {
    Q_OBJECT

   private slots:
    // B1: AnkiManager.swift ankiConnectRequest call sites.
    void b1_action_strings_are_exact();

    // B2: refreshAnkiConnectStatus, getMediaDirPath, fetchAnkiConnect,
    // syncAnkiConnect.
    void b2_paramless_actions_send_no_params();

    // B3: fetchAnkiConnect (modelFieldNames).
    void b3_model_field_names_params_are_the_model_name();

    // B4: duplicateNoteIDs (findNotes).
    void b4_find_notes_params_are_the_query();

    // B5: addNoteAnkiConnect (addNote).
    void b5_add_note_params_are_the_note_unchanged();

    // B6: addNoteAnkiConnect (storeMediaFile).
    void b6_store_media_file_params_are_name_and_base64();

    // B7: duplicateLookup (canAddNotesWithErrorDetail note shape).
    void b7_can_add_note_has_exactly_four_members();
    void b7_names_and_word_pass_through_unchanged();

    // B8: duplicateLookup and Models/Anki.swift
    // DuplicateScope.ankiConnectOptions.
    void b8_duplicate_options_follow_the_scope();
    void b8_duplicate_options_never_carry_allow_duplicate();

    // B9: openNotesInAnki.
    void b9_gui_browse_query_keeps_positive_distinct_ids();
    void b9_gui_browse_sends_nothing_without_a_valid_id();

    // B10: addNoteAnkiConnect.
    void b10_add_note_returns_a_positive_number_id();
    void b10_add_note_unusable_results_carry_no_id();
    void b10_add_note_request_error_is_distinguishable();

    // B11: duplicateNoteIDs.
    void b11_find_notes_reads_numbers_and_decimal_strings();
    void b11_find_notes_drops_invalid_and_non_positive_ids();
    void b11_find_notes_failure_is_distinct_from_an_empty_list();

    // B12: duplicateLookup (canAddNotesWithErrorDetail result).
    void b12_can_add_reads_the_first_boolean();
    void b12_can_add_indeterminate_shapes();

    // B17: fetchAnkiConnect (deckNames, modelNames).
    void b17_deck_and_model_names_need_a_string_array();

    // B18: fetchAnkiConnect (modelFieldNames).
    void b18_model_field_names_need_a_string_array();

    // B19: getMediaDirPath.
    void b19_media_dir_path_needs_a_string();

    // B20: refreshAnkiConnectStatus, syncAnkiConnect, openNotesInAnki.
    void b20_ignored_results_only_need_the_request_to_succeed();
};

void AnkiRequestsTest::b1_action_strings_are_exact() {
    QCOMPARE(niratan::anki::version_request().action, std::string("version"));
    QCOMPARE(niratan::anki::media_dir_path_request().action, std::string("getMediaDirPath"));
    QCOMPARE(niratan::anki::deck_names_request().action, std::string("deckNames"));
    QCOMPARE(niratan::anki::model_names_request().action, std::string("modelNames"));
    QCOMPARE(niratan::anki::model_field_names_request("Lapis").action,
             std::string("modelFieldNames"));
    QCOMPARE(
        niratan::anki::can_add_notes_request("D", "M", "F", "w", DuplicateScope::collection, false)
            .action,
        std::string("canAddNotesWithErrorDetail"));
    QCOMPARE(niratan::anki::find_notes_request("q").action, std::string("findNotes"));
    QCOMPARE(niratan::anki::add_note_request(QJsonObject{}).action, std::string("addNote"));
    QCOMPARE(niratan::anki::store_media_file_request("f", QByteArray()).action,
             std::string("storeMediaFile"));
    const auto browse = niratan::anki::gui_browse_request({1});
    QVERIFY(browse.has_value());
    QCOMPARE(browse->action, std::string("guiBrowse"));
    QCOMPARE(niratan::anki::sync_request().action, std::string("sync"));
}

void AnkiRequestsTest::b2_paramless_actions_send_no_params() {
    QCOMPARE(niratan::anki::version_request(), (Request{"version", std::nullopt}));
    QCOMPARE(niratan::anki::media_dir_path_request(), (Request{"getMediaDirPath", std::nullopt}));
    QCOMPARE(niratan::anki::deck_names_request(), (Request{"deckNames", std::nullopt}));
    QCOMPARE(niratan::anki::model_names_request(), (Request{"modelNames", std::nullopt}));
    QCOMPARE(niratan::anki::sync_request(), (Request{"sync", std::nullopt}));
}

void AnkiRequestsTest::b3_model_field_names_params_are_the_model_name() {
    const Request request = niratan::anki::model_field_names_request("Lapis 日本語");
    QVERIFY(request.params.has_value());
    QCOMPARE(*request.params, (QJsonObject{{"modelName", "Lapis 日本語"}}));
}

void AnkiRequestsTest::b4_find_notes_params_are_the_query() {
    const Request request = niratan::anki::find_notes_request(R"q("deck:D" "expression:決定")q");
    QVERIFY(request.params.has_value());
    QCOMPARE(*request.params, (QJsonObject{{"query", R"q("deck:D" "expression:決定")q"}}));
}

void AnkiRequestsTest::b5_add_note_params_are_the_note_unchanged() {
    const QJsonObject note{
        {"deckName", "日本語"},
        {"modelName", "Lapis"},
        {"fields", QJsonObject{{"Expression", "決定"}, {"Sentence", "決定した"}}},
        {"options", QJsonObject{{"allowDuplicate", true}}},
        {"tags", QJsonArray{"tag"}},
    };
    const Request request = niratan::anki::add_note_request(note);
    QVERIFY(request.params.has_value());
    QCOMPARE(*request.params, (QJsonObject{{"note", note}}));
}

void AnkiRequestsTest::b6_store_media_file_params_are_name_and_base64() {
    // Two bytes whose base64 needs both '+', '/' and padding: "+/A=".
    const Request request =
        niratan::anki::store_media_file_request("画像 1.png", QByteArray::fromHex("fbf0"));
    QVERIFY(request.params.has_value());
    QCOMPARE(request.params->size(), 2);
    QCOMPARE(request.params->value("filename").toString(), QString("画像 1.png"));
    QCOMPARE(request.params->value("data").toString(), QString("+/A="));

    const Request short_request = niratan::anki::store_media_file_request("a", QByteArray("a"));
    QVERIFY(short_request.params.has_value());
    QCOMPARE(short_request.params->value("data").toString(), QString("YQ=="));
    QVERIFY(!short_request.params->value("data").toString().contains('\n'));
}

void AnkiRequestsTest::b7_can_add_note_has_exactly_four_members() {
    const Request request = niratan::anki::can_add_notes_request(
        "日本語::語彙", "Lapis", "Expression", "決定", DuplicateScope::collection, false);
    QVERIFY(request.params.has_value());
    QCOMPARE(request.params->size(), 1);
    const QJsonArray notes = request.params->value("notes").toArray();
    QCOMPARE(notes.size(), 1);

    const QJsonObject note = notes.at(0).toObject();
    QCOMPARE(note.size(), 4);
    QVERIFY(note.contains("deckName"));
    QVERIFY(note.contains("modelName"));
    QVERIFY(note.contains("fields"));
    QVERIFY(note.contains("options"));
    QCOMPARE(note.value("deckName").toString(), QString("日本語::語彙"));
    QCOMPARE(note.value("modelName").toString(), QString("Lapis"));

    const QJsonObject fields = note.value("fields").toObject();
    QCOMPARE(fields.size(), 1);
    QCOMPARE(fields.value("Expression").toString(), QString("決定"));
}

void AnkiRequestsTest::b7_names_and_word_pass_through_unchanged() {
    const Request request = niratan::anki::can_add_notes_request(
        "D \"x\"", "M", "First Field", " 決定\u3000 ", DuplicateScope::collection, false);
    const QJsonObject note = can_add_note(request);
    QCOMPARE(note.value("deckName").toString(), QString("D \"x\""));
    QCOMPARE(note.value("modelName").toString(), QString("M"));
    const QJsonObject fields = note.value("fields").toObject();
    QCOMPARE(fields.size(), 1);
    QCOMPARE(fields.value("First Field").toString(), QString(" 決定\u3000 "));
}

void AnkiRequestsTest::b8_duplicate_options_follow_the_scope() {
    QCOMPARE(can_add_options(DuplicateScope::collection, false),
             (QJsonObject{{"duplicateScope", "collection"}}));
    QCOMPARE(can_add_options(DuplicateScope::deck, false),
             (QJsonObject{{"duplicateScope", "deck"}}));
    QCOMPARE(can_add_options(DuplicateScope::deck_root, false),
             (QJsonObject{{"duplicateScope", "deck"},
                          {"duplicateScopeOptions",
                           QJsonObject{{"deckName", "日本語"}, {"checkChildren", true}}}}));
    QCOMPARE(can_add_options(DuplicateScope::collection, true),
             (QJsonObject{{"duplicateScope", "collection"},
                          {"duplicateScopeOptions", QJsonObject{{"checkAllModels", true}}}}));
    QCOMPARE(can_add_options(DuplicateScope::deck_root, true),
             (QJsonObject{{"duplicateScope", "deck"},
                          {"duplicateScopeOptions",
                           QJsonObject{{"deckName", "日本語"},
                                       {"checkChildren", true},
                                       {"checkAllModels", true}}}}));
}

void AnkiRequestsTest::b8_duplicate_options_never_carry_allow_duplicate() {
    for (const DuplicateScope scope :
         {DuplicateScope::collection, DuplicateScope::deck, DuplicateScope::deck_root}) {
        for (const bool check_all_models : {false, true}) {
            const QJsonObject options = can_add_options(scope, check_all_models);
            QVERIFY(!options.contains("allowDuplicate"));
            QVERIFY(!options.contains("allowDupes"));
            QCOMPARE(options.size(),
                     check_all_models || scope == DuplicateScope::deck_root ? 2 : 1);
        }
    }
}

void AnkiRequestsTest::b9_gui_browse_query_keeps_positive_distinct_ids() {
    const auto request = niratan::anki::gui_browse_request({0, 7, -2, 7, 11, 11, 3});
    QVERIFY(request.has_value());
    QCOMPARE(request->action, std::string("guiBrowse"));
    QVERIFY(request->params.has_value());
    QCOMPARE(*request->params, (QJsonObject{{"query", "nid:7,11,3"}}));
}

void AnkiRequestsTest::b9_gui_browse_sends_nothing_without_a_valid_id() {
    QVERIFY(!niratan::anki::gui_browse_request({}).has_value());
    QVERIFY(!niratan::anki::gui_browse_request({0, -1, -9}).has_value());
    // A control: the absence above is a filter, not a builder that never sends.
    QVERIFY(niratan::anki::gui_browse_request({1}).has_value());
}

void AnkiRequestsTest::b10_add_note_returns_a_positive_number_id() {
    const AddNoteOutcome outcome = parse_add_note_result(ok_result(QJsonValue(1493872489123LL)));
    QVERIFY(outcome.note_id.has_value());
    QCOMPARE(*outcome.note_id, std::int64_t{1493872489123LL});
    QVERIFY(!outcome.request_failed);
    QVERIFY(outcome.error.empty());

    // NSNumber.int64Value truncates a fractional number toward zero.
    const AddNoteOutcome fractional = parse_add_note_result(ok_result(QJsonValue(9.9)));
    QVERIFY(fractional.note_id.has_value());
    QCOMPARE(*fractional.note_id, std::int64_t{9});
}

void AnkiRequestsTest::b10_add_note_unusable_results_carry_no_id() {
    const QJsonValue unusable[] = {
        QJsonValue(),                        // missing result
        QJsonValue(QJsonValue::Null),        // null
        QJsonValue(0),                       // zero
        QJsonValue(-5),                      // negative
        QJsonValue(-0.5),                    // truncates to zero
        QJsonValue(QString("123")),          // string
        QJsonValue(true),                    // boolean, which Swift reads as 1
        QJsonValue(QJsonObject{{"id", 1}}),  // object
    };
    for (const QJsonValue& value : unusable) {
        const AddNoteOutcome outcome = parse_add_note_result(ok_result(value));
        QVERIFY(!outcome.note_id.has_value());
        QVERIFY(!outcome.request_failed);
        QVERIFY(outcome.error.empty());
    }
    // A control: a usable result does carry an ID, so the drops above are
    // decisions rather than an empty parser.
    QVERIFY(parse_add_note_result(ok_result(QJsonValue(1))).note_id.has_value());
}

void AnkiRequestsTest::b10_add_note_request_error_is_distinguishable() {
    const AddNoteOutcome outcome = parse_add_note_result(failed_result("cannot create note"));
    QVERIFY(!outcome.note_id.has_value());
    QVERIFY(outcome.request_failed);
    QCOMPARE(outcome.error, std::string("cannot create note"));
}

void AnkiRequestsTest::b11_find_notes_reads_numbers_and_decimal_strings() {
    const QJsonArray values{1, QJsonValue(QString("2")), 3.9, QJsonValue(QString("+4")),
                            QJsonValue(QString("007"))};
    const auto ids = parse_find_notes_result(ok_result(values));
    QVERIFY(ids.has_value());
    QCOMPARE(*ids, (std::vector<std::int64_t>{1, 2, 3, 4, 7}));

    const auto extremes =
        parse_find_notes_result(ok_result(QJsonArray{QJsonValue(QString("9223372036854775807"))}));
    QVERIFY(extremes.has_value());
    QCOMPARE(*extremes, (std::vector<std::int64_t>{9223372036854775807LL}));
}

void AnkiRequestsTest::b11_find_notes_drops_invalid_and_non_positive_ids() {
    const QJsonArray values{
        0,
        -1,
        QJsonValue(QString("-2")),
        QJsonValue(QString("0")),
        QJsonValue(QString()),
        QJsonValue(QString(" 5")),
        QJsonValue(QString("5x")),
        QJsonValue(QString("9223372036854775808")),
        QJsonValue(true),
        QJsonValue(QJsonValue::Null),
        QJsonValue(QJsonObject{{"id", 3}}),
        QJsonValue(QJsonArray{4}),
        5,
        QJsonValue(QString("5")),
        5.0,
    };
    const auto ids = parse_find_notes_result(ok_result(values));
    QVERIFY(ids.has_value());
    QCOMPARE(*ids, (std::vector<std::int64_t>{5}));
}

void AnkiRequestsTest::b11_find_notes_failure_is_distinct_from_an_empty_list() {
    QVERIFY(!parse_find_notes_result(ok_result(QJsonValue())).has_value());
    QVERIFY(!parse_find_notes_result(ok_result(QJsonValue(QJsonValue::Null))).has_value());
    QVERIFY(!parse_find_notes_result(ok_result(QJsonValue(12))).has_value());
    QVERIFY(!parse_find_notes_result(ok_result(QJsonValue(QString("[1]")))).has_value());
    QVERIFY(!parse_find_notes_result(failed_result("boom")).has_value());

    // An empty array is a successful lookup with no match, not a failure.
    const auto empty = parse_find_notes_result(ok_result(QJsonArray{}));
    QVERIFY(empty.has_value());
    QVERIFY(empty->empty());

    // A control: an array is read, so the possible failure above is a shape
    // decision.
    const auto one = parse_find_notes_result(ok_result(QJsonArray{1}));
    QVERIFY(one.has_value());
    QCOMPARE(*one, (std::vector<std::int64_t>{1}));
}

void AnkiRequestsTest::b12_can_add_reads_the_first_boolean() {
    const QJsonArray can_add{
        QJsonObject{{"canAdd", true}, {"error", QJsonValue::Null}},
    };
    QCOMPARE(parse_can_add_result(ok_result(can_add)), CanAddVerdict::can_add);

    const QJsonArray cannot_add{QJsonObject{{"canAdd", false}}};
    QCOMPARE(parse_can_add_result(ok_result(cannot_add)), CanAddVerdict::cannot_add);

    // Only the first element is read.
    const QJsonArray mixed{QJsonObject{{"canAdd", false}}, QJsonObject{{"canAdd", true}}};
    QCOMPARE(parse_can_add_result(ok_result(mixed)), CanAddVerdict::cannot_add);
}

void AnkiRequestsTest::b12_can_add_indeterminate_shapes() {
    const QJsonValue shapes[] = {
        QJsonValue(),                                             // missing result
        QJsonValue(QJsonValue::Null),                             // null
        QJsonValue(QJsonObject{{"canAdd", true}}),                // not an array
        QJsonValue(QJsonArray{true}),                             // first is not an object
        QJsonValue(QJsonArray{QJsonObject{}}),                    // no canAdd
        QJsonValue(QJsonArray{QJsonObject{{"canAdd", 1}}}),       // number
        QJsonValue(QJsonArray{QJsonObject{{"canAdd", "true"}}}),  // string
        QJsonValue(QJsonArray{}),                                 // empty array
    };
    for (const QJsonValue& value : shapes) {
        QCOMPARE(parse_can_add_result(ok_result(value)), CanAddVerdict::indeterminate);
    }
    QCOMPARE(parse_can_add_result(failed_result("boom")), CanAddVerdict::indeterminate);
    // A control: a Boolean in the first element is read, not ignored.
    QCOMPARE(parse_can_add_result(ok_result(QJsonArray{QJsonObject{{"canAdd", true}}})),
             CanAddVerdict::can_add);
}

void AnkiRequestsTest::b17_deck_and_model_names_need_a_string_array() {
    const auto names =
        parse_string_array_result(ok_result(QJsonArray{QString("Default"), QString("日本語")}));
    QVERIFY(names.has_value());
    QCOMPARE(*names, (std::vector<std::string>{"Default", "日本語"}));

    const auto empty = parse_string_array_result(ok_result(QJsonArray{}));
    QVERIFY(empty.has_value());
    QVERIFY(empty->empty());

    QVERIFY(!parse_string_array_result(ok_result(QJsonArray{QString("a"), 1})).has_value());
    QVERIFY(!parse_string_array_result(ok_result(QJsonObject{{"a", 1}})).has_value());
    QVERIFY(!parse_string_array_result(ok_result(QJsonValue(QString("a")))).has_value());
    QVERIFY(!parse_string_array_result(ok_result(QJsonValue())).has_value());
}

void AnkiRequestsTest::b18_model_field_names_need_a_string_array() {
    const auto fields = parse_string_array_result(
        ok_result(QJsonArray{QString("Expression"), QString("Sentence")}));
    QVERIFY(fields.has_value());
    QCOMPARE(*fields, (std::vector<std::string>{"Expression", "Sentence"}));

    QVERIFY(
        !parse_string_array_result(ok_result(QJsonArray{QString("Expression"), 2})).has_value());
    QVERIFY(!parse_string_array_result(ok_result(QJsonValue(7))).has_value());
}

void AnkiRequestsTest::b19_media_dir_path_needs_a_string() {
    const auto path = parse_media_dir_path_result(
        ok_result(QJsonValue(QString("/home/u/.local/share/Anki2/User 1/collection.media"))));
    QVERIFY(path.has_value());
    QCOMPARE(*path, std::string("/home/u/.local/share/Anki2/User 1/collection.media"));

    QVERIFY(!parse_media_dir_path_result(ok_result(QJsonValue(1))).has_value());
    QVERIFY(!parse_media_dir_path_result(ok_result(QJsonValue(QJsonValue::Null))).has_value());
    QVERIFY(!parse_media_dir_path_result(ok_result(QJsonArray{QString("/tmp")})).has_value());
    QVERIFY(!parse_media_dir_path_result(ok_result(QJsonValue())).has_value());
    QVERIFY(!parse_media_dir_path_result(failed_result("boom")).has_value());
}

void AnkiRequestsTest::b20_ignored_results_only_need_the_request_to_succeed() {
    QVERIFY(request_succeeded(ok_result(QJsonValue(6))));
    QVERIFY(request_succeeded(ok_result(QJsonValue())));
    QVERIFY(request_succeeded(ok_result(QJsonValue(QJsonObject{}))));
    QVERIFY(!request_succeeded(failed_result("boom")));
}

QTEST_APPLESS_MAIN(AnkiRequestsTest)
#include "tst_anki_requests.moc"
