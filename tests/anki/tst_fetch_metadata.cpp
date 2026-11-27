// SPDX-License-Identifier: GPL-3.0-or-later
//
// Core/AnkiManager.swift (applyFetchedAnkiMetadata, pruneFieldMappings,
// autofillFieldMappings) and Models/Anki.swift (AnkiFieldTemplate.autofilledMappings).

#include "services/anki/fetch_metadata.h"

#include <QTest>

using niratan::anki::apply_fetched_metadata;
using niratan::anki::FetchedMetadata;

class FetchMetadataTest : public QObject {
    Q_OBJECT

   private slots:
    void rejects_missing_decks_or_usable_models();
    void preserves_valid_selections_and_repairs_mappings();
    void falls_back_to_first_non_default_deck_and_first_usable_model();
    void falls_back_to_first_deck_even_when_all_are_default();
};

// VM8: AnkiManager.applyFetchedAnkiMetadata.
void FetchMetadataTest::rejects_missing_decks_or_usable_models() {
    QVERIFY(apply_fetched_metadata({"Deck"}, {{"Model", {"Field"}}}, {}, {}, {}).has_value());
    QVERIFY(!apply_fetched_metadata({}, {{"Model", {"Field"}}}, {}, {}, {}));
    QVERIFY(!apply_fetched_metadata({"Deck"}, {}, {}, {}, {}));
    QVERIFY(!apply_fetched_metadata({"Deck"}, {{"Model", {}}}, {}, {}, {}));
}

// VM8–VM10: AnkiManager.applyFetchedAnkiMetadata, pruneFieldMappings,
// autofillFieldMappings and AnkiFieldTemplate.autofilledMappings.
void FetchMetadataTest::preserves_valid_selections_and_repairs_mappings() {
    const auto result = apply_fetched_metadata(
        {"First", "Default", "Last"},
        {{"Lapis", {}},
         {"Kiku", {"Other"}},
         {"Lapis", {"Sentence", "Expression", "ExpressionReading", "Picture", "Unsupported"}}},
        "Default", "Lapis",
        {{"Expression", " {expression} "},
         {"ExpressionReading", "　\t"},
         {"Sentence", ""},
         {"Removed", "stale"},
         {"Unsupported", "custom"}});
    QVERIFY(result);
    const FetchedMetadata expected{
        {"First", "Default", "Last"},
        {{"Kiku", {"Other"}},
         {"Lapis", {"Sentence", "Expression", "ExpressionReading", "Picture", "Unsupported"}}},
        "Default",
        "Lapis",
        {{"Expression", " {expression} "},
         {"ExpressionReading", "{reading}"},
         {"Sentence", ""},
         {"Picture", "{book-cover}"},
         {"Unsupported", "custom"}}};
    QCOMPARE(*result, expected);
}

// VM9–VM10: AnkiManager.applyFetchedAnkiMetadata, including explicit pruning
// when the first usable model is not in AnkiFieldTemplate.templates.
void FetchMetadataTest::falls_back_to_first_non_default_deck_and_first_usable_model() {
    const auto result = apply_fetched_metadata(
        {"dEfAuLt", "KeepCase", "Other"}, {{"Lapis", {}}, {"Custom", {"Shared"}}, {"Kiku", {"A"}}},
        "other", "Lapis", {{"Shared", "custom"}, {"Orphan", "stale"}});
    QVERIFY(result);
    const FetchedMetadata expected{{"dEfAuLt", "KeepCase", "Other"},
                                   {{"Custom", {"Shared"}}, {"Kiku", {"A"}}},
                                   "KeepCase",
                                   "Custom",
                                   {{"Shared", "custom"}}};
    QCOMPARE(*result, expected);
}

// VM9: AnkiManager.applyFetchedAnkiMetadata's last deck fallback is decks.first.
void FetchMetadataTest::falls_back_to_first_deck_even_when_all_are_default() {
    const auto result =
        apply_fetched_metadata({"DEFAULT", "Default"}, {{"Custom", {"Shared"}}}, {}, {}, {});
    QVERIFY(result);
    QCOMPARE(result->selected_deck, std::string("DEFAULT"));
}

QTEST_APPLESS_MAIN(FetchMetadataTest)
#include "tst_fetch_metadata.moc"
