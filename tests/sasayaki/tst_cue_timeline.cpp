// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Sasayaki/SasayakiPlayer.swift (CueTimeline) is the specification.
// The 0.01 second window is tested with starts of 0.01 and 0.02 because those
// are exact multiples of the double nearest 0.01, so a difference of exactly
// 0.01 is representable in that range; the window is inclusive at both ends.

#include <QTest>

#include <string>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "services/sasayaki/cue_timeline.h"

using niratan::sasayaki::CueTimeline;
using niratan::sasayaki::SasayakiCue;
using niratan::sasayaki::seek_target;

namespace {

SasayakiCue make_cue(const std::string& id, double start, double end) {
    SasayakiCue cue;
    cue.id = id;
    cue.start_time = start;
    cue.end_time = end;
    cue.text = id;
    return cue;
}

std::vector<SasayakiCue> two_cues() {
    return {make_cue("first", 10.0, 12.0), make_cue("second", 20.0, 22.0)};
}

}  // namespace

class CueTimelineTest : public QObject {
    Q_OBJECT

   private slots:
    void inside_a_cue();
    void between_cues_without_a_previous_cue();
    void after_the_previous_cue_ended();
    void start_window_at_both_edges();
    void just_outside_the_start_window();
    void next_cue_skips_an_exact_start();
    void previous_cue_uses_the_insertion_point();
    void delay_shifts_lookup_and_seek();
    void sorts_cues_by_start();
    void empty_timeline_has_no_cues();
};

void CueTimelineTest::inside_a_cue() {
    const CueTimeline timeline(two_cues());
    const SasayakiCue* cue = timeline.cue_at(11.0);
    QVERIFY(cue != nullptr);
    QCOMPARE(cue->id, std::string("first"));

    const SasayakiCue* exact = timeline.cue_at(10.0);
    QVERIFY(exact != nullptr);
    QCOMPARE(exact->id, std::string("first"));
}

void CueTimelineTest::between_cues_without_a_previous_cue() {
    const CueTimeline timeline(two_cues());
    QVERIFY(timeline.cue_at(5.0) == nullptr);
    QVERIFY(timeline.cue_at(9.0) == nullptr);
}

void CueTimelineTest::after_the_previous_cue_ended() {
    const CueTimeline timeline(two_cues());
    QVERIFY(timeline.cue_at(13.0) == nullptr);
    QVERIFY(timeline.cue_at(15.0) == nullptr);

    const SasayakiCue* at_end = timeline.cue_at(12.0);
    QVERIFY(at_end != nullptr);
    QCOMPARE(at_end->id, std::string("first"));
}

void CueTimelineTest::start_window_at_both_edges() {
    // A start 0.01 s after the lookup time: delta is exactly +0.01, the upper
    // edge of the window. find() lands on that cue.
    const CueTimeline ahead({make_cue("ahead", 0.02, 0.03)});
    const SasayakiCue* before_start = ahead.cue_at(0.01);
    QVERIFY(before_start != nullptr);
    QCOMPARE(before_start->id, std::string("ahead"));

    // A start 0.01 s before the lookup time. find() lands past that cue, so
    // this is the fallback branch; the cue returns because the lookup time
    // equals its end time, the lower edge of the "previous cue is still
    // playing" rule.
    const CueTimeline behind({make_cue("behind", 0.01, 0.02)});
    const SasayakiCue* after_start = behind.cue_at(0.02);
    QVERIFY(after_start != nullptr);
    QCOMPARE(after_start->id, std::string("behind"));
}

void CueTimelineTest::just_outside_the_start_window() {
    const CueTimeline ahead({make_cue("ahead", 0.02, 0.03)});
    QVERIFY(ahead.cue_at(0.009) == nullptr);

    const CueTimeline behind({make_cue("behind", 0.01, 0.012)});
    QVERIFY(behind.cue_at(0.021) == nullptr);
}

void CueTimelineTest::next_cue_skips_an_exact_start() {
    const CueTimeline timeline(two_cues());

    const SasayakiCue* from_start = timeline.next_cue_after(10.0);
    QVERIFY(from_start != nullptr);
    QCOMPARE(from_start->id, std::string("second"));

    const SasayakiCue* from_before = timeline.next_cue_after(0.0);
    QVERIFY(from_before != nullptr);
    QCOMPARE(from_before->id, std::string("first"));

    const SasayakiCue* from_between = timeline.next_cue_after(15.0);
    QVERIFY(from_between != nullptr);
    QCOMPARE(from_between->id, std::string("second"));

    QVERIFY(timeline.next_cue_after(20.0) == nullptr);
    QVERIFY(timeline.next_cue_after(25.0) == nullptr);
}

void CueTimelineTest::previous_cue_uses_the_insertion_point() {
    const CueTimeline timeline(two_cues());

    const SasayakiCue* before_second = timeline.previous_cue_before(20.0);
    QVERIFY(before_second != nullptr);
    QCOMPARE(before_second->id, std::string("first"));

    const SasayakiCue* between = timeline.previous_cue_before(15.0);
    QVERIFY(between != nullptr);
    QCOMPARE(between->id, std::string("first"));

    QVERIFY(timeline.previous_cue_before(10.0) == nullptr);
    QVERIFY(timeline.previous_cue_before(0.0) == nullptr);
}

void CueTimelineTest::delay_shifts_lookup_and_seek() {
    const CueTimeline timeline({make_cue("cue", 100.0, 105.0)});

    const SasayakiCue* late = timeline.cue_at(100.5 - 0.5);
    QVERIFY(late != nullptr);
    QCOMPARE(late->id, std::string("cue"));
    QCOMPARE(seek_target(*late, 0.5), 100.5);

    const SasayakiCue* early = timeline.cue_at(99.75 + 0.25);
    QVERIFY(early != nullptr);
    QCOMPARE(seek_target(*early, -0.25), 99.75);
}

void CueTimelineTest::sorts_cues_by_start() {
    CueTimeline timeline({make_cue("late", 20.0, 22.0), make_cue("early", 10.0, 12.0)});
    QCOMPARE(timeline.cues().size(), std::size_t(2));
    QCOMPARE(timeline.cues().front().id, std::string("early"));

    const SasayakiCue* inside = timeline.cue_at(11.0);
    QVERIFY(inside != nullptr);
    QCOMPARE(inside->id, std::string("early"));

    const SasayakiCue* next = timeline.next_cue_after(10.0);
    QVERIFY(next != nullptr);
    QCOMPARE(next->id, std::string("late"));

    const SasayakiCue* previous = timeline.previous_cue_before(20.0);
    QVERIFY(previous != nullptr);
    QCOMPARE(previous->id, std::string("early"));
}

void CueTimelineTest::empty_timeline_has_no_cues() {
    const CueTimeline timeline;
    QVERIFY(timeline.empty());
    QVERIFY(timeline.cues().empty());
    QVERIFY(timeline.cue_at(0.0) == nullptr);
    QVERIFY(timeline.cue_at(42.0) == nullptr);
    QVERIFY(timeline.next_cue_after(0.0) == nullptr);
    QVERIFY(timeline.previous_cue_before(42.0) == nullptr);
}

QTEST_APPLESS_MAIN(CueTimelineTest)

#include "tst_cue_timeline.moc"
