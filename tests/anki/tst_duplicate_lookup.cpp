// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/duplicate_lookup.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "services/anki/anki_requests.h"
#include "services/anki/duplicate_query.h"

using niratan::anki::AnkiConnectClient;
using niratan::anki::AnkiConnectResult;
using niratan::anki::AnkiRequestFunction;
using niratan::anki::AnkiSettings;
using niratan::anki::DuplicateLookupResult;
using niratan::anki::DuplicateLookupService;
using niratan::anki::DuplicateScope;
using niratan::anki::Request;

// Core/AnkiManager.swift (duplicateLookup(word:), duplicateNoteIDs(matching:),
// duplicateSearchQuery, loadWords, addWord, saveWords, the ankiWords constant
// and AnkiDuplicateLookupResult) and Models/Anki.swift (DuplicateScope) define
// these cases. script/test_popup_duplicate_button_rendering.swift only asserts
// the signatures, which anki_requests and this module cover.
namespace {

constexpr std::string_view kWord = "決定";
constexpr std::string_view kDeck = "日本語::語彙";
constexpr std::string_view kNoteType = "Lapis";
constexpr std::string_view kFirstField = "Expression";

std::filesystem::path temp_root(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString());
}

bool write_bytes(const std::filesystem::path& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(bytes) == bytes.size();
}

QByteArray read_bytes(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

AnkiConnectResult ok_result(QJsonValue value) {
    AnkiConnectResult result;
    result.ok = true;
    result.result = std::move(value);
    return result;
}

AnkiConnectResult failed_result() {
    AnkiConnectResult result;
    result.ok = false;
    result.error = "connection refused";
    return result;
}

// The shape duplicateLookup reads: the first element's canAdd Boolean.
AnkiConnectResult can_add(bool value) {
    return ok_result(QJsonArray{QJsonObject{{"canAdd", value}, {"error", ""}}});
}

AnkiSettings base_settings() {
    AnkiSettings settings;
    settings.selected_deck = std::string(kDeck);
    settings.selected_note_type = std::string(kNoteType);
    settings.available_note_types = {
        {std::string(kNoteType), {"Expression", "Sentence"}},
        {"Kiku", {"Word"}},
    };
    return settings;
}

// Records each request and answers from the given list, position by position.
// A request without a prepared answer gets a failed result.
class FakeAnki {
   public:
    std::vector<Request> requests;
    std::vector<AnkiConnectResult> answers;

    AnkiRequestFunction function() {
        return [this](std::string action, std::optional<QJsonObject> params,
                      AnkiConnectClient::Callback callback) {
            requests.push_back({.action = std::move(action), .params = std::move(params)});
            AnkiConnectResult answer;
            if (requests.size() <= answers.size()) {
                answer = answers.at(requests.size() - 1);
            }
            callback(std::move(answer));
        };
    }
};

struct Outcome {
    int calls = 0;
    DuplicateLookupResult result;
};

Outcome run_lookup(DuplicateLookupService& duplicate, std::string_view word,
                   const AnkiSettings& settings, FakeAnki& anki) {
    Outcome outcome;
    duplicate.lookup(word, settings, anki.function(), [&outcome](DuplicateLookupResult result) {
        ++outcome.calls;
        outcome.result = result;
    });
    return outcome;
}

}  // namespace

class DuplicateLookupTest : public QObject {
    Q_OBJECT

   private slots:
    // B9: duplicateLookup (the guard before any request).
    void b9_missing_note_type_answers_the_saved_words();
    void b9_note_type_must_exist_in_the_available_note_types();
    void b9_note_type_needs_a_field();
    void b9_missing_deck_answers_the_saved_words();
    void b9_empty_selected_strings_count_as_selected();

    // B10: duplicateLookup (canAddNotesWithErrorDetail).
    void b10_request_is_can_add_for_the_selected_values();
    void b10_request_follows_the_duplicate_scope_setting();
    void b10_first_field_is_the_note_types_first_field();
    void b10_reads_the_verdict_only();

    // B11: duplicateLookup (can_add).
    void b11_can_add_is_not_a_duplicate_for_a_saved_word();

    // B12: duplicateLookup and duplicateNoteIDs(matching:).
    void b12_cannot_add_records_the_word_and_reports_the_found_ids();
    void b12_find_notes_query_follows_duplicate_search_query();
    void b12_empty_find_notes_result_is_a_duplicate_without_ids();

    // B13: duplicateLookup (indeterminate verdict and failed request).
    void b13_indeterminate_verdicts_answer_the_saved_words();

    // B14: duplicateLookup and duplicateNoteIDs(matching:) (unusable findNotes).
    void b14_failed_find_notes_is_a_duplicate_without_ids();
    void b14_non_array_find_notes_result_is_a_duplicate_without_ids();
    void b14_failed_find_notes_keeps_the_in_memory_word_when_the_save_failed();

    // B15: loadWords, addWord, saveWords and the ankiWords constant.
    void b15_saved_words_path_is_anki_words_json();
    void b15_missing_file_is_an_empty_set();
    void b15_reads_a_json_string_array();
    void b15_undecodable_files_are_an_empty_set();
    void b15_a_damaged_file_is_replaced_by_the_next_save();
    void b15_add_inserts_and_saves_the_set();
    void b15_add_keeps_existing_words_and_deduplicates();
    void b15_an_empty_word_is_stored();
    void b15_add_reports_a_failed_save();
};

void DuplicateLookupTest::b9_missing_note_type_answers_the_saved_words() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    QVERIFY(write_bytes(niratan::anki::saved_words_path(root), QByteArray(R"(["決定"])")));
    DuplicateLookupService duplicate(root);

    AnkiSettings settings = base_settings();
    settings.selected_note_type = std::nullopt;
    FakeAnki anki;

    const Outcome outcome = run_lookup(duplicate, kWord, settings, anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, (DuplicateLookupResult{true, {}}));
    QVERIFY(anki.requests.empty());
}

void DuplicateLookupTest::b9_note_type_must_exist_in_the_available_note_types() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    AnkiSettings settings = base_settings();
    settings.selected_note_type = "Kiku";
    settings.available_note_types = {{"Lapis", {"Expression"}}};
    FakeAnki anki;

    const Outcome outcome = run_lookup(duplicate, kWord, settings, anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, DuplicateLookupResult{});
    QVERIFY(anki.requests.empty());
}

void DuplicateLookupTest::b9_note_type_needs_a_field() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    QVERIFY(write_bytes(niratan::anki::saved_words_path(root), QByteArray(R"(["決定"])")));
    DuplicateLookupService duplicate(root);

    AnkiSettings settings = base_settings();
    settings.available_note_types = {{"Lapis", {}}};
    FakeAnki anki;

    const Outcome outcome = run_lookup(duplicate, kWord, settings, anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, (DuplicateLookupResult{true, {}}));
    QVERIFY(anki.requests.empty());
}

void DuplicateLookupTest::b9_missing_deck_answers_the_saved_words() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    QVERIFY(write_bytes(niratan::anki::saved_words_path(root), QByteArray(R"(["猫"])")));
    DuplicateLookupService duplicate(root);

    AnkiSettings settings = base_settings();
    settings.selected_deck = std::nullopt;
    FakeAnki anki;

    const Outcome outcome = run_lookup(duplicate, kWord, settings, anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, DuplicateLookupResult{});
    QVERIFY(anki.requests.empty());
}

void DuplicateLookupTest::b9_empty_selected_strings_count_as_selected() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    AnkiSettings settings = base_settings();
    settings.selected_deck = "";
    settings.selected_note_type = "";
    settings.available_note_types = {{"", {"Expression"}}};
    FakeAnki anki;
    anki.answers = {can_add(true)};

    const Outcome outcome = run_lookup(duplicate, kWord, settings, anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, DuplicateLookupResult{});
    QCOMPARE(anki.requests.size(), std::size_t{1});
    QVERIFY(anki.requests.size() == 1);
    QCOMPARE(anki.requests.front(),
             niratan::anki::can_add_notes_request("", "", "Expression", kWord,
                                                  DuplicateScope::collection, false));
}

void DuplicateLookupTest::b10_request_is_can_add_for_the_selected_values() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    FakeAnki anki;
    anki.answers = {can_add(true)};

    const Outcome outcome = run_lookup(duplicate, kWord, base_settings(), anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, DuplicateLookupResult{});
    QCOMPARE(anki.requests.size(), std::size_t{1});
    QVERIFY(anki.requests.size() == 1);
    QCOMPARE(anki.requests.front(),
             niratan::anki::can_add_notes_request(kDeck, kNoteType, kFirstField, kWord,
                                                  DuplicateScope::collection, false));
}

void DuplicateLookupTest::b10_request_follows_the_duplicate_scope_setting() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    AnkiSettings deck = base_settings();
    QVERIFY(deck.transport.has_value());
    deck.transport->duplicate_scope = DuplicateScope::deck;
    FakeAnki deck_anki;
    deck_anki.answers = {can_add(true)};

    run_lookup(duplicate, kWord, deck, deck_anki);
    QCOMPARE(deck_anki.requests.size(), std::size_t{1});
    QVERIFY(deck_anki.requests.size() == 1);
    QCOMPARE(deck_anki.requests.front(),
             niratan::anki::can_add_notes_request(kDeck, kNoteType, kFirstField, kWord,
                                                  DuplicateScope::deck, false));

    AnkiSettings collection = base_settings();
    QVERIFY(collection.transport.has_value());
    collection.transport->duplicate_scope = DuplicateScope::collection;
    collection.transport->check_all_models = true;
    FakeAnki collection_anki;
    collection_anki.answers = {can_add(true)};

    run_lookup(duplicate, kWord, collection, collection_anki);
    QCOMPARE(collection_anki.requests.size(), std::size_t{1});
    QVERIFY(collection_anki.requests.size() == 1);
    QCOMPARE(collection_anki.requests.front(),
             niratan::anki::can_add_notes_request(kDeck, kNoteType, kFirstField, kWord,
                                                  DuplicateScope::collection, true));

    // Swift's nil ankiConnectConfig sends the deck scope here.
    AnkiSettings unset = base_settings();
    unset.transport = std::nullopt;
    FakeAnki unset_anki;
    unset_anki.answers = {can_add(true)};

    run_lookup(duplicate, kWord, unset, unset_anki);
    QCOMPARE(unset_anki.requests.size(), std::size_t{1});
    QVERIFY(unset_anki.requests.size() == 1);
    QCOMPARE(unset_anki.requests.front(),
             niratan::anki::can_add_notes_request(kDeck, kNoteType, kFirstField, kWord,
                                                  DuplicateScope::deck, false));
}

void DuplicateLookupTest::b10_first_field_is_the_note_types_first_field() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    AnkiSettings settings = base_settings();
    settings.available_note_types = {{"Lapis", {"Reading", "Expression"}}};
    FakeAnki anki;
    anki.answers = {can_add(true)};

    run_lookup(duplicate, kWord, settings, anki);
    QCOMPARE(anki.requests.size(), std::size_t{1});
    QVERIFY(anki.requests.size() == 1);
    QCOMPARE(anki.requests.front(),
             niratan::anki::can_add_notes_request(kDeck, kNoteType, "Reading", kWord,
                                                  DuplicateScope::collection, false));
}

void DuplicateLookupTest::b10_reads_the_verdict_only() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    FakeAnki anki;
    anki.answers = {
        ok_result(QJsonArray{QJsonObject{{"canAdd", true}, {"noteIDs", QJsonArray{7, 8}}}})};

    const Outcome outcome = run_lookup(duplicate, kWord, base_settings(), anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, DuplicateLookupResult{});
    QCOMPARE(anki.requests.size(), std::size_t{1});
}

void DuplicateLookupTest::b11_can_add_is_not_a_duplicate_for_a_saved_word() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    QVERIFY(write_bytes(niratan::anki::saved_words_path(root), QByteArray(R"(["決定"])")));
    DuplicateLookupService duplicate(root);

    FakeAnki anki;
    anki.answers = {can_add(true)};

    const Outcome outcome = run_lookup(duplicate, kWord, base_settings(), anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, DuplicateLookupResult{});
    QCOMPARE(anki.requests.size(), std::size_t{1});
}

void DuplicateLookupTest::b12_cannot_add_records_the_word_and_reports_the_found_ids() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    FakeAnki anki;
    anki.answers = {can_add(false), ok_result(QJsonArray{105, 101, 105})};

    const Outcome outcome = run_lookup(duplicate, kWord, base_settings(), anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, (DuplicateLookupResult{true, {105, 101}}));
    QCOMPARE(anki.requests.size(), std::size_t{2});
    // The lookup records the word in memory only; add_saved_word persists it.
    QVERIFY(!std::filesystem::exists(root / "anki_words.json"));

    FakeAnki later;
    later.answers = {failed_result()};
    const Outcome saved = run_lookup(duplicate, kWord, base_settings(), later);
    QCOMPARE(saved.calls, 1);
    QCOMPARE(saved.result, (DuplicateLookupResult{true, {}}));
    QCOMPARE(later.requests.size(), std::size_t{1});
}

void DuplicateLookupTest::b12_find_notes_query_follows_duplicate_search_query() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    AnkiSettings settings = base_settings();
    QVERIFY(settings.transport.has_value());
    settings.transport->duplicate_scope = DuplicateScope::deck_root;
    FakeAnki anki;
    anki.answers = {can_add(false), ok_result(QJsonArray{})};

    const std::string expected = niratan::anki::duplicate_search_query(
        kWord, kDeck, kNoteType, kFirstField, DuplicateScope::deck_root, false,
        settings.available_note_types);

    run_lookup(duplicate, kWord, settings, anki);
    QCOMPARE(anki.requests.size(), std::size_t{2});
    QVERIFY(anki.requests.size() == 2);
    QCOMPARE(anki.requests.at(1), niratan::anki::find_notes_request(expected));

    // Swift's nil ankiConnectConfig uses the collection scope for the query.
    AnkiSettings unset = base_settings();
    unset.transport = std::nullopt;
    FakeAnki unset_anki;
    unset_anki.answers = {can_add(false), ok_result(QJsonArray{})};
    const std::string unset_expected = niratan::anki::duplicate_search_query(
        kWord, kDeck, kNoteType, kFirstField, DuplicateScope::collection, false,
        unset.available_note_types);

    run_lookup(duplicate, kWord, unset, unset_anki);
    QCOMPARE(unset_anki.requests.size(), std::size_t{2});
    QVERIFY(unset_anki.requests.size() == 2);
    QCOMPARE(unset_anki.requests.at(1), niratan::anki::find_notes_request(unset_expected));
}

void DuplicateLookupTest::b12_empty_find_notes_result_is_a_duplicate_without_ids() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    FakeAnki anki;
    anki.answers = {can_add(false), ok_result(QJsonArray{})};

    const Outcome outcome = run_lookup(duplicate, kWord, base_settings(), anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, (DuplicateLookupResult{true, {}}));
    QCOMPARE(anki.requests.size(), std::size_t{2});
}

void DuplicateLookupTest::b13_indeterminate_verdicts_answer_the_saved_words() {
    const std::vector<AnkiConnectResult> verdicts = {
        failed_result(),
        ok_result(QJsonObject{{"canAdd", false}}),
        ok_result(QJsonArray{}),
        ok_result(QJsonArray{QJsonValue{1}}),
        ok_result(QJsonArray{QJsonObject{{"canAdd", "false"}}}),
        ok_result(QJsonArray{QJsonObject{{"canAdd", QJsonValue::Null}}}),
    };

    for (const AnkiConnectResult& verdict : verdicts) {
        QTemporaryDir saved_directory;
        QVERIFY(saved_directory.isValid());
        const std::filesystem::path saved_root = temp_root(saved_directory);
        QVERIFY(
            write_bytes(niratan::anki::saved_words_path(saved_root), QByteArray(R"(["決定"])")));
        DuplicateLookupService saved_duplicate(saved_root);
        FakeAnki saved_anki;
        saved_anki.answers = {verdict};

        const Outcome saved = run_lookup(saved_duplicate, kWord, base_settings(), saved_anki);
        QCOMPARE(saved.calls, 1);
        QCOMPARE(saved.result, (DuplicateLookupResult{true, {}}));
        QCOMPARE(saved_anki.requests.size(), std::size_t{1});

        QTemporaryDir empty_directory;
        QVERIFY(empty_directory.isValid());
        const std::filesystem::path empty_root = temp_root(empty_directory);
        DuplicateLookupService empty_duplicate(empty_root);
        FakeAnki empty_anki;
        empty_anki.answers = {verdict};

        const Outcome empty = run_lookup(empty_duplicate, kWord, base_settings(), empty_anki);
        QCOMPARE(empty.calls, 1);
        QCOMPARE(empty.result, DuplicateLookupResult{});
        QCOMPARE(empty_anki.requests.size(), std::size_t{1});
        QCOMPARE(niratan::anki::load_saved_words(empty_root), std::set<std::string>{});
    }
}

void DuplicateLookupTest::b14_failed_find_notes_is_a_duplicate_without_ids() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    FakeAnki anki;
    anki.answers = {can_add(false), failed_result()};

    const Outcome outcome = run_lookup(duplicate, kWord, base_settings(), anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, (DuplicateLookupResult{true, {}}));
    QCOMPARE(anki.requests.size(), std::size_t{2});
}

void DuplicateLookupTest::b14_non_array_find_notes_result_is_a_duplicate_without_ids() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    FakeAnki anki;
    anki.answers = {can_add(false), ok_result(QJsonObject{{"unexpected", true}})};

    const Outcome outcome = run_lookup(duplicate, kWord, base_settings(), anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, (DuplicateLookupResult{true, {}}));
    QCOMPARE(anki.requests.size(), std::size_t{2});
}

void DuplicateLookupTest::b14_failed_find_notes_keeps_the_in_memory_word_when_the_save_failed() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    // A directory in place of the file makes every save fail.
    QVERIFY(std::filesystem::create_directories(root / "anki_words.json"));
    DuplicateLookupService duplicate(root);

    FakeAnki anki;
    anki.answers = {can_add(false), failed_result()};

    const Outcome outcome = run_lookup(duplicate, kWord, base_settings(), anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, (DuplicateLookupResult{true, {}}));
    QCOMPARE(anki.requests.size(), std::size_t{2});

    // A later lookup still knows the word from memory.
    FakeAnki later;
    later.answers = {failed_result()};
    const Outcome saved = run_lookup(duplicate, kWord, base_settings(), later);
    QCOMPARE(saved.calls, 1);
    QCOMPARE(saved.result, (DuplicateLookupResult{true, {}}));
    QVERIFY(std::filesystem::is_directory(root / "anki_words.json"));
}

void DuplicateLookupTest::b15_saved_words_path_is_anki_words_json() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);

    QCOMPARE(niratan::anki::saved_words_path(root).string(), (root / "anki_words.json").string());
}

void DuplicateLookupTest::b15_missing_file_is_an_empty_set() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QCOMPARE(niratan::anki::load_saved_words(temp_root(directory)), std::set<std::string>{});
}

void DuplicateLookupTest::b15_reads_a_json_string_array() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    QVERIFY(
        write_bytes(niratan::anki::saved_words_path(root), QByteArray(R"(["決定","猫","決定"])")));

    QCOMPARE(niratan::anki::load_saved_words(root), (std::set<std::string>{"決定", "猫"}));
}

void DuplicateLookupTest::b15_undecodable_files_are_an_empty_set() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);

    const std::vector<QByteArray> contents = {
        QByteArrayLiteral("not json"),     QByteArrayLiteral("{\"a\":\"b\"}"),
        QByteArrayLiteral("[\"決定\",1]"), QByteArrayLiteral("[null]"),
        QByteArrayLiteral("[[]]"),         QByteArray(),
    };
    for (const QByteArray& bytes : contents) {
        QVERIFY(write_bytes(niratan::anki::saved_words_path(root), bytes));
        QCOMPARE(niratan::anki::load_saved_words(root), std::set<std::string>{});
    }
}

void DuplicateLookupTest::b15_a_damaged_file_is_replaced_by_the_next_save() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    QVERIFY(write_bytes(niratan::anki::saved_words_path(root), QByteArrayLiteral("not json")));
    DuplicateLookupService duplicate(root);
    QCOMPARE(niratan::anki::load_saved_words(root), std::set<std::string>{});

    QVERIFY(duplicate.add_saved_word("決定"));
    QCOMPARE(niratan::anki::load_saved_words(root), (std::set<std::string>{"決定"}));

    const QJsonDocument document =
        QJsonDocument::fromJson(read_bytes(niratan::anki::saved_words_path(root)));
    QVERIFY(document.isArray());
    QCOMPARE(document.array(), QJsonArray{QString::fromUtf8("決定")});
}

void DuplicateLookupTest::b15_add_inserts_and_saves_the_set() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    QVERIFY(duplicate.add_saved_word("決定"));
    QCOMPARE(niratan::anki::load_saved_words(root), (std::set<std::string>{"決定"}));

    const QJsonDocument document =
        QJsonDocument::fromJson(read_bytes(niratan::anki::saved_words_path(root)));
    QVERIFY(document.isArray());
    QCOMPARE(document.array(), QJsonArray{QString::fromUtf8("決定")});
}

void DuplicateLookupTest::b15_add_keeps_existing_words_and_deduplicates() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    QVERIFY(write_bytes(niratan::anki::saved_words_path(root), QByteArray(R"(["猫"])")));
    DuplicateLookupService duplicate(root);

    QVERIFY(duplicate.add_saved_word("決定"));
    QVERIFY(duplicate.add_saved_word("決定"));
    QCOMPARE(niratan::anki::load_saved_words(root), (std::set<std::string>{"決定", "猫"}));

    const QJsonDocument document =
        QJsonDocument::fromJson(read_bytes(niratan::anki::saved_words_path(root)));
    QVERIFY(document.isArray());
    QCOMPARE(document.array().size(), 2);
}

void DuplicateLookupTest::b15_an_empty_word_is_stored() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    DuplicateLookupService duplicate(root);

    QVERIFY(duplicate.add_saved_word(""));
    QCOMPARE(niratan::anki::load_saved_words(root), (std::set<std::string>{""}));
}

void DuplicateLookupTest::b15_add_reports_a_failed_save() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = temp_root(directory);
    // A directory in place of the file makes every save fail.
    QVERIFY(std::filesystem::create_directories(root / "anki_words.json"));
    DuplicateLookupService duplicate(root);

    QCOMPARE(duplicate.add_saved_word("決定"), false);
    QCOMPARE(niratan::anki::load_saved_words(root), std::set<std::string>{});

    // The failed save still leaves the word in memory for later lookups.
    FakeAnki anki;
    anki.answers = {failed_result()};
    const Outcome outcome = run_lookup(duplicate, kWord, base_settings(), anki);
    QCOMPARE(outcome.calls, 1);
    QCOMPARE(outcome.result, (DuplicateLookupResult{true, {}}));
}

QTEST_APPLESS_MAIN(DuplicateLookupTest)
#include "tst_duplicate_lookup.moc"
