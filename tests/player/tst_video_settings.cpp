// SPDX-License-Identifier: GPL-3.0-or-later
//
// Core/UserConfig.swift (video defaults and reset), VideoSubtitlePositionPolicy,
// VideoSubtitleEdgePreferenceResolver, VideoSubtitleEdgeRecipe and ColorHexCodec.

#include "services/player/video_settings.h"
#include "services/storage/json_file.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace niratan::player;

class VideoSettingsTest : public QObject {
    Q_OBJECT
private slots:
    void defaultsAndBounds() {
        const auto defaults = video_settings();
        QCOMPARE(defaults.value("videoSeekInterval").toDouble(), 5);
        QCOMPARE(defaults.value("videoSubtitleGapFastForwardSpeed").toDouble(), 2.7);
        QCOMPARE(defaults.value("videoSubtitleFontWeight").toInt(), 700);
        QCOMPARE(defaults.value("videoSubtitleLookupHighlightColor").toString(),
                 QStringLiteral("#B5C1CB3E"));
        QVERIFY(defaults.value("videoRememberPlaybackPosition").toBool());
        const auto limits = video_settings({{"videoSeekInterval", 100}, {"videoSubtitleFontSize", 5},
                                            {"videoMiningHistoryLimit", -2}, {"videoHue", -110},
                                            {"videoSubtitleMaskMode", "invalid"}});
        QCOMPARE(limits.value("videoSeekInterval").toDouble(), 60);
        QCOMPARE(limits.value("videoSubtitleFontSize").toDouble(), 12);
        QCOMPARE(limits.value("videoMiningHistoryLimit").toInt(), 0);
        QCOMPARE(limits.value("videoHue").toDouble(), -100);
        QCOMPARE(limits.value("videoSubtitleMaskMode").toString(), QStringLiteral("Blur"));
    }

    void legacyAndColor() {
        const auto old = video_settings({{"videoSubtitleVerticalPosition", 100},
                                         {"videoSubtitleShadowRadius", 7},
                                         {"videoSubtitleColor", "#aBcd"}});
        QCOMPARE(old.value("videoSubtitleVerticalPositionFraction").toDouble(), 0.45);
        QCOMPARE(old.value("videoSubtitleEdgeStyle").toString(), QStringLiteral("softShadow"));
        QCOMPARE(old.value("videoSubtitleEdgeStrength").toDouble(), 0.7);
        QCOMPARE(old.value("videoSubtitleLookupHighlightTextColor").toString(),
                 QStringLiteral("#AABBCCDD"));
        QCOMPARE(mpv_color(old.value("videoSubtitleColor").toString()), QStringLiteral("#DDAABBCC"));
        QCOMPARE(video_settings({{"videoSubtitleVerticalPosition", -200}})
                     .value("videoSubtitleVerticalPositionFraction").toDouble(), 1);
        const auto modern = video_settings({{"videoSubtitleVerticalPosition", -200},
                                            {"videoSubtitleVerticalPositionFraction", 0.2},
                                            {"videoSubtitleEdgeStyle", "unknown"},
                                            {"videoSubtitleShadowRadius", 9}});
        QCOMPARE(modern.value("videoSubtitleVerticalPositionFraction").toDouble(), 0.2);
        QCOMPARE(modern.value("videoSubtitleEdgeStyle").toString(), QStringLiteral("highContrast"));
        QCOMPARE(modern.value("videoSubtitleEdgeStrength").toDouble(), 0.5);
        QCOMPARE(subtitle_edges(QStringLiteral("highContrast"), 0.5, 36).outline, 1.875);
        QCOMPARE(subtitle_edges(QStringLiteral("softShadow"), 1, 72).shadow, 8);
        QCOMPARE(subtitle_edges(QStringLiteral("off"), 1, 72).outline, 0);
    }

    void saveAndReset() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = std::filesystem::path(dir.path().toStdString());
        QVERIFY(niratan::storage::write_json_file(path / "settings.json",
                                                  QJsonDocument(QJsonObject{{"other", 42}})));
        QVERIFY(store_video_settings(path, {{"videoSeekInterval", 90}, {"videoSubtitleFontSize", 50}}));
        QVERIFY(store_video_settings(path, subtitle_appearance_defaults()));
        const auto loaded = load_video_settings(path);
        QCOMPARE(loaded.value("videoSeekInterval").toDouble(), 60);
        QCOMPARE(loaded.value("videoSubtitleFontSize").toDouble(), 36);
        QCOMPARE(niratan::storage::read_json_file(path / "settings.json").document.object()
                     .value("other").toInt(), 42);
        QVERIFY(!store_video_settings(path, {{"unrecognized", 12}}));
        QVERIFY(!store_video_settings(path, {{"videoSeekInterval", "wrong type"}}));
        QCOMPARE(load_video_settings(path).value("videoSeekInterval").toDouble(), 60);
    }

    void refusesNonObjectStore() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = std::filesystem::path(dir.path().toStdString());
        QVERIFY(niratan::storage::write_json_file(path / "settings.json",
                                                  QJsonDocument::fromJson("[]")));
        QVERIFY(!store_video_settings(path, {{"videoSeekInterval", 10}}));
        QVERIFY(niratan::storage::read_json_file(path / "settings.json").document.isArray());
    }
};

QTEST_APPLESS_MAIN(VideoSettingsTest)
#include "tst_video_settings.moc"
