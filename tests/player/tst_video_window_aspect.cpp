// SPDX-License-Identifier: GPL-3.0-or-later
//
// Window aspect arithmetic, following
// Features/Video/VideoWindowChromeController.swift `VideoWindowAspectLayout`
// and script/test_video_window_aspect_layout.swift. The cases below are the
// Swift script's window-sizing cases with the Linux content-size convention:
// no title-bar decoration, since the desktop owns the window frame.

#include "models/video_window_aspect.h"

#include <QSizeF>
#include <QtTest>

#include <cmath>

namespace {

namespace video = niratan::video;

constexpr qreal kTolerance = 0.001;

bool near(qreal lhs, qreal rhs, qreal tolerance = kTolerance) {
    return std::abs(lhs - rhs) <= tolerance;
}

bool near(const QSizeF& lhs, const QSizeF& rhs, qreal tolerance = kTolerance) {
    return near(lhs.width(), rhs.width(), tolerance) && near(lhs.height(), rhs.height(), tolerance);
}

}  // namespace

class VideoWindowAspectTest : public QObject {
    Q_OBJECT

   private slots:
    void override_ratios_match_swift();
    void aspect_ratio_prefers_override_and_rotates();
    void fitted_content_size_reserves_the_sidebar_and_the_screen();
    void sidebar_width_absorbs_the_aspect_difference();
    void resize_driver_follows_the_dominant_delta();
    void constrained_resize_locks_the_video_aspect();
    void constrained_resize_keeps_a_frozen_driver_monotonic();
    void constrained_resize_respects_the_minimum_envelope();
    void invalid_inputs_pass_through();
};

void VideoWindowAspectTest::override_ratios_match_swift() {
    QVERIFY(near(*video::aspect_override_ratio(QStringLiteral("16:9")), 16.0 / 9.0));
    QVERIFY(near(*video::aspect_override_ratio(QStringLiteral("4:3")), 4.0 / 3.0));
    QVERIFY(near(*video::aspect_override_ratio(QStringLiteral("1:1")), 1.0));
    QVERIFY(near(*video::aspect_override_ratio(QStringLiteral("21:9")), 21.0 / 9.0));
    QVERIFY(!video::aspect_override_ratio(QStringLiteral("-1")));
    QVERIFY(!video::aspect_override_ratio(QString()));
    QVERIFY(!video::aspect_override_ratio(QStringLiteral("automatic")));
}

void VideoWindowAspectTest::aspect_ratio_prefers_override_and_rotates() {
    const QSizeF wide(1920, 1080);
    const auto automatic = video::video_aspect_ratio(wide, std::nullopt, 0);
    QVERIFY(automatic);
    QVERIFY(near(*automatic, 16.0 / 9.0));

    const auto rotated = video::video_aspect_ratio(wide, std::nullopt, 90);
    QVERIFY(rotated);
    QVERIFY(near(*rotated, 9.0 / 16.0));

    const auto explicit_override =
        video::video_aspect_ratio(wide, video::aspect_override_ratio(QStringLiteral("4:3")), 90);
    QVERIFY(explicit_override);
    QVERIFY(near(*explicit_override, 3.0 / 4.0));

    const auto from_override = video::video_aspect_ratio(
        QSizeF(), video::aspect_override_ratio(QStringLiteral("16:9")), 0);
    QVERIFY(from_override);
    QVERIFY(near(*from_override, 16.0 / 9.0));

    QVERIFY(!video::video_aspect_ratio(QSizeF(), std::nullopt, 0));
    QVERIFY(!video::video_aspect_ratio(QSizeF(0, 1080), std::nullopt, 0));
    QVERIFY(!video::video_aspect_ratio(wide, 0.0, 0));
    QVERIFY(!video::video_aspect_ratio(wide, -1.0, 0));
}

void VideoWindowAspectTest::fitted_content_size_reserves_the_sidebar_and_the_screen() {
    const QSizeF content =
        video::fitted_content_size(QSizeF(1200, 760), 16.0 / 9.0, 340, QSizeF(2000, 1200));
    QVERIFY(near(content.width(), content.height() * 16.0 / 9.0 + 340));

    const QSizeF clamped =
        video::fitted_content_size(QSizeF(1600, 900), 16.0 / 9.0, 340, QSizeF(1280, 720));
    QVERIFY(clamped.width() <= 1280.0);
    QVERIFY(near(clamped.width(), clamped.height() * 16.0 / 9.0 + 340));

    QCOMPARE(video::fitted_content_size(QSizeF(1000, 600), 0.0, 340, QSizeF(1280, 720)),
             QSizeF(1000, 600));
}

void VideoWindowAspectTest::sidebar_width_absorbs_the_aspect_difference() {
    const qreal resolved =
        video::aspect_fitting_sidebar_width(QSizeF(1700, 765), 16.0 / 9.0, 340, 320, 560);
    QVERIFY(near(resolved, 340));

    const qreal too_narrow =
        video::aspect_fitting_sidebar_width(QSizeF(1200, 800), 16.0 / 9.0, 400, 320, 560);
    QVERIFY(near(too_narrow, 400));

    QCOMPARE(video::aspect_fitting_sidebar_width(QSizeF(1200, 700), std::nullopt, 400, 320, 560),
             400.0);
    // A zero ratio is "no ratio known": the clamped proposal wins.
    QCOMPARE(video::aspect_fitting_sidebar_width(QSizeF(1700, 765), 0.0, 340, 320, 560), 340.0);
    QCOMPARE(video::aspect_fitting_sidebar_width(QSizeF(1700, 765), 0.0, 400, 320, 560), 400.0);
    QCOMPARE(video::aspect_fitting_sidebar_width(QSizeF(1700, 765), 0.0, 900, 320, 560), 560.0);
    QCOMPARE(video::aspect_fitting_sidebar_width(QSizeF(1200, 700), 16.0 / 9.0, 100, 320, 560),
             320.0);
    QCOMPARE(video::aspect_fitting_sidebar_width(QSizeF(1200, 700), 16.0 / 9.0, 900, 320, 560),
             560.0);
}

void VideoWindowAspectTest::resize_driver_follows_the_dominant_delta() {
    QCOMPARE(video::resize_driver(QSizeF(1280, 748), QSizeF(1600, 748), 16.0 / 9.0),
             std::optional(video::ResizeDriver::Width));
    QCOMPARE(video::resize_driver(QSizeF(1280, 748), QSizeF(1280, 928), 16.0 / 9.0),
             std::optional(video::ResizeDriver::Height));
    QCOMPARE(video::resize_driver(QSizeF(1280, 748), QSizeF(1480, 808), 16.0 / 9.0),
             std::optional(video::ResizeDriver::Width));
    QCOMPARE(video::resize_driver(QSizeF(1280, 748), QSizeF(1284, 800), 16.0 / 9.0),
             std::optional(video::ResizeDriver::Height));
    QCOMPARE(video::resize_driver(QSizeF(1280, 748), QSizeF(1280.2, 748.2), 16.0 / 9.0),
             std::optional<video::ResizeDriver>());
    QCOMPARE(video::resize_driver(QSizeF(), QSizeF(1280, 748), 16.0 / 9.0),
             std::optional<video::ResizeDriver>());
}

void VideoWindowAspectTest::constrained_resize_locks_the_video_aspect() {
    const QSizeF current(1280, 748);
    const QSizeF decoration(0, 28);
    const QSizeF minimum(900, 620);
    const qreal ratio = 16.0 / 9.0;

    const QSizeF horizontal =
        video::constrained_frame_size(current, QSizeF(1600, 748), decoration, ratio, 0, minimum);
    QVERIFY(near(horizontal.width(), 1600.0));
    QVERIFY(near(horizontal.height(), 928.0));

    const QSizeF vertical =
        video::constrained_frame_size(current, QSizeF(1280, 928), decoration, ratio, 0, minimum);
    QVERIFY(near(vertical.width(), 1600.0));
    QVERIFY(near(vertical.height(), 928.0));

    const QSizeF corner =
        video::constrained_frame_size(current, QSizeF(1480, 808), decoration, ratio, 0, minimum);
    QVERIFY(near(corner.width(), 1480.0));
    QVERIFY(near(corner.height(), 860.5));

    const QSizeF sidebar = video::constrained_frame_size(QSizeF(1620, 748), QSizeF(1940, 748),
                                                         decoration, ratio, 340, minimum);
    QVERIFY(near(sidebar.width(), 1940.0));
    QVERIFY(near(sidebar.height(), 928.0));
    QVERIFY(near(sidebar.width() - 340.0, (sidebar.height() - 28.0) * ratio));

    QCOMPARE(video::constrained_frame_size(current, QSizeF(1280.2, 748.2), decoration, ratio, 0,
                                           minimum),
             QSizeF(1280.2, 748.2));
}

void VideoWindowAspectTest::constrained_resize_keeps_a_frozen_driver_monotonic() {
    const QSizeF decoration(0, 28);
    const QSizeF minimum(900, 620);
    const qreal ratio = 16.0 / 9.0;

    QSizeF current(1280, 748);
    QList<QSizeF> width_results;
    for (qreal proposedWidth : {1320.0, 1400.0, 1480.0}) {
        const QSizeF result =
            video::constrained_frame_size(current, QSizeF(proposedWidth, 748), decoration, ratio, 0,
                                          minimum, video::ResizeDriver::Width);
        width_results.append(result);
        current = result;
    }
    for (qsizetype index = 1; index < width_results.size(); ++index) {
        QVERIFY(width_results.at(index).width() > width_results.at(index - 1).width());
        QVERIFY(width_results.at(index).height() > width_results.at(index - 1).height());
    }
    QCOMPARE(video::constrained_frame_size(current, QSizeF(1480, 748), decoration, ratio, 0,
                                           minimum, video::ResizeDriver::Width),
             current);

    current = QSizeF(1280, 748);
    QList<QSizeF> height_results;
    for (qreal proposedHeight : {768.0, 828.0, 928.0}) {
        const QSizeF result =
            video::constrained_frame_size(current, QSizeF(1280, proposedHeight), decoration, ratio,
                                          0, minimum, video::ResizeDriver::Height);
        height_results.append(result);
        current = result;
    }
    for (qsizetype index = 1; index < height_results.size(); ++index) {
        QVERIFY(height_results.at(index).width() > height_results.at(index - 1).width());
        QVERIFY(height_results.at(index).height() > height_results.at(index - 1).height());
    }
    QCOMPARE(video::constrained_frame_size(current, QSizeF(1280, 928), decoration, ratio, 0,
                                           minimum, video::ResizeDriver::Height),
             current);

    current = QSizeF(1620, 748);
    QList<QSizeF> sidebar_results;
    for (qreal proposedWidth : {1760.0, 1850.0, 1940.0}) {
        const QSizeF result =
            video::constrained_frame_size(current, QSizeF(proposedWidth, 748), decoration, ratio,
                                          340, minimum, video::ResizeDriver::Width);
        sidebar_results.append(result);
        current = result;
    }
    for (qsizetype index = 1; index < sidebar_results.size(); ++index) {
        QVERIFY(sidebar_results.at(index).width() > sidebar_results.at(index - 1).width());
        QVERIFY(sidebar_results.at(index).height() > sidebar_results.at(index - 1).height());
    }
    for (const QSizeF& result : sidebar_results) {
        QVERIFY(near(result.width() - 340.0, (result.height() - 28.0) * ratio));
    }
    QCOMPARE(video::constrained_frame_size(current, QSizeF(1940, 748), decoration, ratio, 340,
                                           minimum, video::ResizeDriver::Width),
             current);
}

void VideoWindowAspectTest::constrained_resize_respects_the_minimum_envelope() {
    const QSizeF decoration(0, 28);
    const QSizeF minimum(900, 620);
    const qreal ratio = 16.0 / 9.0;

    const QSizeF minimum_resize = video::constrained_frame_size(QSizeF(1280, 748), QSizeF(800, 500),
                                                                decoration, ratio, 0, minimum);
    QVERIFY(near(minimum_resize.width(), 592.0 * 16.0 / 9.0));
    QVERIFY(near(minimum_resize.height(), 620.0));

    const QSizeF iina_minimum =
        video::constrained_frame_size(QSizeF(1280, 720), QSizeF(240, 120), QSizeF(), ratio, 0,
                                      QSizeF(285, 120), video::ResizeDriver::Width);
    QVERIFY(near(iina_minimum, QSizeF(285, 160.3125)));

    const QSizeF oversized =
        video::constrained_frame_size(QSizeF(1280, 720), QSizeF(8192, 4608), QSizeF(), ratio, 0,
                                      QSizeF(285, 120), video::ResizeDriver::Width);
    QVERIFY(near(oversized, QSizeF(8192, 4608)));
}

void VideoWindowAspectTest::invalid_inputs_pass_through() {
    const QSizeF proposal(1400, 900);
    QCOMPARE(video::constrained_frame_size(QSizeF(1280, 748), proposal, QSizeF(0, 28), std::nan(""),
                                           0, QSizeF(900, 620)),
             proposal);
    QCOMPARE(video::constrained_frame_size(QSizeF(), proposal, QSizeF(0, 28), 16.0 / 9.0, 0,
                                           QSizeF(900, 620)),
             proposal);
    QCOMPARE(video::constrained_frame_size(QSizeF(1280, 748), QSizeF(), QSizeF(0, 28), 16.0 / 9.0,
                                           0, QSizeF(900, 620)),
             QSizeF());
}

QTEST_APPLESS_MAIN(VideoWindowAspectTest)

#include "tst_video_window_aspect.moc"
