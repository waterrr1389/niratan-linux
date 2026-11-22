// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the video audio clip arguments and exporter, following
// script/test_anki_audio_compression.swift (both formats, a non-empty output,
// the extension per format) and script/test_video_mining_io_coordination.swift
// (the export receives the resolved range). ffmpeg is replaced by a script
// that writes FAKE_OUTPUT to its last argument, records its arguments in
// ARGS_FILE one per line and exits with FAKE_EXIT; WRITE_OUTPUT=0 skips the
// file so a missing output can be tested.

#include "services/player/audio_clip.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace niratan::player;
namespace anki = niratan::anki;

namespace {

constexpr auto clip_error = "Unable to capture the subtitle audio clip.";

bool write_file(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(bytes) == bytes.size();
}

QString fake_ffmpeg(const QTemporaryDir& dir) {
    const QString path = dir.filePath(QStringLiteral("ffmpeg"));
    write_file(path,
               "#!/bin/sh\n"
               "for last; do :; done\n"
               "printf '%s\\n' \"$@\" > \"$ARGS_FILE\"\n"
               "if [ \"${WRITE_OUTPUT:-1}\" = 1 ]; then\n"
               "    printf '%s' \"$FAKE_OUTPUT\" > \"$last\"\n"
               "fi\n"
               "exit ${FAKE_EXIT:-0}\n");
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

QString value_after(const QStringList& arguments, const QString& flag) {
    const auto index = arguments.indexOf(flag);
    if (index < 0 || index + 1 >= arguments.size()) {
        return {};
    }
    return arguments.at(index + 1);
}

struct Fixture {
    QTemporaryDir dir;
    QString source = dir.filePath(QStringLiteral("Show 01.mkv"));
    QString args_file = dir.filePath(QStringLiteral("args.txt"));
    QString stem = dir.filePath(QStringLiteral("hoshi-video-abc"));

    Fixture() {
        write_file(source, "video");
        qputenv("ARGS_FILE", args_file.toUtf8());
    }

    AudioClipRequest request() const {
        AudioClipRequest value;
        value.source_path = source;
        value.audio_stream_index = 2;
        value.start = 0.13;
        value.end = 4.25;
        value.format = anki::AudioCompressionFormat::aac;
        value.bitrate_kbps = 64;
        value.output_stem = stem;
        return value;
    }
};

QStringList recorded_arguments(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    if (!lines.isEmpty() && lines.last().isEmpty()) {
        lines.removeLast();
    }
    return lines;
}

}  // namespace

class AudioClipTest : public QObject {
    Q_OBJECT

    QByteArray path_;

   private slots:
    void init() {
        path_ = qgetenv("PATH");
        qputenv("FAKE_OUTPUT", "clip-bytes");
        qputenv("FAKE_EXIT", "0");
        qunsetenv("WRITE_OUTPUT");
    }

    void cleanup() { qputenv("PATH", path_); }

    void arguments_encodeAacWithTheSelectedStream();
    void arguments_encodeMp3WithoutAStreamMapping();
    void arguments_formatTheClampedRangeAndBitrate();
    void outputPath_appendsTheFormatExtension();
    void export_writesTheClipAndReportsIt();
    void export_failures_data();
    void export_failures();
};

void AudioClipTest::arguments_encodeAacWithTheSelectedStream() {
    AudioClipRequest request;
    request.source_path = QStringLiteral("/videos/Show 01.mkv");
    request.audio_stream_index = 2;
    request.start = 0.13;
    request.end = 4.25;
    request.format = anki::AudioCompressionFormat::aac;
    request.bitrate_kbps = 64;
    request.output_stem = QStringLiteral("/tmp/HoshiVideoMining/hoshi-video-abc");
    QCOMPARE(audio_clip_arguments(request),
             (QStringList{"-nostdin", "-y", "-i", "/videos/Show 01.mkv", "-ss", "0.130000", "-t",
                          "4.120000", "-map", "0:2", "-vn", "-ac", "1", "-c:a", "aac", "-b:a",
                          "64k", "/tmp/HoshiVideoMining/hoshi-video-abc.m4a"}));
}

void AudioClipTest::arguments_encodeMp3WithoutAStreamMapping() {
    AudioClipRequest request;
    request.source_path = QStringLiteral("/videos/Show 01.mkv");
    request.audio_stream_index = -1;
    request.start = 0.13;
    request.end = 4.25;
    request.format = anki::AudioCompressionFormat::mp3;
    request.bitrate_kbps = 96;
    request.output_stem = QStringLiteral("/tmp/HoshiVideoMining/hoshi-video-abc");
    QCOMPARE(audio_clip_arguments(request),
             (QStringList{"-nostdin", "-y", "-i", "/videos/Show 01.mkv", "-ss", "0.130000", "-t",
                          "4.120000", "-vn", "-ac", "1", "-c:a", "libmp3lame", "-b:a", "96k",
                          "/tmp/HoshiVideoMining/hoshi-video-abc.mp3"}));
}

void AudioClipTest::arguments_formatTheClampedRangeAndBitrate() {
    AudioClipRequest request;
    request.source_path = QStringLiteral("/videos/Show 01.mkv");
    request.output_stem = QStringLiteral("/tmp/clip");
    request.audio_stream_index = 0;
    request.start = -0.5;
    request.end = 1;
    QCOMPARE(value_after(audio_clip_arguments(request), QStringLiteral("-ss")),
             QStringLiteral("0.000000"));
    QCOMPARE(value_after(audio_clip_arguments(request), QStringLiteral("-t")),
             QStringLiteral("1.000000"));

    request.start = 12.3456789;
    request.end = 15.6789;
    QCOMPARE(value_after(audio_clip_arguments(request), QStringLiteral("-ss")),
             QStringLiteral("12.345679"));
    QCOMPARE(value_after(audio_clip_arguments(request), QStringLiteral("-t")),
             QStringLiteral("3.333221"));

    request.bitrate_kbps = 16;
    QCOMPARE(value_after(audio_clip_arguments(request), QStringLiteral("-b:a")),
             QStringLiteral("32k"));
    request.bitrate_kbps = 400;
    QCOMPARE(value_after(audio_clip_arguments(request), QStringLiteral("-b:a")),
             QStringLiteral("192k"));
    request.bitrate_kbps = 64;
    QCOMPARE(value_after(audio_clip_arguments(request), QStringLiteral("-b:a")),
             QStringLiteral("64k"));
}

void AudioClipTest::outputPath_appendsTheFormatExtension() {
    QCOMPARE(audio_clip_output_path(QStringLiteral("/tmp/hoshi-video-abc"),
                                    anki::AudioCompressionFormat::aac),
             QStringLiteral("/tmp/hoshi-video-abc.m4a"));
    QCOMPARE(audio_clip_output_path(QStringLiteral("/tmp/hoshi-video-abc"),
                                    anki::AudioCompressionFormat::mp3),
             QStringLiteral("/tmp/hoshi-video-abc.mp3"));
}

void AudioClipTest::export_writesTheClipAndReportsIt() {
    Fixture fixture;
    const auto request = fixture.request();
    const QString output = audio_clip_output_path(request.output_stem, request.format);
    AudioClipExporter exporter(fake_ffmpeg(fixture.dir));
    QSignalSpy finished(&exporter, &AudioClipExporter::finished);
    exporter.start(request);
    QVERIFY(finished.count() == 1 || finished.wait(5000));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().at(0).toString(), output);
    QCOMPARE(finished.first().at(1).toString(), QString());
    QFile file(output);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("clip-bytes"));
    QCOMPARE(recorded_arguments(fixture.args_file), audio_clip_arguments(request));
}

void AudioClipTest::export_failures_data() {
    QTest::addColumn<QString>("failure");
    QTest::newRow("invalid-range") << "invalid-range";
    QTest::newRow("missing-program") << "missing-program";
    QTest::newRow("missing-ffmpeg") << "missing-ffmpeg";
    QTest::newRow("exit-nonzero") << "exit-nonzero";
    QTest::newRow("empty-output") << "empty-output";
    QTest::newRow("missing-output") << "missing-output";
}

void AudioClipTest::export_failures() {
    QFETCH(QString, failure);
    Fixture fixture;
    auto request = fixture.request();
    if (failure == "invalid-range") {
        request.end = request.start;
    } else if (failure == "exit-nonzero") {
        qputenv("FAKE_EXIT", "2");
    } else if (failure == "empty-output") {
        qputenv("FAKE_OUTPUT", "");
    } else if (failure == "missing-output") {
        qputenv("WRITE_OUTPUT", "0");
    } else if (failure == "missing-ffmpeg") {
        qputenv("PATH", QByteArray());
    }
    const QString output = audio_clip_output_path(request.output_stem, request.format);
    const QString program = failure == "missing-program"
        ? fixture.dir.filePath(QStringLiteral("missing-ffmpeg"))
        : failure == "missing-ffmpeg" ? QString()
                                      : fake_ffmpeg(fixture.dir);
    AudioClipExporter exporter(program);
    QSignalSpy finished(&exporter, &AudioClipExporter::finished);
    exporter.start(request);
    QVERIFY(finished.count() == 1 || finished.wait(5000));
    QTest::qWait(50);
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().at(0).toString(), QString());
    QCOMPARE(finished.first().at(1).toString(), QString::fromUtf8(clip_error));
    QVERIFY(!QFileInfo::exists(output));
    if (failure == "invalid-range") {
        QVERIFY(!QFileInfo::exists(fixture.args_file));
    }
}

QTEST_GUILESS_MAIN(AudioClipTest)
#include "tst_audio_clip.moc"
