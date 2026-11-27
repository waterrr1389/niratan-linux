// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/duplicate_query.h"

#include <QTest>

using niratan::anki::DuplicateScope;
using niratan::anki::NoteType;
using niratan::anki::duplicate_search_query;

namespace {

constexpr std::string_view kDeck = "日本語::語彙::N1";

std::string query(std::string_view word, DuplicateScope scope = DuplicateScope::collection,
                  bool check_all_models = false, const std::vector<NoteType>& note_types = {}) {
    return duplicate_search_query(word, kDeck, "Lapis", "Expression", scope, check_all_models,
                                  note_types);
}

}  // namespace

// Core/AnkiManager.swift (duplicateSearchQuery, quotedAnkiSearchTerm) has no
// test script of its own, so the cases follow its rules directly.
class DuplicateQueryTest : public QObject {
    Q_OBJECT

   private slots:
    void collection_scope_has_no_deck_term();
    void deck_scope_names_the_deck();
    void deck_root_scope_names_the_top_level_deck();
    void deck_root_scope_skips_leading_separators();
    void quotes_are_removed_from_every_term();
    void only_the_field_name_is_lowercased();
    void other_characters_pass_through();
    void check_all_models_drops_the_note_type_term();
    void check_all_models_alternates_distinct_first_fields();
    void one_distinct_first_field_needs_no_parentheses();
    void check_all_models_ignores_the_given_first_field();
    void check_all_models_without_fields_has_no_field_term();
};

void DuplicateQueryTest::collection_scope_has_no_deck_term() {
    QCOMPARE(query("決定"), std::string(R"q("note:Lapis" "expression:決定")q"));
}

void DuplicateQueryTest::deck_scope_names_the_deck() {
    QCOMPARE(query("決定", DuplicateScope::deck),
             std::string(R"q("deck:日本語::語彙::N1" "note:Lapis" "expression:決定")q"));
}

void DuplicateQueryTest::deck_root_scope_names_the_top_level_deck() {
    QCOMPARE(query("決定", DuplicateScope::deck_root),
             std::string(R"q("deck:日本語" "note:Lapis" "expression:決定")q"));
}

void DuplicateQueryTest::deck_root_scope_skips_leading_separators() {
    const auto result = duplicate_search_query("x", "::A::B", "Lapis", "Expression",
                                               DuplicateScope::deck_root, false, {});
    QCOMPARE(result, std::string(R"q("deck:A" "note:Lapis" "expression:x")q"));
}

void DuplicateQueryTest::quotes_are_removed_from_every_term() {
    const auto result = duplicate_search_query(R"q(a"b)q", R"q(De"ck)q", R"q(No"te)q",
                                               R"q(Fi"eld)q", DuplicateScope::deck, false, {});
    QCOMPARE(result, std::string(R"q("deck:Deck" "note:Note" "field:ab")q"));
}

void DuplicateQueryTest::only_the_field_name_is_lowercased() {
    const auto result = duplicate_search_query("Decide", "D", "N", "ÉCRIT",
                                               DuplicateScope::collection, false, {});
    QCOMPARE(result, std::string(R"q("note:N" "écrit:Decide")q"));
}

void DuplicateQueryTest::other_characters_pass_through() {
    QCOMPARE(query(" 決定 *_:"), std::string(R"q("note:Lapis" "expression: 決定 *_:")q"));
}

void DuplicateQueryTest::check_all_models_drops_the_note_type_term() {
    const std::vector<NoteType> note_types = {{"Lapis", {"Expression"}}};
    QCOMPARE(query("決定", DuplicateScope::deck, true, note_types),
             std::string(R"q("deck:日本語::語彙::N1" "expression:決定")q"));
}

void DuplicateQueryTest::check_all_models_alternates_distinct_first_fields() {
    const std::vector<NoteType> note_types = {{"Lapis", {"Expression", "Sentence"}},
                                              {"Kiku", {"expression"}},
                                              {"Senren", {"word"}},
                                              {"Empty", {}}};
    QCOMPARE(query("決定", DuplicateScope::collection, true, note_types),
             std::string(R"q(("expression:決定" or "word:決定"))q"));
}

void DuplicateQueryTest::one_distinct_first_field_needs_no_parentheses() {
    const std::vector<NoteType> same_ascii = {{"Lapis", {"Expression"}}, {"Kiku", {"expression"}}};
    QCOMPARE(query("決定", DuplicateScope::collection, true, same_ascii),
             std::string(R"q("expression:決定")q"));

    const std::vector<NoteType> same_unicode = {{"A", {"ÉCRIT"}}, {"B", {"écrit"}}};
    QCOMPARE(query("決定", DuplicateScope::collection, true, same_unicode),
             std::string(R"q("écrit:決定")q"));
}

void DuplicateQueryTest::check_all_models_ignores_the_given_first_field() {
    const std::vector<NoteType> note_types = {{"Senren", {"word"}}};
    QCOMPARE(query("決定", DuplicateScope::collection, true, note_types),
             std::string(R"q("word:決定")q"));
}

void DuplicateQueryTest::check_all_models_without_fields_has_no_field_term() {
    const std::vector<NoteType> note_types = {{"Empty", {}}};
    QCOMPARE(query("決定", DuplicateScope::collection, true, note_types), std::string());
    QCOMPARE(query("決定", DuplicateScope::deck, true, note_types),
             std::string(R"q("deck:日本語::語彙::N1")q"));
}

QTEST_APPLESS_MAIN(DuplicateQueryTest)
#include "tst_duplicate_query.moc"
