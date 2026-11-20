// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/text_selection.h"

#include <QtTest>

using niratan::dictionary::highlight_range;
using niratan::dictionary::lookup_candidate;

// Cases follow Core/SelectionLookup/TextSelectionResolver.swift:
// lookupCandidate, highlightRange, englishWordStart, isEnglishHitBoundary.
class TextSelectionTest : public QObject {
    Q_OBJECT

   private slots:
    void rejects_out_of_range_input();
    void japanese_starts_at_the_click();
    void japanese_trims_and_shifts_the_start();
    void english_walks_back_to_the_word_start();
    void english_keeps_internal_delimiters();
    void english_breaks_on_shared_delimiters();
    void highlight_uses_the_shorter_length();
};

void TextSelectionTest::rejects_out_of_range_input() {
    QVERIFY(!lookup_candidate("abc", -1, 4, "en"));
    QVERIFY(!lookup_candidate("abc", 3, 4, "en"));
    QVERIFY(!lookup_candidate("abc", 0, 0, "ja"));
    QVERIFY(!lookup_candidate("   ", 1, 4, "en"));
}

void TextSelectionTest::japanese_starts_at_the_click() {
    const auto candidate = lookup_candidate("日本語です", 1, 3, "ja");
    QVERIFY(candidate);
    QCOMPARE(candidate->text, std::string("本語で"));
    QCOMPARE(candidate->utf16_start, 1);
}

void TextSelectionTest::japanese_trims_and_shifts_the_start() {
    const auto candidate = lookup_candidate("  本語で", 0, 6, "ja");
    QVERIFY(candidate);
    QCOMPARE(candidate->text, std::string("本語で"));
    QCOMPARE(candidate->utf16_start, 2);
}

void TextSelectionTest::english_walks_back_to_the_word_start() {
    const auto candidate = lookup_candidate("say decided now", 6, 8, "en");
    QVERIFY(candidate);
    QCOMPARE(candidate->text, std::string("decided"));
    QCOMPARE(candidate->utf16_start, 4);
}

void TextSelectionTest::english_keeps_internal_delimiters() {
    const auto hyphen = lookup_candidate("a well-known case", 9, 12, "en");
    QVERIFY(hyphen);
    QCOMPARE(hyphen->text, std::string("well-known"));

    const auto apostrophe = lookup_candidate("it's fine", 3, 6, "en");
    QVERIFY(apostrophe);
    QCOMPARE(apostrophe->text, std::string("it's"));

    // A trailing hyphen is a boundary, not part of the word.
    const auto trailing = lookup_candidate("go- home", 1, 6, "en");
    QVERIFY(trailing);
    QCOMPARE(trailing->text, std::string("go"));
}

void TextSelectionTest::english_breaks_on_shared_delimiters() {
    const auto candidate = lookup_candidate("yes。no", 4, 4, "en");
    QVERIFY(candidate);
    QCOMPARE(candidate->text, std::string("no"));
    QCOMPARE(candidate->utf16_start, 4);
}

void TextSelectionTest::highlight_uses_the_shorter_length() {
    const auto candidate = lookup_candidate("decided", 3, 8, "en");
    QVERIFY(candidate);

    const auto shorter = highlight_range(*candidate, "decide");
    QVERIFY(shorter);
    QCOMPARE(shorter->utf16_start, 0);
    QCOMPARE(shorter->utf16_length, 6);

    QVERIFY(!highlight_range(*candidate, ""));
}

QTEST_APPLESS_MAIN(TextSelectionTest)
#include "tst_text_selection.moc"
