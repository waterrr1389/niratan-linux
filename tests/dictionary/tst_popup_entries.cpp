// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/popup_entries.h"

#include <QJsonDocument>
#include <QtTest>

#include <string>
#include <vector>

using niratan::dictionary::FrequencyGroup;
using niratan::dictionary::FrequencyValue;
using niratan::dictionary::GlossaryEntry;
using niratan::dictionary::LookupEntry;
using niratan::dictionary::PitchGroup;
using niratan::dictionary::TraceStep;
using niratan::dictionary::lookup_entries_json;

// Features/Popup/PopupView.swift (buildLookupEntries) and popup.js define the
// entry shape; there is no Swift test script, so the cases follow that
// builder directly.
namespace {

QJsonArray expected(const char* json) {
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(json));
    return document.array();
}

TraceStep step(const char* name, const char* description) {
    return TraceStep{name, description};
}

}  // namespace

class PopupEntriesTest : public QObject {
    Q_OBJECT

   private slots:
    void a_full_entry_has_every_popup_field();
    void an_entry_without_details_has_empty_arrays();
    void traces_are_reversed_and_the_first_is_repeated();
    void pitches_keep_the_first_of_each_duplicate();
    void rules_split_on_single_spaces();
    void entries_keep_their_order();
    void strings_are_escaped_as_json();
};

void PopupEntriesTest::a_full_entry_has_every_popup_field() {
    LookupEntry entry;
    entry.expression = "decide";
    entry.reading = "decide";
    entry.matched = "decided";
    entry.rules = "v vt";
    entry.traces = {{step("past", "Simple past"), step("polite", "Polite")}};
    entry.glossaries = {{"Probe", "[\"to make up one's mind\"]", "n", "common"}};
    entry.frequencies = {{"freq", {{1200, "1200"}, {-1, ""}}}};
    entry.pitches = {{"pitch", {0, 1}, {"a", "b"}}};

    const QJsonArray actual = lookup_entries_json({entry});
    const QJsonArray want = expected(R"([
                 {
                   "expression": "decide",
                   "reading": "decide",
                   "matched": "decided",
                   "deinflectionTrace": [
                     {"name": "polite", "description": "Polite"},
                     {"name": "past", "description": "Simple past"}
                   ],
                   "deinflectionTraces": [[
                     {"name": "polite", "description": "Polite"},
                     {"name": "past", "description": "Simple past"}
                   ]],
                   "glossaries": [{
                     "dictionary": "Probe",
                     "content": "[\"to make up one's mind\"]",
                     "definitionTags": "n",
                     "termTags": "common"
                   }],
                   "frequencies": [{
                     "dictionary": "freq",
                     "frequencies": [
                       {"value": 1200, "displayValue": "1200"},
                       {"value": -1, "displayValue": ""}
                     ]
                   }],
                   "pitches": [{
                     "dictionary": "pitch",
                     "pitchPositions": [0, 1],
                     "transcriptions": ["a", "b"]
                   }],
                   "rules": ["v", "vt"]
                 }
               ])");
    QCOMPARE(actual, want);
}

void PopupEntriesTest::an_entry_without_details_has_empty_arrays() {
    LookupEntry entry;
    entry.expression = "x";
    entry.reading = "x";
    entry.matched = "x";

    const QJsonArray actual = lookup_entries_json({entry});
    const QJsonArray want = expected(R"([{
                 "expression": "x",
                 "reading": "x",
                 "matched": "x",
                 "deinflectionTrace": [],
                 "deinflectionTraces": [],
                 "glossaries": [],
                 "frequencies": [],
                 "pitches": [],
                 "rules": []
               }])");
    QCOMPARE(actual, want);
}

void PopupEntriesTest::traces_are_reversed_and_the_first_is_repeated() {
    LookupEntry entry;
    entry.traces = {{step("a", "1"), step("b", "2"), step("c", "3")},
                    {step("d", "4")}};

    const QJsonArray result = lookup_entries_json({entry});
    const QJsonArray first = result.first().toObject().value("deinflectionTrace").toArray();
    const QJsonArray all = result.first().toObject().value("deinflectionTraces").toArray();

    QCOMPARE(first.at(0).toObject().value("name").toString(), QString("c"));
    QCOMPARE(first.at(2).toObject().value("name").toString(), QString("a"));
    QCOMPARE(all.size(), 2);
    QCOMPARE(all.at(1).toArray().at(0).toObject().value("name").toString(), QString("d"));
}

void PopupEntriesTest::pitches_keep_the_first_of_each_duplicate() {
    LookupEntry entry;
    entry.pitches = {{"p", {0, 1, 1, 2, 0}, {"x", "y", "x", "y"}}};

    const QJsonObject pitch =
        lookup_entries_json({entry}).first().toObject().value("pitches").toArray().first().toObject();
    const QJsonArray positions = pitch.value("pitchPositions").toArray();
    const QJsonArray transcriptions = pitch.value("transcriptions").toArray();

    QCOMPARE(positions.size(), 3);
    QCOMPARE(positions.at(0).toInt(), 0);
    QCOMPARE(positions.at(1).toInt(), 1);
    QCOMPARE(positions.at(2).toInt(), 2);
    QCOMPARE(transcriptions.size(), 2);
    QCOMPARE(transcriptions.at(0).toString(), QString("x"));
    QCOMPARE(transcriptions.at(1).toString(), QString("y"));
}

void PopupEntriesTest::rules_split_on_single_spaces() {
    LookupEntry entry;
    entry.rules = "  a  b ";

    const QJsonArray rules =
        lookup_entries_json({entry}).first().toObject().value("rules").toArray();

    QCOMPARE(rules.size(), 2);
    QCOMPARE(rules.at(0).toString(), QString("a"));
    QCOMPARE(rules.at(1).toString(), QString("b"));
}

void PopupEntriesTest::entries_keep_their_order() {
    LookupEntry first;
    first.expression = "one";
    LookupEntry second;
    second.expression = "two";

    const QJsonArray result = lookup_entries_json({first, second});

    QCOMPARE(result.size(), 2);
    QCOMPARE(result.at(0).toObject().value("expression").toString(), QString("one"));
    QCOMPARE(result.at(1).toObject().value("expression").toString(), QString("two"));
}

void PopupEntriesTest::strings_are_escaped_as_json() {
    LookupEntry entry;
    entry.expression = "say \"hi\"\\\n";
    entry.matched = "化";

    const QJsonArray result = lookup_entries_json({entry});
    const QJsonObject object = result.first().toObject();

    QCOMPARE(object.value("expression").toString(), QString("say \"hi\"\\\n"));
    QCOMPARE(object.value("matched").toString(), QString::fromUtf8("化"));
}

QTEST_APPLESS_MAIN(PopupEntriesTest)
#include "tst_popup_entries.moc"
