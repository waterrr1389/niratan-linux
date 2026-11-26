// SPDX-License-Identifier: GPL-3.0-or-later
//
// First-open subtitle defaults, following Models/Subtitle.swift
// (VideoSubtitleAutoloadCandidate.bestCandidate, matchesLanguageSuffixedSidecar,
// compareCandidateOrder), Features/Video/Playback/VideoTrack.swift
// (VideoEmbeddedSubtitleDefault.preferredTrack, isJapanese, rank),
// Features/Video/VideoPlaybackHistoryStore.swift
// (VideoSubtitleRestoreResolver.resolve),
// Features/Video/VideoPlayerScreen+Subtitles.swift
// (restoreRememberedSubtitleSelectionOrAutoload, autoloadSubtitleIfAvailable,
// applyEmbeddedSubtitleDefaultIfReady, loadPrimarySubtitle,
// selectSubtitleTrack, applySubtitlesOff),
// Features/Video/Playback/HSMpvClient.mm
// (existingExternalSubtitleTrackIDForURL:), script/test_video_subtitles.swift
// and script/test_video_embedded_subtitle_default.swift.

#include "services/player/subtitle_default.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using niratan::player::best_subtitle_sidecar;
using niratan::player::EmbeddedSubtitleActivation;
using niratan::player::EmbeddedSubtitleDefault;
using niratan::player::existing_external_subtitle_track_id;
using niratan::player::is_japanese_subtitle_track;
using niratan::player::plan_subtitle_first_open;
using niratan::player::preferred_embedded_subtitle_track;
using niratan::player::primary_subtitle_selection_path;
using niratan::player::resolve_subtitle_restore;
using niratan::player::SubtitleRestoreKind;
using niratan::player::SubtitleRestoreResolution;
using niratan::player::SubtitleSelection;
using niratan::player::SubtitleTrackIdentity;
using niratan::player::VideoTrack;
using niratan::player::VideoTrackType;

namespace {

QUrl media(const QDir& dir, const QString& name) {
    return QUrl::fromLocalFile(dir.filePath(name));
}

bool write_file(const QDir& dir, const QString& name, const QByteArray& bytes = {}) {
    QFile file(dir.filePath(name));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

VideoTrack track(int id, const QString& title, VideoTrackType type = VideoTrackType::subtitle,
                 std::optional<QString> language = std::nullopt,
                 std::optional<QString> external = std::nullopt, bool image = false,
                 bool selected = false) {
    return VideoTrack{id,           type,     title, language, QStringLiteral("ass"),
                      std::nullopt, external, image, selected};
}

QString absolute(const QDir& dir, const QString& name) {
    return QFileInfo(dir.filePath(name)).absoluteFilePath();
}

std::optional<int> preferred_id(const QList<VideoTrack>& tracks) {
    const auto preferred = preferred_embedded_subtitle_track(tracks);
    if (!preferred) {
        return std::nullopt;
    }
    return preferred->id;
}

}  // namespace

class SubtitleDefaultTest : public QObject {
    Q_OBJECT

   private slots:
    void sidecar_exactStemWinsInExtensionOrder() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QDir dir(temporary.path());
        QVERIFY(write_file(dir, QStringLiteral("Episode 01.mkv")));
        for (const auto& extension : {QStringLiteral("srt"), QStringLiteral("vtt"),
                                      QStringLiteral("ass"), QStringLiteral("ssa")}) {
            QVERIFY(write_file(dir, QStringLiteral("Episode 01.") + extension));
        }
        QVERIFY(write_file(dir, QStringLiteral("Episode 01.ja.srt")));
        const QUrl episode = media(dir, QStringLiteral("Episode 01.mkv"));
        QCOMPARE(
            best_subtitle_sidecar(episode),
            std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode 01.srt")))));
        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("Episode 01.srt"))));
        QCOMPARE(
            best_subtitle_sidecar(episode),
            std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode 01.vtt")))));
        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("Episode 01.vtt"))));
        QCOMPARE(
            best_subtitle_sidecar(episode),
            std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode 01.ass")))));
        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("Episode 01.ass"))));
        QCOMPARE(
            best_subtitle_sidecar(episode),
            std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode 01.ssa")))));

        QVERIFY(write_file(dir, QStringLiteral("Episode 02.SRT")));
        QCOMPARE(best_subtitle_sidecar(media(dir, QStringLiteral("Episode 02.mkv"))), std::nullopt);
    }

    void sidecar_suffixedEntriesMatchAnySuffixWithoutHiddenNames() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QDir dir(temporary.path());
        QVERIFY(write_file(dir, QStringLiteral("Episode 01.mkv")));
        QVERIFY(write_file(dir, QStringLiteral("Episode 01.ja.SRT")));
        QVERIFY(write_file(dir, QStringLiteral("Episode 01 en.ass")));
        QVERIFY(write_file(dir, QStringLiteral("Episode 01x.srt")));
        QVERIFY(write_file(dir, QStringLiteral("Episode 01.txt")));
        const QUrl episode = media(dir, QStringLiteral("Episode 01.mkv"));
        QCOMPARE(
            best_subtitle_sidecar(episode),
            std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode 01.ja.SRT")))));

        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("Episode 01.ja.SRT"))));
        QCOMPARE(
            best_subtitle_sidecar(episode),
            std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode 01 en.ass")))));

        QVERIFY(write_file(dir, QStringLiteral("Episode 01.banana.srt")));
        QCOMPARE(best_subtitle_sidecar(episode),
                 std::optional(
                     QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode 01.banana.srt")))));

        const QDir hidden(temporary.filePath(QStringLiteral("hidden")));
        QVERIFY(QDir().mkpath(hidden.path()));
        QVERIFY(write_file(hidden, QStringLiteral(".Episode.mkv")));
        QVERIFY(write_file(hidden, QStringLiteral(".Episode.ja.srt")));
        QCOMPARE(best_subtitle_sidecar(media(hidden, QStringLiteral(".Episode.mkv"))),
                 std::nullopt);
    }

    void sidecar_extensionPriorityThenNaturalFilenameOrder() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QDir dir(temporary.path());
        QVERIFY(write_file(dir, QStringLiteral("Episode.mkv")));
        QVERIFY(write_file(dir, QStringLiteral("Episode 10.srt")));
        QVERIFY(write_file(dir, QStringLiteral("Episode 2.srt")));
        const QUrl episode = media(dir, QStringLiteral("Episode.mkv"));
        QCOMPARE(
            best_subtitle_sidecar(episode),
            std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode 2.srt")))));

        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("Episode 2.srt"))));
        QVERIFY(write_file(dir, QStringLiteral("Episode 2.ass")));
        QCOMPARE(
            best_subtitle_sidecar(episode),
            std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode 10.srt")))));
    }

    void sidecar_nonFileMissingDirectoryAndNoCandidatesYieldNone() {
        QCOMPARE(best_subtitle_sidecar(QUrl(QStringLiteral("https://example.com/Episode.mkv"))),
                 std::nullopt);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QCOMPARE(best_subtitle_sidecar(
                     media(QDir(temporary.path()), QStringLiteral("missing/Episode.mkv"))),
                 std::nullopt);
        const QDir dir(temporary.path());
        QVERIFY(write_file(dir, QStringLiteral("Episode.mkv")));
        QVERIFY(write_file(dir, QStringLiteral("Episode.srt")));
        QCOMPARE(best_subtitle_sidecar(media(dir, QStringLiteral("Episode.mkv"))),
                 std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode.srt")))));
        QVERIFY(QFile::remove(dir.filePath(QStringLiteral("Episode.srt"))));
        QVERIFY(write_file(dir, QStringLiteral("Other.srt")));
        QVERIFY(write_file(dir, QStringLiteral("Episode01.ja.srt")));
        QCOMPARE(best_subtitle_sidecar(media(dir, QStringLiteral("Episode.mkv"))), std::nullopt);

        const QDir blocked(temporary.filePath(QStringLiteral("blocked")));
        QVERIFY(QDir().mkpath(blocked.path()));
        QVERIFY(write_file(blocked, QStringLiteral("Episode.mkv")));
        QVERIFY(write_file(blocked, QStringLiteral("Episode.ja.srt")));
        QVERIFY(QFile::setPermissions(blocked.path(), QFileDevice::Permissions{}));
        const auto unreadable =
            best_subtitle_sidecar(media(blocked, QStringLiteral("Episode.mkv")));
        QVERIFY(QFile::setPermissions(blocked.path(),
                                      QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                          QFileDevice::ExeOwner));
        QCOMPARE(unreadable, std::nullopt);
    }

    void restore_resolvesRememberedChoiceBeforeDefaults() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QDir dir(temporary.path());
        QVERIFY(write_file(dir, QStringLiteral("external.srt")));
        const QString external = absolute(dir, QStringLiteral("external.srt"));

        QCOMPARE(resolve_subtitle_restore(std::monostate{}, {}, false),
                 (SubtitleRestoreResolution{SubtitleRestoreKind::off, {}, 0}));
        QCOMPARE(resolve_subtitle_restore(SubtitleSelection{external}, {}, false),
                 (SubtitleRestoreResolution{SubtitleRestoreKind::external,
                                            QUrl::fromLocalFile(external), 0}));
        QCOMPARE(
            resolve_subtitle_restore(
                SubtitleSelection{
                    QUrl::fromLocalFile(dir.filePath(QStringLiteral("gone.srt"))).toLocalFile()},
                {}, false)
                .kind,
            SubtitleRestoreKind::unavailable);

        const SubtitleTrackIdentity saved{2, std::optional<int>(5), QStringLiteral("Full"),
                                          QStringLiteral("jpn"), QStringLiteral("ass")};
        const QList<VideoTrack> loaded{
            track(9, QStringLiteral("Full"), VideoTrackType::subtitle, QStringLiteral("jpn"))};
        auto with_index = loaded;
        with_index[0].ff_index = 5;
        QCOMPARE(resolve_subtitle_restore(SubtitleSelection{saved}, with_index, false),
                 (SubtitleRestoreResolution{SubtitleRestoreKind::waiting_for_tracks, {}, 0}));
        QCOMPARE(resolve_subtitle_restore(SubtitleSelection{saved}, with_index, true),
                 (SubtitleRestoreResolution{SubtitleRestoreKind::embedded_track, {}, 9}));
        QCOMPARE(resolve_subtitle_restore(SubtitleSelection{saved}, loaded, true),
                 (SubtitleRestoreResolution{SubtitleRestoreKind::embedded_track, {}, 9}));
        auto other = loaded;
        other[0].title = QStringLiteral("Other");
        QCOMPARE(resolve_subtitle_restore(SubtitleSelection{saved}, other, true),
                 (SubtitleRestoreResolution{SubtitleRestoreKind::unavailable, {}, 0}));
        QCOMPARE(resolve_subtitle_restore(SubtitleSelection{saved}, {}, true),
                 (SubtitleRestoreResolution{SubtitleRestoreKind::waiting_for_tracks, {}, 0}));
    }

    void firstOpen_sidecarTakesExternalPathWithoutEmbeddedFallback() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QDir dir(temporary.path());
        QVERIFY(write_file(dir, QStringLiteral("Episode.mkv")));
        QVERIFY(write_file(dir, QStringLiteral("Episode.ja.srt"),
                           QByteArrayLiteral("not a subtitle document")));
        const auto plan = plan_subtitle_first_open(media(dir, QStringLiteral("Episode.mkv")));
        QCOMPARE(
            plan.sidecar,
            std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Episode.ja.srt")))));
        QVERIFY(!plan.wait_for_embedded_default);

        const auto missing = plan_subtitle_first_open(media(dir, QStringLiteral("Other.mkv")));
        QCOMPARE(missing.sidecar, std::nullopt);
        QVERIFY(missing.wait_for_embedded_default);

        const QUrl valid = QUrl::fromLocalFile(dir.filePath(QStringLiteral("Primary.srt")));
        QCOMPARE(primary_subtitle_selection_path(
                     valid, QByteArrayLiteral("1\n00:00:00,000 --> 00:00:01,000\n字幕\n")),
                 std::optional(QUrl::fromLocalFile(absolute(dir, QStringLiteral("Primary.srt")))));
        QCOMPARE(primary_subtitle_selection_path(
                     QUrl::fromLocalFile(dir.filePath(QStringLiteral("Broken.srt"))),
                     QByteArrayLiteral("not a subtitle document")),
                 std::nullopt);
        QCOMPARE(primary_subtitle_selection_path(
                     QUrl::fromLocalFile(dir.filePath(QStringLiteral("Primary.txt"))),
                     QByteArrayLiteral("1\n00:00:00,000 --> 00:00:01,000\n字幕\n")),
                 std::nullopt);
    }

    void existingExternalTrack_usesResolvedSymlinkPathAndSkipsEffectsAndNonSubtitles() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QDir dir(temporary.path());
        QVERIFY(write_file(dir, QStringLiteral("Episode.srt")));
        QVERIFY(write_file(dir, QStringLiteral("Other.srt")));
        const QString target = absolute(dir, QStringLiteral("Episode.srt"));
        const QString other = absolute(dir, QStringLiteral("Other.srt"));
        const QString link = dir.filePath(QStringLiteral("Sidecar.srt"));
        QVERIFY(QFile::link(target, link));

        const QList<VideoTrack> tracks{
            track(1, QStringLiteral("Audio"), VideoTrackType::audio, {}, target),
            track(2, QStringLiteral("__niratan_internal_ass_effects__"), VideoTrackType::subtitle,
                  {}, link),
            track(3, QStringLiteral("Other"), VideoTrackType::subtitle, {}, other),
            track(0, QStringLiteral("Invalid"), VideoTrackType::subtitle, {}, target),
            track(4, QStringLiteral("Sidecar"), VideoTrackType::subtitle, {}, target),
        };
        QCOMPARE(existing_external_subtitle_track_id(QUrl::fromLocalFile(link), tracks),
                 std::optional<int>(4));
        QCOMPARE(existing_external_subtitle_track_id(QUrl::fromLocalFile(target), tracks),
                 std::optional<int>(4));
        QCOMPARE(existing_external_subtitle_track_id(
                     QUrl::fromLocalFile(dir.filePath(QStringLiteral("None.srt"))), tracks),
                 std::nullopt);
    }

    void embeddedDefault_waitsForLoadedSnapshotAndConsumesOnce() {
        EmbeddedSubtitleDefault pending;
        pending.wait_for_tracks();
        const QList<VideoTrack> tracks{
            track(1, QStringLiteral("Full"), VideoTrackType::subtitle, QStringLiteral("jpn"))};
        QCOMPARE(pending.apply_if_ready(false, tracks, false), std::nullopt);
        QVERIFY(pending.awaiting());
        const auto activation = pending.apply_if_ready(true, tracks, false);
        QVERIFY(activation.has_value());
        QCOMPARE(activation->track_id, 1);
        QVERIFY(!pending.awaiting());
        QCOMPARE(pending.apply_if_ready(true, tracks, false), std::nullopt);

        pending.wait_for_tracks();
        QCOMPARE(pending.apply_if_ready(true, {}, false), std::nullopt);
        QVERIFY(!pending.awaiting());
    }

    void embeddedDefault_leavesExistingPrimaryOrSelectedSubtitleAlone() {
        EmbeddedSubtitleDefault pending;
        const QList<VideoTrack> tracks{
            track(1, QStringLiteral("Full"), VideoTrackType::subtitle, QStringLiteral("jpn")),
            track(2, QStringLiteral("Audio"), VideoTrackType::audio, QStringLiteral("jpn"))};
        pending.wait_for_tracks();
        QCOMPARE(pending.apply_if_ready(true, tracks, true), std::nullopt);
        QVERIFY(!pending.awaiting());

        pending.wait_for_tracks();
        auto selected = tracks;
        selected[0].is_selected = true;
        QCOMPARE(pending.apply_if_ready(true, selected, false), std::nullopt);
        QVERIFY(!pending.awaiting());

        pending.wait_for_tracks();
        auto audio_selected = tracks;
        audio_selected[1].is_selected = true;
        QCOMPARE(pending.apply_if_ready(true, audio_selected, false),
                 std::optional(EmbeddedSubtitleActivation{1, false, false, true}));
    }

    void embeddedDefault_isInvalidatedByNewMediaOrExplicitChoices() {
        EmbeddedSubtitleDefault pending;
        const QList<VideoTrack> tracks{
            track(1, QStringLiteral("Full"), VideoTrackType::subtitle, QStringLiteral("jpn"))};
        pending.wait_for_tracks();
        pending.invalidate();
        QVERIFY(!pending.awaiting());
        QCOMPARE(pending.apply_if_ready(true, tracks, false), std::nullopt);

        pending.wait_for_tracks();
        pending.invalidate();
        pending.wait_for_tracks();
        QVERIFY(pending.awaiting());
    }

    void preferredTrack_requiresJapaneseEmbeddedSubtitles() {
        using enum VideoTrackType;
        QCOMPARE(
            preferred_id({track(1, QStringLiteral("English"), subtitle, QStringLiteral("eng")),
                          track(2, QStringLiteral("日本語"), subtitle, QStringLiteral("jpn"))}),
            std::optional<int>(2));
        QCOMPARE(
            preferred_id({track(1, QStringLiteral("English"), subtitle, QStringLiteral("eng")),
                          track(2, QStringLiteral("Commentary"), subtitle, QStringLiteral("eng"))}),
            std::nullopt);
        QCOMPARE(
            preferred_id({track(1, QStringLiteral("Signs & Songs"), subtitle, QStringLiteral("ja")),
                          track(2, QStringLiteral("Full"), subtitle, QStringLiteral("ja"))}),
            std::optional<int>(2));
        QCOMPARE(
            preferred_id({track(1, QStringLiteral("Forced"), subtitle, QStringLiteral("JPN")),
                          track(2, QStringLiteral("Dialogue"), subtitle, QStringLiteral("jpn"))}),
            std::optional<int>(2));
        QCOMPARE(preferred_id({track(1, QStringLiteral("PGS"), subtitle, QStringLiteral("jpn"),
                                     std::nullopt, true),
                               track(2, QStringLiteral("Text"), subtitle, QStringLiteral("jpn"))}),
                 std::optional<int>(2));
        QCOMPARE(preferred_id({track(1, QStringLiteral("PGS"), subtitle, QStringLiteral("jpn"),
                                     std::nullopt, true)}),
                 std::optional<int>(1));
        QCOMPARE(preferred_id({track(1, QStringLiteral("日本語字幕")),
                               track(2, QStringLiteral("Untitled"))}),
                 std::optional<int>(1));
        QCOMPARE(preferred_id({track(1, QStringLiteral("Japanese (CC)"))}), std::optional<int>(1));
        QCOMPARE(
            preferred_id({track(1, QStringLiteral("Full"), subtitle, QStringLiteral("ja-JP"))}),
            std::optional<int>(1));
        QCOMPARE(preferred_id({track(1, QStringLiteral("Sidecar"), subtitle, QStringLiteral("jpn"),
                                     QStringLiteral("/tmp/a.ass"))}),
                 std::nullopt);
        QCOMPARE(preferred_id({track(1, QStringLiteral("Audio"), audio, QStringLiteral("jpn"))}),
                 std::nullopt);
        QCOMPARE(
            preferred_id({track(3, QStringLiteral("Dialogue A"), subtitle, QStringLiteral("jpn")),
                          track(4, QStringLiteral("Dialogue B"), subtitle, QStringLiteral("jpn"))}),
            std::optional<int>(3));
        QVERIFY(is_japanese_subtitle_track(
            track(1, QStringLiteral("番組"), subtitle, QStringLiteral("JA"))));
        QVERIFY(!is_japanese_subtitle_track(
            track(1, QStringLiteral("Full"), subtitle, QStringLiteral("japanese-ish"))));

        QCOMPARE(preferred_id({track(1, QStringLiteral("Full"), subtitle, QStringLiteral("JP"))}),
                 std::optional<int>(1));
        QCOMPARE(
            preferred_id({track(1, QStringLiteral("Full"), subtitle, QStringLiteral("JAPANESE"))}),
            std::optional<int>(1));
        QCOMPARE(preferred_id(
                     {track(1, QStringLiteral("jAPanEse (CC)"), subtitle, QStringLiteral("eng"))}),
                 std::optional<int>(1));
    }

    void preferredTrack_rankPenalizesPartialTitlesAndImages() {
        using enum VideoTrackType;
        QCOMPARE(
            preferred_id({track(1, QStringLiteral("Signs"), subtitle, QStringLiteral("jpn")),
                          track(2, QStringLiteral("Dialogue"), subtitle, QStringLiteral("jpn"))}),
            std::optional<int>(2));
        QCOMPARE(preferred_id({track(1, QStringLiteral("Dialogue"), subtitle, QStringLiteral("jpn"),
                                     std::nullopt, true),
                               track(2, QStringLiteral("Signs"), subtitle, QStringLiteral("jpn"))}),
                 std::optional<int>(1));
        QCOMPARE(
            preferred_id({track(1, QStringLiteral("看板"), subtitle, QStringLiteral("jpn")),
                          track(2, QStringLiteral("Dialogue"), subtitle, QStringLiteral("jpn"))}),
            std::optional<int>(2));
        QCOMPARE(
            preferred_id({track(1, QStringLiteral("Dialogue"), subtitle, QStringLiteral("jpn")),
                          track(2, QStringLiteral("歌詞"), subtitle, QStringLiteral("jpn"))}),
            std::optional<int>(1));
        for (const auto& title :
             {QStringLiteral("SONGS"), QStringLiteral("FORCED"), QStringLiteral("歌詞")}) {
            QCOMPARE(preferred_id(
                         {track(1, title, subtitle, QStringLiteral("jpn")),
                          track(2, QStringLiteral("Dialogue"), subtitle, QStringLiteral("jpn"))}),
                     std::optional<int>(2));
        }
    }

    void activation_usesOrdinarySelectionWithoutRememberingOrOsd() {
        EmbeddedSubtitleDefault pending;
        pending.wait_for_tracks();
        const QList<VideoTrack> tracks{
            track(7, QStringLiteral("Full"), VideoTrackType::subtitle, QStringLiteral("jpn"))};
        QCOMPARE(pending.apply_if_ready(true, tracks, false),
                 std::optional(EmbeddedSubtitleActivation{7, false, false, true}));
    }
};

QTEST_APPLESS_MAIN(SubtitleDefaultTest)
#include "tst_subtitle_default.moc"
