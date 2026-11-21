// SPDX-License-Identifier: GPL-3.0-or-later
// VideoPlaybackHistoryStore.swift: VideoPlaybackResumeOptions and VideoAudioTrackIdentity;
// script/test_video_playback_model.swift: per-video options restoration.

#include "services/player/resume_options.h"

#include <QTest>

namespace niratan::player {
namespace {

const AudioTrackIdentity kJapanese{2, 8, QStringLiteral("Japanese Audio"),
                                  QStringLiteral("jpn"), QStringLiteral("aac")};

class ResumeOptionsTest : public QObject {
    Q_OBJECT

   private slots:
    void capture_omitsDefaultsAndNormalizesChanges() {
        const auto off = capture_resume_options(1, 0.004, -0.004, {{kJapanese, false}});
        QCOMPARE(off, (ResumeOptions{{}, {}, {}, AudioOff{}}));
        QCOMPARE(capture_resume_options(1.61, 100, -100, {{kJapanese, true}}),
                 (ResumeOptions{1.6, 60, -30, kJapanese}));
        QVERIFY(capture_resume_options(1, 0, 0, {}).empty());
    }

    void restore_normalizesDecodedValuesWithoutOmittingDefaults() {
        QCOMPARE(normalize_resume_options({0.24, 0, 42, {}}),
                 (ResumeOptions{0.25, 0, 30, {}}));
    }

    void match_prefersStreamIndexOrTrackIdAndThenUniqueMetadata() {
        auto current = kJapanese;
        current.track_id = 9;
        const AudioTrackIdentity id_collision{2, 7, QStringLiteral("English"), {}, {}};
        QCOMPARE(matching_audio_track_id(kJapanese, {{id_collision, true}, {current, false}}),
                 std::optional<qint64>(9));
        current.ff_index = 10;
        QCOMPARE(matching_audio_track_id(kJapanese, {{id_collision, true}, {current, false}}),
                 std::optional<qint64>(9));
        auto legacy = kJapanese;
        legacy.ff_index.reset();
        QCOMPARE(matching_audio_track_id(legacy, {{id_collision, true}, {current, false}}),
                 std::optional<qint64>(2));
        auto duplicate = current;
        duplicate.track_id = 10;
        QCOMPARE(matching_audio_track_id(kJapanese, {{current, true}, {duplicate, false}}),
                 std::nullopt);
    }

    void codable_matchesSwiftEnumAndOptionalKeys() {
        const ResumeOptions selected{1.6, 0.45, -0.35, kJapanese};
        const QJsonObject identity{{"trackID", 2}, {"ffIndex", 8}, {"title", "Japanese Audio"},
                                   {"language", "jpn"}, {"codec", "aac"}};
        const QJsonObject embedded{{"embedded", QJsonObject{{"_0", identity}}}};
        QCOMPARE(encode_resume_options(selected),
                 (QJsonObject{{"speed", 1.6}, {"subtitleDelay", 0.45}, {"audioDelay", -0.35},
                              {"audioSelection", embedded}}));
        QCOMPARE(decode_resume_options(encode_resume_options(selected)), std::optional(selected));
        const QJsonObject off{{"audioSelection", QJsonObject{{"off", QJsonObject{}}}}};
        const ResumeOptions disabled{{}, {}, {}, AudioOff{}};
        QCOMPARE(decode_resume_options(off), std::optional(disabled));
        QCOMPARE(encode_resume_options(disabled), off);
        QCOMPARE(decode_resume_options({{"audioSelection", QJsonObject{{"embedded", identity}}}}),
                 std::nullopt);
    }
};

}  // namespace
}  // namespace niratan::player

QTEST_APPLESS_MAIN(niratan::player::ResumeOptionsTest)
#include "tst_resume_options.moc"
