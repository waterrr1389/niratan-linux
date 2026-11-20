// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/field_mapping.h"

#include <QtTest>

using niratan::anki::FieldMappings;
using niratan::anki::apply_default_mappings;
using niratan::anki::autofill_mappings;
using niratan::anki::clears_mapping;
using niratan::anki::has_field_defaults;

// Cases follow script/test_anki_field_templates.swift, which specifies
// Models/Anki.swift.
class FieldMappingTest : public QObject {
    Q_OBJECT

   private slots:
    void lapis_fills_its_known_fields();
    void an_existing_mapping_wins_unless_it_is_blank();
    void an_empty_mapping_stays_off();
    void unicode_whitespace_counts_as_blank();
    void shared_fields_apply_to_every_known_note_type();
    void an_unknown_note_type_keeps_its_mappings();
    void applying_defaults_overwrites_and_drops_removed_fields();
    void lapis_clears_definition_picture();
};

void FieldMappingTest::lapis_fills_its_known_fields() {
    const auto fields = std::vector<std::string>{"Expression", "MainDefinition", "Sentence",
                                                  "SentenceAudio", "Picture", "UnknownField"};
    const auto mapped = autofill_mappings("Lapis", fields, {});
    QCOMPARE(mapped.at("Expression"), std::string("{expression}"));
    QCOMPARE(mapped.at("MainDefinition"), std::string("{glossary-first}"));
    QCOMPARE(mapped.at("Sentence"), std::string("{sentence}"));
    QCOMPARE(mapped.at("SentenceAudio"), std::string("{sasayaki-audio}"));
    QCOMPARE(mapped.at("Picture"), std::string("{book-cover}"));
    QVERIFY(!mapped.contains("UnknownField"));
}

void FieldMappingTest::an_existing_mapping_wins_unless_it_is_blank() {
    const auto fields = std::vector<std::string>{"Expression", "MainDefinition"};
    const auto mapped = autofill_mappings(
        "Lapis", fields,
        {{"Expression", "{custom-expression}"}, {"MainDefinition", "   "}, {"RemovedField", "{sentence}"}});
    QCOMPARE(mapped.at("Expression"), std::string("{custom-expression}"));
    QCOMPARE(mapped.at("MainDefinition"), std::string("{glossary-first}"));
    QVERIFY(!mapped.contains("RemovedField"));
}

void FieldMappingTest::an_empty_mapping_stays_off() {
    const auto fields = std::vector<std::string>{"Expression", "SentenceAudio", "Picture"};
    const auto mapped = autofill_mappings(
        "Lapis", fields, {{"Expression", "{expression}"}, {"SentenceAudio", ""}, {"Picture", ""}});
    QCOMPARE(mapped.at("SentenceAudio"), std::string(""));
    QCOMPARE(mapped.at("Picture"), std::string(""));
}

// whitespacesAndNewlines covers ideographic and no-break spaces, which
// Japanese input produces.
void FieldMappingTest::unicode_whitespace_counts_as_blank() {
    const auto mapped = autofill_mappings(
        "Lapis", {"Expression", "Sentence", "Glossary"},
        {{"Expression", "\u3000"}, {"Sentence", "\u00A0\n"}, {"Glossary", "\u3000x"}});
    QCOMPARE(mapped.at("Expression"), std::string("{expression}"));
    QCOMPARE(mapped.at("Sentence"), std::string("{sentence}"));
    QCOMPARE(mapped.at("Glossary"), std::string("\u3000x"));
}

void FieldMappingTest::shared_fields_apply_to_every_known_note_type() {
    const auto kiku = autofill_mappings(
        "Kiku", {"ExpressionAudio", "SentenceAudio", "Picture"}, {});
    QCOMPARE(kiku.at("ExpressionAudio"), std::string("{audio}"));
    QCOMPARE(kiku.at("SentenceAudio"), std::string("{sasayaki-audio}"));
    QCOMPARE(kiku.at("Picture"), std::string("{book-cover}"));

    const auto senren =
        autofill_mappings("Senren", {"word", "definition", "wordAudio", "sentenceAudio", "picture"}, {});
    QCOMPARE(senren.at("word"), std::string("{expression}"));
    QCOMPARE(senren.at("definition"), std::string("{glossary-first}"));
    QCOMPARE(senren.at("wordAudio"), std::string("{audio}"));
    QCOMPARE(senren.at("sentenceAudio"), std::string("{sasayaki-audio}"));
    QCOMPARE(senren.at("picture"), std::string("{book-cover}"));
}

void FieldMappingTest::an_unknown_note_type_keeps_its_mappings() {
    const FieldMappings existing = {{"Front", "{expression}"}};
    QCOMPARE(autofill_mappings("Custom", {"Front"}, existing), existing);
    QCOMPARE(apply_default_mappings("Custom", {"Front"}, existing), existing);
    QVERIFY(!has_field_defaults("Custom"));
    QVERIFY(has_field_defaults("Lapis"));
}

void FieldMappingTest::applying_defaults_overwrites_and_drops_removed_fields() {
    const auto fields = std::vector<std::string>{"Expression", "Sentence", "SentenceAudio", "Picture",
                                                  "ExtraField"};
    const auto mapped = apply_default_mappings(
        "Lapis", fields,
        {{"Expression", "{custom-expression}"},
         {"Sentence", "{video-subtitle}"},
         {"Picture", "{video-screenshot}"},
         {"ExtraField", "{custom-extra}"},
         {"RemovedField", "{removed}"}});
    QCOMPARE(mapped.at("Expression"), std::string("{expression}"));
    QCOMPARE(mapped.at("Sentence"), std::string("{sentence}"));
    QCOMPARE(mapped.at("SentenceAudio"), std::string("{sasayaki-audio}"));
    QCOMPARE(mapped.at("Picture"), std::string("{book-cover}"));
    QCOMPARE(mapped.at("ExtraField"), std::string("{custom-extra}"));
    QVERIFY(!mapped.contains("RemovedField"));
}

void FieldMappingTest::lapis_clears_definition_picture() {
    const auto mapped = apply_default_mappings(
        "Lapis", {"Expression", "DefinitionPicture", "CustomField"},
        {{"DefinitionPicture", "{glossary}"}, {"CustomField", "{custom-extra}"}});
    QCOMPARE(mapped.at("Expression"), std::string("{expression}"));
    QVERIFY(!mapped.contains("DefinitionPicture"));
    QCOMPARE(mapped.at("CustomField"), std::string("{custom-extra}"));

    QVERIFY(clears_mapping("Lapis", "DefinitionPicture"));
    QVERIFY(!clears_mapping("Lapis", "CustomField"));
    QVERIFY(!clears_mapping("Kiku", "DefinitionPicture"));
}

QTEST_APPLESS_MAIN(FieldMappingTest)
#include "tst_field_mapping.moc"
