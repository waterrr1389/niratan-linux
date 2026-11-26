// SPDX-License-Identifier: GPL-3.0-or-later
//
// V4 B1-B5 of notes/specs/video-v4-subtitle-gap-speed.md:
// VideoPlayerViewModel.swift `shouldFastForwardSubtitleGap`,
// `updateSubtitleGapPlayback`, `restoreSubtitleGapFastForwardSpeedIfNeeded`,
// `setSpeed`, `adjustSpeed`, `currentUserSpeed`, `snapshotForPersistence`,
// `setSubtitleGapFastForwardEnabled`, `setSubtitleGapFastForwardSpeed` and
// `normalizedSubtitleGapFastForwardSpeed`, over SubtitleCueStore.swift
// `slice` and PlaybackEngine.swift `VideoPlaybackSpeed.normalized`.

#include "services/player/subtitle_gap_playback.h"
#include "services/player/subtitle_document.h"

#include <QtTest>

#include <limits>

using niratan::player::SubtitleCue;
using niratan::player::SubtitleCueStore;
using niratan::player::SubtitleDocument;
using niratan::player::SubtitleGapPlayback;

namespace {

SubtitleCueStore store_with(std::initializer_list<SubtitleCue> cues) {
    SubtitleDocument document;
    document.cues = QList<SubtitleCue>(cues);
    return SubtitleCueStore(std::move(document));
}

// A gap from 1.0 s to 4.0 s between two cues.
SubtitleCueStore gap_store() {
    return store_with({{QStringLiteral("a"), 0.0, 1.0, QStringLiteral("previous")},
                       {QStringLiteral("b"), 4.0, 5.0, QStringLiteral("next")}});
}

}  // namespace

class SubtitleGapPlaybackTest : public QObject {
    Q_OBJECT

   private slots:
    void eligibleGapNeedsBothNeighboursAndMargins();
    void capturesBaseAndAccelerates();
    void pausedDisabledOrLeavingRestores();
    void manualSpeedDuringAccelerationMovesBase();
    void configuredSpeedNormalizesAndAppliesWhileActive();
    void persistenceUsesBaseSpeed();
    void delayUsesAdjustedTime();
};

// B1: no showing cue, both neighbours, and strictly more than 0.6 s from each
// edge at playback minus subtitle delay.
void SubtitleGapPlaybackTest::eligibleGapNeedsBothNeighboursAndMargins() {
    SubtitleGapPlayback policy;
    const SubtitleCueStore gap = gap_store();
    QVERIFY(policy.in_eligible_gap(gap, 2.5, 0.0));
    QVERIFY(policy.in_eligible_gap(gap, 1.65, 0.0));
    QVERIFY(policy.in_eligible_gap(gap, 3.35, 0.0));
    QVERIFY(!policy.in_eligible_gap(gap, 1.55, 0.0));
    QVERIFY(!policy.in_eligible_gap(gap, 3.45, 0.0));

    // A showing cue blocks acceleration even with gaps on both sides.
    QVERIFY(
        !policy.in_eligible_gap(store_with({{QStringLiteral("a"), 0.0, 1.0, QStringLiteral("a")},
                                            {QStringLiteral("b"), 2.0, 3.0, QStringLiteral("b")},
                                            {QStringLiteral("c"), 4.0, 5.0, QStringLiteral("c")}}),
                                2.5, 0.0));

    // Before the first cue or after the last one there is nothing to bound a gap.
    QVERIFY(!policy.in_eligible_gap(
        store_with({{QStringLiteral("a"), 4.0, 5.0, QStringLiteral("next")}}), 2.5, 0.0));
    QVERIFY(!policy.in_eligible_gap(
        store_with({{QStringLiteral("a"), 0.0, 1.0, QStringLiteral("previous")}}), 2.5, 0.0));
    QVERIFY(!policy.in_eligible_gap(SubtitleCueStore(SubtitleDocument{}), 2.5, 0.0));
}

// B2: the first qualifying update captures the current speed once, even when
// it already equals or is faster than the configured speed, and applies the
// configured speed only when it differs by at least 0.001.
void SubtitleGapPlaybackTest::capturesBaseAndAccelerates() {
    const SubtitleCueStore gap = gap_store();

    SubtitleGapPlayback slower;
    slower.set_enabled(true, 1.0);
    QVERIFY(!slower.accelerating());
    const auto applied = slower.update(gap, 2.5, 0.0, true, 1.0);
    QVERIFY(applied.has_value());
    QCOMPARE(*applied, 2.7);
    QVERIFY(slower.accelerating());
    QCOMPARE(slower.user_speed(2.7), 1.0);

    // Still in the gap: the base stays and the effective speed is unchanged.
    QVERIFY(!slower.update(gap, 2.6, 0.0, true, 2.7).has_value());
    QCOMPARE(slower.user_speed(2.7), 1.0);

    SubtitleGapPlayback already_fast;
    already_fast.set_enabled(true, 2.7);
    QVERIFY(!already_fast.update(gap, 2.5, 0.0, true, 2.7).has_value());
    QVERIFY(already_fast.accelerating());
    QCOMPARE(already_fast.user_speed(2.7), 2.7);

    SubtitleGapPlayback faster;
    faster.set_enabled(true, 3.0);
    const auto reduced = faster.update(gap, 2.5, 0.0, true, 3.0);
    QVERIFY(reduced.has_value());
    QCOMPARE(*reduced, 2.7);
    QCOMPARE(faster.user_speed(2.7), 3.0);

    // A difference under 0.001 neither commands nor replaces the base, while
    // one at or above it applies the configured speed and keeps the base.
    SubtitleGapPlayback epsilon;
    epsilon.set_enabled(true, 1.0);
    QVERIFY(!epsilon.update(gap, 2.5, 0.0, true, 2.7005).has_value());
    QVERIFY(epsilon.accelerating());
    QCOMPARE(epsilon.user_speed(2.7), 2.7005);
    const auto outside_epsilon = epsilon.update(gap, 2.5, 0.0, true, 2.702);
    QVERIFY(outside_epsilon.has_value());
    QCOMPARE(*outside_epsilon, 2.7);
    QCOMPARE(epsilon.user_speed(2.7), 2.7005);
}

// B3: leaving the gap, pausing and disabling restore the normalized base and
// clear it; seek, opening media and shutdown use the same restore.
void SubtitleGapPlaybackTest::pausedDisabledOrLeavingRestores() {
    const SubtitleCueStore gap = gap_store();
    SubtitleGapPlayback policy;
    policy.set_enabled(true, 1.0);
    QVERIFY(policy.update(gap, 2.5, 0.0, true, 1.0).has_value());

    const auto paused = policy.update(gap, 2.6, 0.0, false, 2.7);
    QVERIFY(paused.has_value());
    QCOMPARE(*paused, 1.0);
    QVERIFY(!policy.accelerating());
    QVERIFY(!policy.update(gap, 2.6, 0.0, false, 1.0).has_value());

    QVERIFY(policy.update(gap, 2.5, 0.0, true, 1.0).has_value());
    const auto left = policy.update(gap, 0.5, 0.0, true, 2.7);
    QVERIFY(left.has_value());
    QCOMPARE(*left, 1.0);
    QVERIFY(!policy.accelerating());

    QVERIFY(policy.update(gap, 2.5, 0.0, true, 1.0).has_value());
    const auto disabled = policy.set_enabled(false, 2.7);
    QVERIFY(disabled.has_value());
    QCOMPARE(*disabled, 1.0);
    QVERIFY(!policy.accelerating());
    QVERIFY(!policy.enabled());

    QVERIFY(!policy.set_enabled(true, 1.0).has_value());
    QVERIFY(policy.update(gap, 2.5, 0.0, true, 1.5).has_value());
    const auto seek = policy.restore(2.7);
    QVERIFY(seek.has_value());
    QCOMPARE(*seek, 1.5);
    QVERIFY(!policy.restore(1.5).has_value());
    QVERIFY(!policy.accelerating());

    // A base that already matches the engine clears without an update.
    SubtitleGapPlayback matching;
    matching.set_enabled(true, 2.7);
    QVERIFY(!matching.update(gap, 2.5, 0.0, true, 2.7).has_value());
    QVERIFY(!matching.restore(2.7).has_value());
    QVERIFY(!matching.accelerating());
}

// B4: manual changes during acceleration move the base, never the effective
// speed, and relative steps add to the base.
void SubtitleGapPlaybackTest::manualSpeedDuringAccelerationMovesBase() {
    const SubtitleCueStore gap = gap_store();
    SubtitleGapPlayback policy;
    policy.set_enabled(true, 2.0);
    QVERIFY(policy.update(gap, 2.5, 0.0, true, 2.0).has_value());
    QCOMPARE(policy.user_speed(2.7), 2.0);

    QVERIFY(!policy.set_user_speed(1.5, 2.7).has_value());
    QCOMPARE(policy.user_speed(2.7), 1.5);
    QVERIFY(!policy.adjust_user_speed(-0.5, 2.7).has_value());
    QCOMPARE(policy.user_speed(2.7), 1.0);
    QVERIFY(!policy.adjust_user_speed(0.2, 2.7).has_value());
    QCOMPARE(policy.user_speed(2.7), 1.2);

    const auto restored = policy.restore(2.7);
    QVERIFY(restored.has_value());
    QCOMPARE(*restored, 1.2);

    // Outside a gap the normalized speed applies through the caller.
    const auto idle = policy.set_user_speed(1.5, 1.0);
    QVERIFY(idle.has_value());
    QCOMPARE(*idle, 1.5);
    const auto adjusted = policy.adjust_user_speed(0.2, 1.5);
    QVERIFY(adjusted.has_value());
    QCOMPARE(*adjusted, 1.7);
}

// B5: the configured speed clamps to [1.1, 5.0], falls back to 2.7 when
// non-finite, snaps to the 0.1 increment, and applies live only while active.
void SubtitleGapPlaybackTest::configuredSpeedNormalizesAndAppliesWhileActive() {
    SubtitleGapPlayback policy;
    QCOMPARE(policy.configured_speed(), 2.7);
    QVERIFY(!policy.set_configured_speed(3.44, 1.0).has_value());
    QCOMPARE(policy.configured_speed(), 3.4);
    QVERIFY(!policy.set_configured_speed(0.5, 1.0).has_value());
    QCOMPARE(policy.configured_speed(), 1.1);
    QVERIFY(!policy.set_configured_speed(9.0, 1.0).has_value());
    QCOMPARE(policy.configured_speed(), 5.0);
    QVERIFY(
        !policy.set_configured_speed(std::numeric_limits<double>::quiet_NaN(), 1.0).has_value());
    QCOMPARE(policy.configured_speed(), 2.7);

    const SubtitleCueStore gap = gap_store();
    policy.set_enabled(true, 1.0);
    QVERIFY(policy.update(gap, 2.5, 0.0, true, 1.0).has_value());
    QCOMPARE(policy.user_speed(2.7), 1.0);
    QVERIFY(!policy.set_configured_speed(2.7, 2.7).has_value());
    const auto live = policy.set_configured_speed(3.2, 2.7);
    QVERIFY(live.has_value());
    QCOMPARE(*live, 3.2);
    QCOMPARE(policy.user_speed(3.2), 1.0);
}

// B4: saves keep the base speed while the temporary speed runs, and the
// restore normalizes the base before handing it back.
void SubtitleGapPlaybackTest::persistenceUsesBaseSpeed() {
    const SubtitleCueStore gap = gap_store();
    SubtitleGapPlayback policy;
    policy.set_enabled(true, 1.26);
    QVERIFY(policy.update(gap, 2.5, 0.0, true, 1.26).has_value());
    QCOMPARE(policy.persistence_speed(2.7), 1.26);
    QCOMPARE(policy.user_speed(2.7), 1.26);
    const auto restored = policy.restore(2.7);
    QVERIFY(restored.has_value());
    QCOMPARE(*restored, 1.3);
    QVERIFY(!policy.accelerating());
    QCOMPARE(policy.persistence_speed(1.3), 1.3);
}

// B1: the eligibility check runs at playback minus subtitle delay.
void SubtitleGapPlaybackTest::delayUsesAdjustedTime() {
    const SubtitleCueStore gap = gap_store();
    SubtitleGapPlayback policy;
    policy.set_enabled(true, 1.0);
    QVERIFY(!policy.update(gap, 2.5, 1.0, true, 1.0).has_value());
    QVERIFY(policy.update(gap, 3.5, 1.0, true, 1.0).has_value());
}

QTEST_APPLESS_MAIN(SubtitleGapPlaybackTest)

#include "tst_subtitle_gap_playback.moc"
