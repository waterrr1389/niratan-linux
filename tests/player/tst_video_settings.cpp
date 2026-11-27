// SPDX-License-Identifier: GPL-3.0-or-later
//
// Core/UserConfig.swift (video defaults and reset), VideoSubtitlePositionPolicy,
// VideoSubtitleEdgePreferenceResolver, VideoSubtitleEdgeRecipe and ColorHexCodec.
// VideoPlayerScreen.studySidebarWidth and VideoMiningHistorySidebar's bounds.

#include "services/player/video_settings.h"
#include "services/storage/json_file.h"

#include <QTemporaryDir>
#include <QTest>

#include <limits>

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

    // B1: VideoShaderPreset raw values; absent, null or unknown reads off.
    void shaderPresetKey() {
        QCOMPARE(video_settings().value("videoShaderPreset").toString(), QStringLiteral("off"));
        QCOMPARE(video_settings({{"videoShaderPreset", QJsonValue(QJsonValue::Null)}})
                     .value("videoShaderPreset")
                     .toString(),
                 QStringLiteral("off"));
        QCOMPARE(video_settings({{"videoShaderPreset", "unknown"}})
                     .value("videoShaderPreset")
                     .toString(),
                 QStringLiteral("off"));
        QCOMPARE(video_settings({{"videoShaderPreset", 1}}).value("videoShaderPreset").toString(),
                 QStringLiteral("off"));
        QCOMPARE(video_settings({{"videoShaderPreset", "anime4KFast"}})
                     .value("videoShaderPreset")
                     .toString(),
                 QStringLiteral("anime4KFast"));
        QCOMPARE(video_settings({{"videoShaderPreset", "anime4KHighQuality"}})
                     .value("videoShaderPreset")
                     .toString(),
                 QStringLiteral("anime4KHighQuality"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = std::filesystem::path(dir.path().toStdString());
        QVERIFY(store_video_settings(path, {{"videoShaderPreset", "anime4KHighQuality"}}));
        QCOMPARE(load_video_settings(path).value("videoShaderPreset").toString(),
                 QStringLiteral("anime4KHighQuality"));
        QCOMPARE(niratan::storage::read_json_file(path / "settings.json")
                     .document.object()
                     .value("videoShaderPreset")
                     .toString(),
                 QStringLiteral("anime4KHighQuality"));
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

    // B6: videoAutoPlayNext is the durable boolean key. A missing or non-bool
    // stored value falls back to true, an explicit false survives a reload.
    void autoPlayNextKey() {
        QVERIFY(video_settings().value("videoAutoPlayNext").toBool());
        QVERIFY(
            !video_settings({{"videoAutoPlayNext", false}}).value("videoAutoPlayNext").toBool());
        QVERIFY(video_settings({{"videoAutoPlayNext", "no"}}).value("videoAutoPlayNext").toBool());
        QVERIFY(video_settings({{"videoAutoPlayNext", 0}}).value("videoAutoPlayNext").toBool());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = std::filesystem::path(dir.path().toStdString());
        QVERIFY(store_video_settings(path, {{"videoAutoPlayNext", false}}));
        QVERIFY(!load_video_settings(path).value("videoAutoPlayNext").toBool());
        QVERIFY(store_video_settings(path, {{"videoAutoPlayNext", true}}));
        QVERIFY(load_video_settings(path).value("videoAutoPlayNext").toBool());
    }

    // V4 B5: the subtitle gap fast-forward keys keep Swift's defaults and
    // bounds; storage does not round the configured speed.
    void subtitleGapFastForwardKeys() {
        const auto defaults = video_settings();
        QVERIFY(!defaults.value("videoSubtitleGapFastForwardEnabled").toBool());
        QCOMPARE(defaults.value("videoSubtitleGapFastForwardSpeed").toDouble(), 2.7);
        QVERIFY(video_settings({{"videoSubtitleGapFastForwardEnabled", true}})
                    .value("videoSubtitleGapFastForwardEnabled")
                    .toBool());
        QVERIFY(!video_settings({{"videoSubtitleGapFastForwardEnabled", "on"}})
                     .value("videoSubtitleGapFastForwardEnabled")
                     .toBool());
        QCOMPARE(video_settings({{"videoSubtitleGapFastForwardSpeed", 0.5}})
                     .value("videoSubtitleGapFastForwardSpeed")
                     .toDouble(),
                 1.1);
        QCOMPARE(video_settings({{"videoSubtitleGapFastForwardSpeed", 9.0}})
                     .value("videoSubtitleGapFastForwardSpeed")
                     .toDouble(),
                 5.0);
        QCOMPARE(video_settings({{"videoSubtitleGapFastForwardSpeed", 2.73}})
                     .value("videoSubtitleGapFastForwardSpeed")
                     .toDouble(),
                 2.73);
        QCOMPARE(video_settings({{"videoSubtitleGapFastForwardSpeed",
                                  std::numeric_limits<double>::infinity()}})
                     .value("videoSubtitleGapFastForwardSpeed")
                     .toDouble(),
                 2.7);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = std::filesystem::path(dir.path().toStdString());
        QVERIFY(store_video_settings(path,
                                     {{"videoSubtitleGapFastForwardEnabled", true},
                                      {"videoSubtitleGapFastForwardSpeed", 2.73}}));
        const auto stored = load_video_settings(path);
        QVERIFY(stored.value("videoSubtitleGapFastForwardEnabled").toBool());
        QCOMPARE(stored.value("videoSubtitleGapFastForwardSpeed").toDouble(), 2.73);
    }

    void studySidebarWidth() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = std::filesystem::path(dir.path().toStdString());
        QCOMPARE(load_video_settings(path).value("studySidebarWidth").toInt(), 340);
        QCOMPARE(video_settings({{"studySidebarWidth", 200}}).value("studySidebarWidth").toInt(), 320);
        QCOMPARE(video_settings({{"studySidebarWidth", 700}}).value("studySidebarWidth").toInt(), 560);
        QVERIFY(store_video_settings(path, {{"studySidebarWidth", 480}}));
        QVERIFY(store_video_settings(path, subtitle_appearance_defaults()));
        QCOMPARE(load_video_settings(path).value("studySidebarWidth").toInt(), 480);
    }

    void videoLibraryKeys() {
        const auto defaults = video_settings();
        QCOMPARE(defaults.value("videoLibraryLayoutMode").toString(), QStringLiteral("posters"));
        QCOMPARE(defaults.value("videoLibraryPosterWidth").toDouble(), 160);
        QCOMPARE(
            video_settings({{"videoLibraryLayoutMode", "list"}, {"videoLibraryPosterWidth", 300}})
                .value("videoLibraryPosterWidth")
                .toDouble(),
            240);
        QCOMPARE(video_settings({{"videoLibraryLayoutMode", "grid"}})
                     .value("videoLibraryLayoutMode")
                     .toString(),
                 QStringLiteral("posters"));
        QCOMPARE(video_settings({{"videoLibraryPosterWidth", 20}})
                     .value("videoLibraryPosterWidth")
                     .toDouble(),
                 110);
        QCOMPARE(video_settings({{"videoLibraryPosterWidth", 999}})
                     .value("videoLibraryPosterWidth")
                     .toDouble(),
                 240);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = std::filesystem::path(dir.path().toStdString());
        QVERIFY(niratan::storage::write_json_file(path / "settings.json",
                                                  QJsonDocument(QJsonObject{{"other", 42}})));
        QVERIFY(store_video_settings(
            path, {{"videoLibraryLayoutMode", "list"}, {"videoLibraryPosterWidth", 300}}));
        const auto stored =
            niratan::storage::read_json_file(path / "settings.json").document.object();
        QCOMPARE(stored.value("videoLibraryLayoutMode").toString(), QStringLiteral("list"));
        QCOMPARE(stored.value("videoLibraryPosterWidth").toDouble(), 240);
        QCOMPARE(stored.value("other").toInt(), 42);
        QCOMPARE(load_video_settings(path).value("videoLibraryLayoutMode").toString(),
                 QStringLiteral("list"));
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
