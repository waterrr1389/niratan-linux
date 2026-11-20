// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/dictionary/frequency_rank.h"

#include <QtTest>

using niratan::dictionary::FrequencyReading;
using niratan::dictionary::FrequencySample;
using niratan::dictionary::FrequencySortOrder;
using niratan::dictionary::RankedLookup;
using niratan::dictionary::TraceCandidate;
using niratan::dictionary::rank_by_frequency;

// Cases follow LookupEngine.sortedByFrequencyDictionary, traceSortKey and
// frequencyValue.
class FrequencyRankTest : public QObject {
    Q_OBJECT

   private slots:
    void leaves_order_untouched_without_a_dictionary();
    void longer_match_wins_over_frequency();
    void shorter_trace_wins_over_frequency();
    void best_candidate_decides_the_trace();
    void no_trace_sorts_last();
    void ascending_keeps_the_smallest_value();
    void descending_keeps_the_largest_value();
    void a_value_ranks_ahead_of_none();
    void equal_values_keep_input_order();
    void ignores_other_dictionaries_and_negative_values();
};

RankedLookup result(std::string matched, std::string expression, int value) {
    RankedLookup lookup;
    lookup.matched = std::move(matched);
    lookup.expression = std::move(expression);
    lookup.frequencies.push_back(FrequencyReading{"freq", {FrequencySample{value}}});
    return lookup;
}

void FrequencyRankTest::leaves_order_untouched_without_a_dictionary() {
    const std::vector<RankedLookup> input = {result("ab", "ab", 1), result("a", "a", 9)};
    QCOMPARE(rank_by_frequency(input, FrequencySortOrder::automatic, "freq"), input);
    QCOMPARE(rank_by_frequency(input, FrequencySortOrder::ascending, ""), input);
    QCOMPARE(rank_by_frequency({input.front()}, FrequencySortOrder::ascending, "freq"),
             std::vector<RankedLookup>{input.front()});
}

void FrequencyRankTest::longer_match_wins_over_frequency() {
    const auto ranked =
        rank_by_frequency({result("a", "a", 1), result("abc", "abc", 100)},
                          FrequencySortOrder::ascending, "freq");
    QVERIFY(!ranked.empty());
    QCOMPARE(ranked.front().matched, std::string("abc"));
}

void FrequencyRankTest::shorter_trace_wins_over_frequency() {
    auto plain = result("ab", "ab", 50);
    plain.traces.push_back(TraceCandidate{0, 0, "ab"});
    auto inflected = result("ab", "ab", 1);
    inflected.traces.push_back(TraceCandidate{0, 2, "ab"});
    const auto ranked =
        rank_by_frequency({inflected, plain}, FrequencySortOrder::ascending, "freq");
    QVERIFY(!ranked.empty());
    QCOMPARE(ranked.front().frequencies.front().samples.front().value, 50);
}

void FrequencyRankTest::best_candidate_decides_the_trace() {
    auto lookup = result("ab", "ab", 1);
    lookup.traces = {TraceCandidate{2, 1, "ab"}, TraceCandidate{0, 3, "ab"}};
    auto other = result("ab", "ab", 9);
    other.traces = {TraceCandidate{1, 0, "ab"}};
    const auto ranked =
        rank_by_frequency({lookup, other}, FrequencySortOrder::ascending, "freq");
    QVERIFY(!ranked.empty());
    QCOMPARE(ranked.front().frequencies.front().samples.front().value, 1);
}

void FrequencyRankTest::no_trace_sorts_last() {
    auto traced = result("ab", "ab", 9);
    traced.traces.push_back(TraceCandidate{5, 5, "ab"});
    const auto ranked = rank_by_frequency({result("ab", "ab", 1), traced},
                                          FrequencySortOrder::ascending, "freq");
    QVERIFY(!ranked.empty());
    QCOMPARE(ranked.front().frequencies.front().samples.front().value, 9);
}

void FrequencyRankTest::ascending_keeps_the_smallest_value() {
    auto lookup = result("ab", "ab", 0);
    lookup.frequencies.front().samples.push_back(FrequencySample{4});
    lookup.frequencies.front().samples.push_back(FrequencySample{-1});
    auto other = result("ab", "ab", 3);
    const auto ranked =
        rank_by_frequency({other, lookup}, FrequencySortOrder::ascending, "freq");
    QVERIFY(!ranked.empty());
    QCOMPARE(ranked.front().frequencies.front().samples.front().value, 0);
}

void FrequencyRankTest::descending_keeps_the_largest_value() {
    auto lookup = result("ab", "ab", 2);
    lookup.frequencies.front().samples.push_back(FrequencySample{6});
    auto other = result("ab", "ab", 5);
    const auto ranked =
        rank_by_frequency({other, lookup}, FrequencySortOrder::descending, "freq");
    QVERIFY(!ranked.empty());
    QCOMPARE(ranked.front().frequencies.front().samples.front().value, 2);
}

void FrequencyRankTest::a_value_ranks_ahead_of_none() {
    auto missing = result("ab", "ab", 1);
    missing.frequencies.clear();
    const auto ranked = rank_by_frequency({missing, result("ab", "ab", 9)},
                                          FrequencySortOrder::descending, "freq");
    QVERIFY(!ranked.empty());
    QCOMPARE(ranked.front().frequencies.size(), 1u);
}

void FrequencyRankTest::equal_values_keep_input_order() {
    const auto first = result("ab", "first", 3);
    const auto second = result("ab", "second", 3);
    const auto ranked =
        rank_by_frequency({first, second}, FrequencySortOrder::ascending, "freq");
    QCOMPARE(ranked.size(), 2u);
    QCOMPARE(ranked.front().expression, std::string("first"));
    QCOMPARE(ranked.back().expression, std::string("second"));
}

void FrequencyRankTest::ignores_other_dictionaries_and_negative_values() {
    auto lookup = result("ab", "ab", -3);
    lookup.frequencies.push_back(FrequencyReading{"other", {FrequencySample{1}}});
    auto other = result("ab", "other", 8);
    const auto ranked =
        rank_by_frequency({lookup, other}, FrequencySortOrder::ascending, "freq");
    QVERIFY(!ranked.empty());
    QCOMPARE(ranked.front().expression, std::string("other"));
}

QTEST_APPLESS_MAIN(FrequencyRankTest)
#include "tst_frequency_rank.moc"
