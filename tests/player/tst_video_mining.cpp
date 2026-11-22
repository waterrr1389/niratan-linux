// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the video mining values, following script/test_video_media_mining.swift
// (the deterministic names, their quality and bitrate tokens, the audio clip
// range), script/test_video_screenshot_compression.swift (the JPEG extension,
// the AVIF tokens, the delayed range and the direct-media claim/wait/finish
// cases), script/test_video_mining_selection_resolution.swift (the adjacent
// cues of the no-context branch) and the media-ready wait of
// script/test_video_mining_io_coordination.swift.

#include "services/player/video_mining.h"
#include "services/player/video_mining_store.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

using namespace niratan::player;
namespace anki = niratan::anki;

namespace {

bool write_file(const std::string& path, const QByteArray& bytes) {
    QFile file(QString::fromStdString(path));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(bytes) == bytes.size();
}

QByteArray read_file(const std::string& path) {
    QFile file(QString::fromStdString(path));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

bool file_exists(const std::string& path) {
    return QFileInfo::exists(QString::fromStdString(path));
}

bool is_ascii(const std::string& value) {
    return std::ranges::all_of(value, [](unsigned char c) { return c < 0x80; });
}

MediaFilenameRequest base_request() {
    MediaFilenameRequest request;
    request.identity_key = "/Users/me/Show Episode 1.mkv";
    request.cue_start = 12.345;
    request.cue_end = 15.678;
    request.audio_start = 12.265;
    request.audio_end = 15.758;
    request.screenshot_format = VideoScreenshotFormat::png;
    return request;
}

QString screenshot(const MediaFilenameRequest& request) {
    return QString::fromStdString(deterministic_media_filenames(request).screenshot);
}

QString audio_clip(const MediaFilenameRequest& request) {
    return QString::fromStdString(deterministic_media_filenames(request).audio_clip);
}

QList<SubtitleCue> neighbour_cues() {
    return {
        SubtitleCue{"a", 1, 2, QStringLiteral("前。")},
        SubtitleCue{"b", 3, 4, QStringLiteral("現在。")},
        SubtitleCue{"c", 5, 6, QStringLiteral("次。")},
        SubtitleCue{"d", 7, 8, QStringLiteral("次の次。")},
    };
}

}  // namespace

class VideoMiningTest : public QObject {
    Q_OBJECT

   private slots:
    void audioClipRange_padsAndClamps();
    void deterministicNames_hashAndRanges();
    void deterministicNames_millisecondsAndTokens();
    void deterministicNames_avifTokensAndRange();
    void screenshotFormat_fallsBackToJpeg();
    void miningSelection_takesTheClickedCueAndNeighbours();
    void miningSelection_duplicateAndAbsentIds();
    void videoMiningContext_carriesSelectionAndCapture();
    void videoMiningContext_reportsRequestedCaptureFailures();
    void directMediaStore_claimWaitAndFinish();
    void directMediaStore_replacesOnlyAnAbsentDestination();
};

void VideoMiningTest::audioClipRange_padsAndClamps() {
    const auto padded = audio_clip_range(0.05, 4.0, 0.2, 4.25);
    QVERIFY(padded);
    QVERIFY(std::abs(padded->start - 0.13) < 1e-9);
    QVERIFY(std::abs(padded->end - 4.25) < 1e-9);
    QVERIFY(!audio_clip_range(2, 1, 0, 10));
    const auto unknown_duration = audio_clip_range(1, 2, 0, 0);
    QVERIFY(unknown_duration);
    QVERIFY(std::abs(unknown_duration->start - 0.88) < 1e-9);
    QVERIFY(std::abs(unknown_duration->end - 2.12) < 1e-9);
    const auto at_zero = audio_clip_range(0, 1, 0, 0);
    QVERIFY(at_zero);
    QVERIFY(std::abs(at_zero->start) < 1e-9);
    QVERIFY(std::abs(at_zero->end - 1.12) < 1e-9);
    QVERIFY(!audio_clip_range(0, 0, 0, 0, 0));
}

void VideoMiningTest::deterministicNames_hashAndRanges() {
    const auto request = base_request();
    const MediaFilenames names = deterministic_media_filenames(request);
    QCOMPARE(QString::fromStdString(names.screenshot),
             QStringLiteral("hoshi_video_frame_6c7af43fe8d969c7758b8849f26bee002041cb06_"
                            "12345-15678.png"));
    QCOMPARE(QString::fromStdString(names.audio_clip),
             QStringLiteral("hoshi_video_audio_6c7af43fe8d969c7758b8849f26bee002041cb06_"
                            "12265-15758.m4a"));
    QCOMPARE(deterministic_media_filenames(base_request()), names);

    auto compressed = request;
    compressed.screenshot_format = VideoScreenshotFormat::jpeg;
    compressed.screenshot_quality = 0.55;
    compressed.audio_format = anki::AudioCompressionFormat::mp3;
    compressed.audio_bitrate_kbps = 96;
    QCOMPARE(screenshot(compressed),
             QStringLiteral("hoshi_video_frame_6c7af43fe8d969c7758b8849f26bee002041cb06_"
                            "12345-15678_q55.jpg"));
    QCOMPARE(audio_clip(compressed),
             QStringLiteral("hoshi_video_audio_6c7af43fe8d969c7758b8849f26bee002041cb06_"
                            "12265-15758_96k.mp3"));

    auto japanese = request;
    japanese.identity_key = "/media/星を見ています Episode 1.mkv";
    japanese.cue_start = 0;
    japanese.cue_end = 0;
    japanese.audio_start = 0;
    japanese.audio_end = 0;
    QCOMPARE(screenshot(japanese),
             QStringLiteral("hoshi_video_frame_a9c4a1257e03a736bb404311d0783201a58d766f_"
                            "0-0.png"));
    QVERIFY(is_ascii(names.screenshot));
    QVERIFY(is_ascii(names.audio_clip));
    QVERIFY(is_ascii(deterministic_media_filenames(japanese).screenshot));
}

void VideoMiningTest::deterministicNames_millisecondsAndTokens() {
    auto negative = base_request();
    negative.cue_start = -1;
    negative.cue_end = 1;
    QVERIFY(screenshot(negative).contains(QStringLiteral("_0-1000.png")));

    auto rounded = base_request();
    rounded.cue_start = 12.3456;
    rounded.cue_end = 12.3456;
    QVERIFY(screenshot(rounded).contains(QStringLiteral("_12346-12346.png")));

    auto low = base_request();
    low.screenshot_format = VideoScreenshotFormat::jpeg;
    low.screenshot_quality = 0.20;
    QVERIFY(screenshot(low).endsWith(QStringLiteral("_12345-15678_q40.jpg")));

    auto high = base_request();
    high.screenshot_format = VideoScreenshotFormat::jpeg;
    high.screenshot_quality = 0.99;
    QVERIFY(screenshot(high).endsWith(QStringLiteral("_12345-15678_q95.jpg")));

    auto default_jpeg = base_request();
    default_jpeg.screenshot_format = VideoScreenshotFormat::jpeg;
    default_jpeg.screenshot_quality = 0.80;
    QVERIFY(screenshot(default_jpeg).endsWith(QStringLiteral("_12345-15678.jpg")));

    auto png_quality = base_request();
    png_quality.screenshot_quality = 0.55;
    QVERIFY(screenshot(png_quality).endsWith(QStringLiteral("_12345-15678.png")));

    auto low_bitrate = base_request();
    low_bitrate.audio_bitrate_kbps = 16;
    QVERIFY(audio_clip(low_bitrate).endsWith(QStringLiteral("_12265-15758_32k.m4a")));

    auto high_bitrate = base_request();
    high_bitrate.audio_bitrate_kbps = 400;
    QVERIFY(audio_clip(high_bitrate).endsWith(QStringLiteral("_12265-15758_192k.m4a")));

    auto default_bitrate = base_request();
    default_bitrate.audio_bitrate_kbps = 64;
    QVERIFY(audio_clip(default_bitrate).endsWith(QStringLiteral("_12265-15758.m4a")));
}

void VideoMiningTest::deterministicNames_avifTokensAndRange() {
    MediaFilenameRequest request;
    request.identity_key = "/tmp/episode.mkv";
    request.cue_start = 1;
    request.cue_end = 2;
    request.audio_start = 1;
    request.audio_end = 2;
    request.screenshot_format = VideoScreenshotFormat::avif;
    request.screenshot_quality = 0.75;
    QCOMPARE(screenshot(request),
             QStringLiteral("hoshi_video_frame_cf1720df1b7ecab2047247fc0e88d23abd5afc9e_"
                            "1000-2000_avif5_q75.avif"));
    QCOMPARE(audio_clip(request),
             QStringLiteral("hoshi_video_audio_cf1720df1b7ecab2047247fc0e88d23abd5afc9e_"
                            "1000-2000.m4a"));

    auto default_quality = request;
    default_quality.screenshot_quality = 0.80;
    QVERIFY(screenshot(default_quality).endsWith(QStringLiteral("_1000-2000_avif5.avif")));

    auto compact = request;
    compact.animated_avif_maximum_height = 240;
    compact.animated_avif_frames_per_second = 6;
    QVERIFY(screenshot(compact).endsWith(QStringLiteral("_1000-2000_avif5_q75_h240_fps6.avif")));

    auto low_height = request;
    low_height.animated_avif_maximum_height = 50;
    QVERIFY(screenshot(low_height).endsWith(QStringLiteral("_avif5_q75_h100.avif")));
    auto high_height = request;
    high_height.animated_avif_maximum_height = 2000;
    QVERIFY(screenshot(high_height).endsWith(QStringLiteral("_avif5_q75_h1080.avif")));
    auto default_height = request;
    default_height.animated_avif_maximum_height = 350;
    QVERIFY(screenshot(default_height).endsWith(QStringLiteral("_1000-2000_avif5_q75.avif")));

    auto low_fps = request;
    low_fps.animated_avif_frames_per_second = 0;
    QVERIFY(screenshot(low_fps).endsWith(QStringLiteral("_avif5_q75_fps1.avif")));
    auto high_fps = request;
    high_fps.animated_avif_frames_per_second = 99;
    QVERIFY(screenshot(high_fps).endsWith(QStringLiteral("_avif5_q75_fps30.avif")));
    auto default_fps = request;
    default_fps.animated_avif_frames_per_second = 10;
    QVERIFY(screenshot(default_fps).endsWith(QStringLiteral("_1000-2000_avif5_q75.avif")));

    auto delayed = request;
    delayed.screenshot_start = 1.25;
    delayed.screenshot_end = 2.25;
    QVERIFY(screenshot(delayed).contains(QStringLiteral("_1250-2250_avif5_q75.avif")));
    QVERIFY(audio_clip(delayed).contains(QStringLiteral("_1000-2000.m4a")));
    auto start_only = request;
    start_only.screenshot_start = 1.25;
    QVERIFY(screenshot(start_only).contains(QStringLiteral("_1250-2000_avif5_q75.avif")));
    auto end_only = request;
    end_only.screenshot_end = 2.25;
    QVERIFY(screenshot(end_only).contains(QStringLiteral("_1000-2250_avif5_q75.avif")));

    QCOMPARE(clamped_avif_maximum_height(std::nullopt), 350);
    QCOMPARE(clamped_avif_frames_per_second(std::nullopt), 10);
}

void VideoMiningTest::screenshotFormat_fallsBackToJpeg() {
    QCOMPARE(screenshot_format(false, anki::ImageCompressionFormat::jpeg),
             VideoScreenshotFormat::png);
    QCOMPARE(screenshot_format(false, anki::ImageCompressionFormat::avif),
             VideoScreenshotFormat::png);
    QCOMPARE(screenshot_format(true, anki::ImageCompressionFormat::jpeg),
             VideoScreenshotFormat::jpeg);
    QCOMPARE(screenshot_format(true, anki::ImageCompressionFormat::avif),
             VideoScreenshotFormat::jpeg);
}

void VideoMiningTest::miningSelection_takesTheClickedCueAndNeighbours() {
    const auto cues = neighbour_cues();
    const auto current = resolve_mining_selection(cues[1], cues);
    QCOMPARE(current.sentence, QStringLiteral("現在。"));
    QCOMPARE(current.cue_text, QStringLiteral("現在。"));
    QCOMPARE(current.cue_start, 3.0);
    QCOMPARE(current.cue_end, 4.0);
    QCOMPARE(current.previous_cue_text, std::optional<QString>(QStringLiteral("前。")));
    QCOMPARE(current.next_cue_text, std::optional<QString>(QStringLiteral("次。")));

    const auto first = resolve_mining_selection(cues[0], cues);
    QVERIFY(!first.previous_cue_text);
    QCOMPARE(first.next_cue_text, std::optional<QString>(QStringLiteral("現在。")));

    const auto last = resolve_mining_selection(cues[3], cues);
    QCOMPARE(last.previous_cue_text, std::optional<QString>(QStringLiteral("次。")));
    QVERIFY(!last.next_cue_text);

    const SubtitleCue absent{"x", 9, 10, QStringLiteral("外。")};
    const auto outside = resolve_mining_selection(absent, cues);
    QCOMPARE(outside.sentence, QStringLiteral("外。"));
    QCOMPARE(outside.cue_text, QStringLiteral("外。"));
    QCOMPARE(outside.cue_start, 9.0);
    QCOMPARE(outside.cue_end, 10.0);
    QVERIFY(!outside.previous_cue_text);
    QVERIFY(!outside.next_cue_text);

    const auto empty = resolve_mining_selection(absent, {});
    QVERIFY(!empty.previous_cue_text);
    QVERIFY(!empty.next_cue_text);
}

void VideoMiningTest::miningSelection_duplicateAndAbsentIds() {
    const QList<SubtitleCue> duplicates{
        SubtitleCue{"a", 1, 2, QStringLiteral("一")},
        SubtitleCue{"a", 3, 4, QStringLiteral("二")},
    };
    const auto duplicate = resolve_mining_selection(duplicates[1], duplicates);
    QCOMPARE(duplicate.sentence, QStringLiteral("二"));
    QCOMPARE(duplicate.cue_text, QStringLiteral("二"));
    QVERIFY(!duplicate.previous_cue_text);
    QCOMPARE(duplicate.next_cue_text, std::optional<QString>(QStringLiteral("二")));
}

void VideoMiningTest::videoMiningContext_carriesSelectionAndCapture() {
    const auto selection = resolve_mining_selection(neighbour_cues()[1], neighbour_cues());
    const auto context =
        build_video_mining_context("Episode Display Title", selection, VideoMediaCapture{});
    QCOMPARE(QString::fromStdString(context.sentence), QStringLiteral("現在。"));
    QCOMPARE(context.document_title,
             std::optional<std::string>(std::string("Episode Display Title")));
    QVERIFY(context.cover_url.empty());
    QVERIFY(context.cover_path.empty());
    QVERIFY(context.video);
    const auto& video = *context.video;
    QCOMPARE(QString::fromStdString(video.file_name), QStringLiteral("Episode Display Title"));
    QCOMPARE(QString::fromStdString(video.cue_text), QStringLiteral("現在。"));
    QCOMPARE(video.cue_start, 3.0);
    QCOMPARE(video.cue_end, 4.0);
    QCOMPARE(video.previous_cue_text, std::optional<std::string>(std::string("前。")));
    QCOMPARE(video.next_cue_text, std::optional<std::string>(std::string("次。")));
    QVERIFY(!video.screenshot_filename);
    QVERIFY(!video.audio_clip_filename);
    QVERIFY(!video.screenshot_path);
    QVERIFY(!video.audio_clip_path);
    QVERIFY(!video.screenshot_error_message);
    QVERIFY(!video.audio_clip_error_message);

    VideoMediaCapture capture;
    capture.screenshot_requested = true;
    capture.audio_clip_requested = true;
    capture.screenshot_filename = std::string("hoshi_video_frame_direct.png");
    capture.audio_clip_filename = std::string("hoshi_video_audio_direct.m4a");
    capture.screenshot_path = std::string("/tmp/frame.png");
    capture.audio_clip_path = std::string("/tmp/clip.m4a");
    const auto captured = build_video_mining_context("Title", selection, capture);
    QVERIFY(captured.video);
    QCOMPARE(captured.video->screenshot_filename,
             std::optional<std::string>(std::string("hoshi_video_frame_direct.png")));
    QCOMPARE(captured.video->audio_clip_filename,
             std::optional<std::string>(std::string("hoshi_video_audio_direct.m4a")));
    QCOMPARE(captured.video->screenshot_path,
             std::optional<std::string>(std::string("/tmp/frame.png")));
    QCOMPARE(captured.video->audio_clip_path,
             std::optional<std::string>(std::string("/tmp/clip.m4a")));
    QVERIFY(!captured.video->screenshot_error_message);
    QVERIFY(!captured.video->audio_clip_error_message);
}

void VideoMiningTest::videoMiningContext_reportsRequestedCaptureFailures() {
    const MiningSelection selection;
    VideoMediaCapture both;
    both.screenshot_requested = true;
    both.audio_clip_requested = true;
    const auto failed = build_video_mining_context("Title", selection, both);
    QVERIFY(failed.video);
    QCOMPARE(failed.video->screenshot_error_message,
             std::optional<std::string>(std::string("Unable to capture the video screenshot.")));
    QCOMPARE(failed.video->audio_clip_error_message,
             std::optional<std::string>(std::string("Unable to capture the subtitle audio clip.")));

    const auto unrequested = build_video_mining_context("Title", selection, VideoMediaCapture{});
    QVERIFY(unrequested.video);
    QVERIFY(!unrequested.video->screenshot_error_message);
    QVERIFY(!unrequested.video->audio_clip_error_message);
    QVERIFY(!unrequested.video->screenshot_filename);
    QVERIFY(!unrequested.video->audio_clip_filename);

    VideoMediaCapture path_only;
    path_only.screenshot_requested = true;
    path_only.audio_clip_requested = true;
    path_only.screenshot_path = std::string("/tmp/frame.png");
    path_only.audio_clip_path = std::string("/tmp/clip.m4a");
    const auto captured = build_video_mining_context("Title", selection, path_only);
    QVERIFY(captured.video);
    QVERIFY(!captured.video->screenshot_error_message);
    QVERIFY(!captured.video->audio_clip_error_message);

    VideoMediaCapture audio_only;
    audio_only.audio_clip_requested = true;
    const auto invalid_range = build_video_mining_context("Title", selection, audio_only);
    QVERIFY(invalid_range.video);
    QVERIFY(!invalid_range.video->screenshot_error_message);
    QCOMPARE(invalid_range.video->audio_clip_error_message,
             std::optional<std::string>(std::string("Unable to capture the subtitle audio clip.")));
}

void VideoMiningTest::directMediaStore_claimWaitAndFinish() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DirectMediaStore store(directory.path().toStdString());
    QCOMPARE(QString::fromStdString(store.path_for("clip.m4a")),
             directory.filePath(QStringLiteral("clip.m4a")));

    const auto owned = store.path_for("owned.bin");
    QVERIFY(store.claim(owned));
    QVERIFY(!store.claim(owned));
    std::optional<bool> waited;
    store.wait_for(owned, [&waited](bool ready) { waited = ready; });
    QVERIFY(!waited);
    QVERIFY(write_file(owned, "ready"));
    store.finish(owned, true);
    QCOMPARE(waited, std::optional<bool>(true));
    QVERIFY(!store.claim(owned));

    const auto failed = store.path_for("failed.bin");
    QVERIFY(store.claim(failed));
    std::optional<bool> failed_wait;
    store.wait_for(failed, [&failed_wait](bool ready) { failed_wait = ready; });
    store.finish(failed, false);
    QCOMPARE(failed_wait, std::optional<bool>(false));
    QVERIFY(store.claim(failed));
    store.finish(failed, false);

    const auto existing = store.path_for("existing.bin");
    QVERIFY(write_file(existing, "ready"));
    std::optional<bool> inline_ready;
    store.wait_for(existing, [&inline_ready](bool ready) { inline_ready = ready; });
    QCOMPARE(inline_ready, std::optional<bool>(true));

    const auto absent = store.path_for("absent.bin");
    std::optional<bool> inline_absent;
    store.wait_for(absent, [&inline_absent](bool ready) { inline_absent = ready; });
    QCOMPARE(inline_absent, std::optional<bool>(false));

    const auto never_written = store.path_for("never.bin");
    QVERIFY(store.claim(never_written));
    std::optional<bool> unfinished;
    store.wait_for(never_written, [&unfinished](bool ready) { unfinished = ready; });
    store.finish(never_written, true);
    QCOMPARE(unfinished, std::optional<bool>(false));
}

void VideoMiningTest::directMediaStore_replacesOnlyAnAbsentDestination() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DirectMediaStore store(directory.path().toStdString());
    const auto destination = store.path_for("destination.bin");
    const auto temp = store.path_for("temp.bin");

    QVERIFY(write_file(temp, "new"));
    QVERIFY(store.replace(temp, destination));
    QVERIFY(!file_exists(temp));
    QCOMPARE(read_file(destination), QByteArray("new"));

    const auto second_temp = store.path_for("temp2.bin");
    QVERIFY(write_file(second_temp, "newer"));
    QVERIFY(store.replace(second_temp, destination));
    QVERIFY(!file_exists(second_temp));
    QCOMPARE(read_file(destination), QByteArray("new"));

    const auto missing = store.path_for("missing.bin");
    const auto untouched = store.path_for("untouched.bin");
    QVERIFY(!store.replace(missing, untouched));
    QVERIFY(!file_exists(untouched));
    QVERIFY(write_file(untouched, "keep"));
    QVERIFY(!store.replace(missing, untouched));
    QCOMPARE(read_file(untouched), QByteArray("keep"));
    QVERIFY(!file_exists(missing));
}

QTEST_APPLESS_MAIN(VideoMiningTest)
#include "tst_video_mining.moc"
