// SPDX-License-Identifier: GPL-3.0-or-later
//
// PlaybackEngine.swift: VideoPlaybackSpeed.
// VideoControlsView.swift: commitSpeedInput.
// VideoPlayerViewModel.swift: setABLoopStart, setABLoopEnd, clearABLoop.

#include "services/player/playback_controls.h"

#include <QtTest>

#include <limits>

using namespace niratan::player;

class PlaybackControlsTest : public QObject {
    Q_OBJECT

private slots:
    void speedNormalization() {
        QCOMPARE(normalized_speed(-1), 0.25);
        QCOMPARE(normalized_speed(0.25), 0.25);
        QCOMPARE(normalized_speed(0.26), 0.3);
        QCOMPARE(normalized_speed(1.26), 1.3);
        QCOMPARE(normalized_speed(7), 5.0);
        QCOMPARE(normalized_speed(std::numeric_limits<double>::infinity()), 1.0);
    }

    void speedEntry() {
        QVERIFY(!parse_speed(u" "));
        QVERIFY(!parse_speed(u" 2x "));
        QVERIFY(!parse_speed(u"1,2.3"));
        QCOMPARE(parse_speed(u" 1,26 "), std::optional(1.3));
        QCOMPARE(parse_speed(u"inf"), std::optional(1.0));
        QCOMPARE(speed_label(0.25), QStringLiteral("0.25"));
        QCOMPARE(speed_label(2), QStringLiteral("2"));
        QCOMPARE(speed_label(1.5), QStringLiteral("1.5"));
    }

    void loopNeedsOrderedEndpoints() {
        ABLoop loop;
        QVERIFY(!loop.set_end(4, 20));
        QVERIFY(loop.set_start(10, 20));
        QCOMPARE(loop.start(), std::optional(10.0));
        QVERIFY(!loop.end());
        QVERIFY(!loop.set_end(10, 20));
        QVERIFY(!loop.set_end(9, 20));
        QVERIFY(!loop.end());
        QVERIFY(loop.set_end(14, 20));
        QVERIFY(!loop.set_end(9, 20));
        QCOMPARE(loop.end(), std::optional(14.0));
        QVERIFY(loop.set_end(18, 20));
        QCOMPARE(loop.end(), std::optional(18.0));
    }

    void loopClampsAndClears() {
        ABLoop loop;
        QVERIFY(loop.set_start(-1, 20));
        QCOMPARE(loop.start(), std::optional(0.0));
        QVERIFY(loop.set_end(30, 20));
        QCOMPARE(loop.end(), std::optional(20.0));
        QVERIFY(loop.set_start(4, 20));
        QVERIFY(!loop.end());
        QVERIFY(!loop.set_end(std::numeric_limits<double>::quiet_NaN(), 20));
        loop.clear();
        QVERIFY(!loop.start());
        QVERIFY(!loop.end());
        QVERIFY(loop.set_start(40, 0));
        QCOMPARE(loop.start(), std::optional(40.0));
    }
};

QTEST_APPLESS_MAIN(PlaybackControlsTest)
#include "tst_playback_controls.moc"
