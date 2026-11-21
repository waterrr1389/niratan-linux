// SPDX-License-Identifier: GPL-3.0-or-later
//
// The pure Sasayaki player policy, following Features/Sasayaki/SasayakiPlayer.swift
// (playCue(from:stop:), tick, seek, flushPlayback, skip(forward:),
// seekRelative, updateCue, the rate and delay didSet observers and the
// constants skipInterval and seekLandingTolerance) and
// NativeMac/NativeReaderView.swift (pushPopups, closePopup(resumePausedPlayback:)).

#include "viewmodels/sasayaki/sasayaki_player_policy.h"

#include <QtTest>

#include <limits>
#include <optional>

using niratan::viewmodels::clamp_delay;
using niratan::viewmodels::clamp_rate;
using niratan::viewmodels::clamped_seek_target;
using niratan::viewmodels::clamped_skip_target;
using niratan::viewmodels::cue_reference_time;
using niratan::viewmodels::flush_position;
using niratan::viewmodels::follows_playback;
using niratan::viewmodels::pause_for_lookup;
using niratan::viewmodels::persisted_second;
using niratan::viewmodels::play_cue_stop_time;
using niratan::viewmodels::resume_after_lookup;
using niratan::viewmodels::stop_reached;
using niratan::viewmodels::tick_lands;

namespace {

void expect_position(const std::optional<double>& actual, std::optional<double> expected) {
    if (!expected.has_value()) {
        QVERIFY(!actual.has_value());
        return;
    }
    QVERIFY(actual.has_value());
    QCOMPARE(*actual, *expected);
}

}  // namespace

class SasayakiViewModelTest : public QObject {
    Q_OBJECT

   private slots:
    void playCueStopTime();
    void stopReached();
    void tickLands();
    void persistedSecond_data();
    void persistedSecond();
    void flushPosition();
    void skipTargets();
    void seekTargets();
    void rateAndDelayClamps();
    void followsPlayback();
    void lookupPauseAndResume();
    void nextAndPreviousReference();
};

// B1: playCue(stop:) sets cue.endTime + delay only when stop is true.
void SasayakiViewModelTest::playCueStopTime() {
    QVERIFY(!play_cue_stop_time(12.5, 0.3, false).has_value());
    QVERIFY(!play_cue_stop_time(5.0, -1.25, false).has_value());
    expect_position(play_cue_stop_time(12.5, 0.3, true), 12.5 + 0.3);
    expect_position(play_cue_stop_time(5.0, -1.25, true), 5.0 - 1.25);
}

// B2: the stop time fires at the target and after it, paused or playing.
void SasayakiViewModelTest::stopReached() {
    QVERIFY(!stop_reached(std::nullopt, 100.0));
    QVERIFY(!stop_reached(10.0, 9.999));
    QVERIFY(stop_reached(10.0, 10.0));
    QVERIFY(stop_reached(10.0, 10.5));
}

// B3: a tick within 0.75 s of the pending target lands, a farther one is
// ignored.
void SasayakiViewModelTest::tickLands() {
    QVERIFY(tick_lands(3.0, std::nullopt));
    QVERIFY(tick_lands(10.0, 10.0));
    QVERIFY(tick_lands(10.75, 10.0));
    QVERIFY(tick_lands(9.25, 10.0));
    QVERIFY(!tick_lands(10.750001, 10.0));
    QVERIFY(!tick_lands(9.249999, 10.0));
}

// B4: the tick persists the raw seconds once per whole second.
void SasayakiViewModelTest::persistedSecond_data() {
    QTest::addColumn<double>("seconds");
    QTest::addColumn<int>("last_second");
    QTest::addColumn<bool>("writes");
    QTest::addColumn<double>("expected");

    QTest::newRow("first second") << 0.0 << -1 << true << 0.0;
    QTest::newRow("same whole second") << 0.999 << 0 << false << 0.0;
    QTest::newRow("whole second") << 1.0 << 0 << true << 1.0;
    QTest::newRow("raw seconds are written") << 1.99 << 0 << true << 1.99;
    QTest::newRow("floor keeps the second") << 2.999 << 2 << false << 0.0;
    QTest::newRow("floor crosses at the second") << 3.0 << 2 << true << 3.0;
    QTest::newRow("delay crossing") << 4.2 << 4 << false << 0.0;
}

void SasayakiViewModelTest::persistedSecond() {
    QFETCH(double, seconds);
    QFETCH(int, last_second);
    QFETCH(bool, writes);
    QFETCH(double, expected);

    const std::optional<double> written = persisted_second(seconds, last_second);
    QCOMPARE(written.has_value(), writes);
    if (writes && written.has_value()) {
        QCOMPARE(*written, expected);
    }
}

// B5: a pending target is written as-is first; a paused player writes
// nothing; a playing player samples its position when finite.
void SasayakiViewModelTest::flushPosition() {
    expect_position(flush_position(7.5, true, 3.0, 2.0), 7.5);
    expect_position(flush_position(7.5, false, std::nullopt, 2.0), 7.5);
    expect_position(flush_position(std::nullopt, false, 4.0, 3.0), std::nullopt);
    expect_position(flush_position(std::nullopt, true, 4.25, 3.0), 4.25);
    expect_position(flush_position(std::nullopt, true, std::nullopt, 3.5), 3.5);
    expect_position(
        flush_position(std::nullopt, true, std::numeric_limits<double>::quiet_NaN(), 3.5), 3.5);
    expect_position(
        flush_position(std::nullopt, true, std::numeric_limits<double>::infinity(), 3.5), 3.5);
}

// B7: skip moves 15 s, forward capped by a known duration, backward at 0.
void SasayakiViewModelTest::skipTargets() {
    QCOMPARE(clamped_skip_target(10.0, 100.0, true), 25.0);
    QCOMPARE(clamped_skip_target(95.0, 100.0, true), 100.0);
    QCOMPARE(clamped_skip_target(10.0, 0.0, true), 25.0);
    QCOMPARE(clamped_skip_target(20.0, 0.0, true), 35.0);
    QCOMPARE(clamped_skip_target(20.0, 0.0, false), 5.0);
    QCOMPARE(clamped_skip_target(10.0, 100.0, false), 0.0);
    QCOMPARE(clamped_skip_target(5.0, 100.0, false), 0.0);
}

// B7: a relative seek moves by the delta, clamped to a known duration and
// never below zero.
void SasayakiViewModelTest::seekTargets() {
    QCOMPARE(clamped_seek_target(12.5, 100.0), 12.5);
    QCOMPARE(clamped_seek_target(-1.0, 100.0), 0.0);
    QCOMPARE(clamped_seek_target(150.0, 100.0), 100.0);
    QCOMPARE(clamped_seek_target(-1.0, 0.0), 0.0);
    QCOMPARE(clamped_seek_target(7.0, 0.0), 7.0);
}

// B9 and D5: the speed and delay slider ranges.
void SasayakiViewModelTest::rateAndDelayClamps() {
    QCOMPARE(clamp_rate(0.5), 0.5);
    QCOMPARE(clamp_rate(2.5), 2.5);
    QCOMPARE(clamp_rate(0.1), 0.5);
    QCOMPARE(clamp_rate(3.0), 2.5);
    QCOMPARE(clamp_rate(1.25), 1.25);

    QCOMPARE(clamp_delay(-2.0), -2.0);
    QCOMPARE(clamp_delay(2.0), 2.0);
    QCOMPARE(clamp_delay(-3.0), -2.0);
    QCOMPARE(clamp_delay(3.0), 2.0);
    QCOMPARE(clamp_delay(0.35), 0.35);
}

// B11 and B12: reveal and cross-chapter follow need auto-scroll and a start.
void SasayakiViewModelTest::followsPlayback() {
    QVERIFY(follows_playback(true, true));
    QVERIFY(!follows_playback(true, false));
    QVERIFY(!follows_playback(false, true));
    QVERIFY(!follows_playback(false, false));
}

// B13: only a playing player with auto-pause is paused by a lookup, and only
// a lookup pause is resumed.
void SasayakiViewModelTest::lookupPauseAndResume() {
    QVERIFY(pause_for_lookup(true, true));
    QVERIFY(!pause_for_lookup(true, false));
    QVERIFY(!pause_for_lookup(false, true));
    QVERIFY(!pause_for_lookup(false, false));

    QVERIFY(resume_after_lookup(true, false));
    QVERIFY(!resume_after_lookup(true, true));
    QVERIFY(!resume_after_lookup(false, false));
    QVERIFY(!resume_after_lookup(false, true));
}

// B8: next/prev cue navigate from the active cue's start; without one, a
// previous-cue lookup clamps a negative reference to zero.
void SasayakiViewModelTest::nextAndPreviousReference() {
    QCOMPARE(cue_reference_time(true, 42.0, -5.0, false), 42.0);
    QCOMPARE(cue_reference_time(true, 42.0, -5.0, true), 42.0);
    QCOMPARE(cue_reference_time(false, 0.0, 7.5, false), 7.5);
    QCOMPARE(cue_reference_time(false, 0.0, -2.0, false), -2.0);
    QCOMPARE(cue_reference_time(false, 0.0, 7.5, true), 7.5);
    QCOMPARE(cue_reference_time(false, 0.0, -2.0, true), 0.0);
}

QTEST_APPLESS_MAIN(SasayakiViewModelTest)
#include "tst_sasayaki_view_model.moc"
