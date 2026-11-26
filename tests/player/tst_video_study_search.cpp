// SPDX-License-Identifier: GPL-3.0-or-later
//
// V5 B1 and B4 of notes/specs/video-v5-transcript-search.md:
// VideoStudyListCard.swift `VideoStudySearch.normalizedQuery`, `matches`,
// `highlighted` and `options`, with the module Decision to share one NFKD,
// case-folded, mark-stripped normalization for all three insensitive options.

#include "services/player/video_study_search.h"

#include <QtTest>

using niratan::player::normalized_study_query;
using niratan::player::study_search_matches;
using niratan::player::study_search_ranges;
using niratan::player::StudySearchRange;

class VideoStudySearchTest : public QObject {
    Q_OBJECT

   private slots:
    void queryTrimsWhitespaceAndNewlines();
    void matchesIgnoreCaseDiacriticsAndWidth();
    void inactiveQueryMatchesNothing();
    void rangesCoverEveryNonOverlappingMatch();
    void rangesAddressTheOriginalCharacters();
};

// B1: normalizedQuery trims whitespace and newlines.
void VideoStudySearchTest::queryTrimsWhitespaceAndNewlines() {
    QCOMPARE(normalized_study_query(QStringLiteral("  needle\n")), QStringLiteral("needle"));
    QCOMPARE(normalized_study_query(QStringLiteral("\t\u3000needle \r\n")),
             QStringLiteral("needle"));
    QCOMPARE(normalized_study_query(QStringLiteral(" \n\t ")), QString());
    QCOMPARE(normalized_study_query(QStringLiteral("needle and thread")),
             QStringLiteral("needle and thread"));
}

// B1: case, diacritics and widths do not decide a match in either direction.
void VideoStudySearchTest::matchesIgnoreCaseDiacriticsAndWidth() {
    QVERIFY(study_search_matches(QStringLiteral("Café"), QStringLiteral("cafe")));
    QVERIFY(study_search_matches(QStringLiteral("cafe"), QStringLiteral("Café")));
    QVERIFY(study_search_matches(QStringLiteral("ｃａｆｅ"), QStringLiteral("cafe")));
    QVERIFY(study_search_matches(QStringLiteral("CAFÉ"), QStringLiteral("café")));
    QVERIFY(study_search_matches(QStringLiteral("日本語の字幕"), QStringLiteral("日本語")));
    // Full case folding expands the sharp s, as Swift's case-insensitive
    // comparison does.
    QVERIFY(study_search_matches(QStringLiteral("Straße"), QStringLiteral("strasse")));
    QVERIFY(study_search_matches(QStringLiteral("ß"), QStringLiteral("s")));
    QVERIFY(!study_search_matches(QStringLiteral("Café"), QStringLiteral("tea")));
    QVERIFY(!study_search_matches(QString(), QStringLiteral("tea")));
}

// B1: an empty or unmatchable query matches nothing, whatever the text.
void VideoStudySearchTest::inactiveQueryMatchesNothing() {
    QVERIFY(!study_search_matches(QStringLiteral("text"), QString()));
    QVERIFY(!study_search_matches(QStringLiteral("text"), QStringLiteral("  \n")));
    QVERIFY(study_search_ranges(QStringLiteral("text"), QStringLiteral(" ")).isEmpty());
}

// B4: every non-overlapping occurrence, left to right.
void VideoStudySearchTest::rangesCoverEveryNonOverlappingMatch() {
    const QList<StudySearchRange> twice =
        study_search_ranges(QStringLiteral("one two one"), QStringLiteral("one"));
    QCOMPARE(twice.size(), 2);
    QCOMPARE(twice.at(0), (StudySearchRange{0, 3}));
    QCOMPARE(twice.at(1), (StudySearchRange{8, 3}));

    // The next search starts at the previous match's end.
    const QList<StudySearchRange> pairs =
        study_search_ranges(QStringLiteral("aaaa"), QStringLiteral("aa"));
    QCOMPARE(pairs.size(), 2);
    QCOMPARE(pairs.at(0), (StudySearchRange{0, 2}));
    QCOMPARE(pairs.at(1), (StudySearchRange{2, 2}));
    QCOMPARE(study_search_ranges(QStringLiteral("aaa"), QStringLiteral("aa")).size(), 1);

    // Case, width and diacritics do not move the range off the original text.
    const QList<StudySearchRange> folded =
        study_search_ranges(QStringLiteral("xx ｃａｆｅ yy"), QStringLiteral("CAFE"));
    QCOMPARE(folded.size(), 1);
    QCOMPARE(folded.at(0), (StudySearchRange{3, 4}));
}

// B4: a match inside a decomposed character covers that whole original
// character, and characters and order are unchanged.
void VideoStudySearchTest::rangesAddressTheOriginalCharacters() {
    const QList<StudySearchRange> composed =
        study_search_ranges(QStringLiteral("xx café yy"), QStringLiteral("cafe"));
    QCOMPARE(composed.size(), 1);
    QCOMPARE(composed.at(0), (StudySearchRange{3, 4}));

    const QList<StudySearchRange> japanese =
        study_search_ranges(QStringLiteral("日本語"), QStringLiteral("本"));
    QCOMPARE(japanese.size(), 1);
    QCOMPARE(japanese.at(0), (StudySearchRange{1, 1}));

    const QList<StudySearchRange> leaves =
        study_search_ranges(QStringLiteral("Café au lait"), QStringLiteral("CAFÉ"));
    QCOMPARE(leaves.size(), 1);
    QCOMPARE(leaves.at(0), (StudySearchRange{0, 4}));

    const QList<StudySearchRange> sharp_word =
        study_search_ranges(QStringLiteral("Straße"), QStringLiteral("STRASSE"));
    QCOMPARE(sharp_word.size(), 1);
    QCOMPARE(sharp_word.at(0), (StudySearchRange{0, 6}));

    // An NFKD expansion that repeats the query advances past the whole
    // character, so it highlights once.
    const QList<StudySearchRange> sharp_s =
        study_search_ranges(QStringLiteral("ß"), QStringLiteral("s"));
    QCOMPARE(sharp_s.size(), 1);
    QCOMPARE(sharp_s.at(0), (StudySearchRange{0, 1}));
    const QList<StudySearchRange> ligature =
        study_search_ranges(QStringLiteral("ﬄ"), QStringLiteral("f"));
    QCOMPARE(ligature.size(), 1);
    QCOMPARE(ligature.at(0), (StudySearchRange{0, 1}));
}

QTEST_APPLESS_MAIN(VideoStudySearchTest)

#include "tst_video_study_search.moc"
