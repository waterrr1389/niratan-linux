// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the cue-audio range, cache key and ffmpeg arguments, following
// Features/Sasayaki/SasayakiPlayer.swift (cueSentenceAudio, expandCue and
// miningAudioCache), Core/AnkiAudioCompressor.swift (data) and
// script/test_anki_audio_compression.swift (both formats, a non-empty output
// and the extension per format).
//
// ffmpeg is replaced by a script that writes FAKE_OUTPUT to its last argument,
// records its arguments in ARGS_FILE one per line and exits with FAKE_EXIT;
// WRITE_OUTPUT=0 skips the file so a missing output can be tested.

#include "services/sasayaki/cue_audio.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <string>
#include <vector>

using namespace niratan::sasayaki;
namespace anki = niratan::anki;

namespace {

std::vector<SasayakiMatch> base_matches() {
    return {
        {"a", 1.0, 1.5, "こんにちは", 0, 0, 0},
        {"b", 2.0, 2.5, "世界", 0, 0, 0},
        {"c", 3.0, 3.5, "です", 0, 0, 0},
        {"d", 0.5, 1.0, "世界", 1, 0, 0},
    };
}

QString write_script(const QString& directory, const QByteArray& body) {
    const QString path = QDir(directory).filePath(QStringLiteral("ffmpeg"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(body) != body.size()) {
        return {};
    }
    file.close();
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

QString fake_ffmpeg(const QString& directory) {
    return write_script(directory,
                        "#!/bin/sh\n"
                        "for last; do :; done\n"
                        "printf '%s\\n' \"$@\" > \"$ARGS_FILE\"\n"
                        "if [ \"${WRITE_OUTPUT:-1}\" = \"1\" ]; then\n"
                        "    printf '%s' \"$FAKE_OUTPUT\" > \"$last\"\n"
                        "fi\n"
                        "exit ${FAKE_EXIT:-0}\n");
}

QString order_fake_ffmpeg(const QString& directory) {
    return write_script(directory,
                        "#!/bin/sh\n"
                        "for last; do :; done\n"
                        "printf '%s\\n' \"$@\" > \"$ARGS_FILE\"\n"
                        "previous=\n"
                        "for argument in \"$@\"; do\n"
                        "    if [ \"$previous\" = \"-ss\" ]; then\n"
                        "        case \"$argument\" in\n"
                        "            0.100000) printf 'first' > \"$last\" ;;\n"
                        "            0.200000) printf 'second' > \"$last\" ;;\n"
                        "        esac\n"
                        "    fi\n"
                        "    previous=\"$argument\"\n"
                        "done\n"
                        "exit 0\n");
}

QByteArray read_file(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

CueAudioRequest clip_request(const QString& stem, double start, double end, double rate = 1.0,
                             bool mp3 = false, int bitrate = 64) {
    CueAudioRequest request;
    request.source_path = QStringLiteral("input.wav");
    request.start = start;
    request.end = end;
    request.rate = rate;
    request.format = mp3 ? anki::AudioCompressionFormat::mp3 : anki::AudioCompressionFormat::aac;
    request.bitrate_kbps = bitrate;
    request.output_stem = stem;
    return request;
}

}  // namespace

class CueAudioTest : public QObject {
    Q_OBJECT

   private slots:
    void init() {
        qputenv("FAKE_OUTPUT", "abc");
        qputenv("WRITE_OUTPUT", "1");
        qputenv("FAKE_EXIT", "0");
    }

    void expand_cue_data();
    void expand_cue();
    void clip_range_data();
    void clip_range();
    void cache_key_data();
    void cache_key();
    void arguments_data();
    void arguments();

    void exporter_answers_the_file_bytes();
    void exporter_failures_data();
    void exporter_failures();
    void exporter_caches_a_successful_export();
    void a_failed_export_is_not_cached();
    void exporter_answers_in_order();
};

void CueAudioTest::expand_cue_data() {
    QTest::addColumn<int>("variant");
    QTest::addColumn<QString>("sentence");
    QTest::addColumn<QString>("cueId");
    QTest::addColumn<int>("cueChapter");
    QTest::addColumn<double>("cueStart");
    QTest::addColumn<double>("cueEnd");
    QTest::addColumn<double>("expectedStart");
    QTest::addColumn<double>("expectedEnd");

    QTest::newRow("widens backwards and forwards")
        << 0 << QString::fromUtf8("こんにちは世界です") << "b" << 0 << 2.0 << 2.5 << 1.0 << 3.5;
    QTest::newRow("widens forwards only")
        << 0 << QString::fromUtf8("世界です") << "b" << 0 << 2.0 << 2.5 << 2.0 << 3.5;
    QTest::newRow("widens backwards only")
        << 0 << QString::fromUtf8("こんにちは世界") << "b" << 0 << 2.0 << 2.5 << 1.0 << 2.5;
    QTest::newRow("the cue alone")
        << 0 << QString::fromUtf8("世界") << "b" << 0 << 2.0 << 2.5 << 2.0 << 2.5;
    QTest::newRow("stops before a neighbour it does not contain")
        << 0 << QString::fromUtf8("こんにちは世界") << "a" << 0 << 1.0 << 1.5 << 1.0 << 2.5;
    QTest::newRow("stays inside the cue's chapter")
        << 0 << QString::fromUtf8("世界です") << "d" << 1 << 0.5 << 1.0 << 0.5 << 1.0;
    QTest::newRow("a cue missing from the list keeps its times")
        << 0 << QString::fromUtf8("世界") << "z" << 0 << 9.0 << 9.5 << 9.0 << 9.5;
    QTest::newRow("no match data keeps the cue's times")
        << 1 << QString::fromUtf8("世界") << "b" << 0 << 2.0 << 2.5 << 2.0 << 2.5;
    QTest::newRow("a neighbour's markup is filtered")
        << 2 << QString::fromUtf8("こんにちは世界") << "a" << 0 << 1.0 << 1.5 << 1.0 << 2.5;
    QTest::newRow("the sentence is filtered")
        << 0 << QString::fromUtf8("世、界") << "c" << 0 << 3.0 << 3.5 << 2.0 << 3.5;
}

void CueAudioTest::expand_cue() {
    QFETCH(int, variant);
    QFETCH(QString, sentence);
    QFETCH(QString, cueId);
    QFETCH(int, cueChapter);
    QFETCH(double, cueStart);
    QFETCH(double, cueEnd);
    QFETCH(double, expectedStart);
    QFETCH(double, expectedEnd);

    std::vector<SasayakiMatch> matches;
    if (variant != 1) {
        matches = base_matches();
    }
    if (variant == 2) {
        matches.at(1).text = "<ruby>世界<rt>せかい</rt></ruby>";
    }
    const SasayakiMatch cue{cueId.toStdString(), cueStart, cueEnd, "", cueChapter, 0, 0};
    const CueAudioRange range = niratan::sasayaki::expand_cue(matches, cue, sentence.toStdString());
    QCOMPARE(range.start, expectedStart);
    QCOMPARE(range.end, expectedEnd);
}

void CueAudioTest::clip_range_data() {
    QTest::addColumn<double>("start");
    QTest::addColumn<double>("end");
    QTest::addColumn<double>("delay");
    QTest::addColumn<double>("expectedStart");
    QTest::addColumn<double>("expectedEnd");

    QTest::newRow("positive delay") << 1.2 << 3.4 << 0.5 << 1.7 << 3.9;
    QTest::newRow("start clamps to zero") << 0.2 << 1.0 << -0.5 << 0.0 << 0.5;
    QTest::newRow("end follows the clamped start") << 1.0 << 0.5 << 0.0 << 1.0 << 1.0;
    QTest::newRow("negative delay") << 5.0 << 6.0 << -0.75 << 4.25 << 5.25;
}

void CueAudioTest::clip_range() {
    QFETCH(double, start);
    QFETCH(double, end);
    QFETCH(double, delay);
    QFETCH(double, expectedStart);
    QFETCH(double, expectedEnd);

    const CueAudioRange range = cue_clip_range({start, end}, delay);
    QCOMPARE(range.start, expectedStart);
    QCOMPARE(range.end, expectedEnd);
}

void CueAudioTest::cache_key_data() {
    QTest::addColumn<QString>("source");
    QTest::addColumn<double>("start");
    QTest::addColumn<double>("end");
    QTest::addColumn<bool>("mp3");
    QTest::addColumn<int>("bitrate");
    QTest::addColumn<double>("rate");
    QTest::addColumn<QString>("expected");

    QTest::newRow("aac with rate") << "/audio/a.m4a" << 1.2 << 2.4 << false << 64 << 1.25
                                   << QStringLiteral("/audio/a.m4a\n1200\n2400\naac\n64\n125");
    QTest::newRow("mp3 at unit rate") << "/audio/a.m4a" << 0.0 << 3.0 << true << 192 << 1.0
                                      << QStringLiteral("/audio/a.m4a\n0\n3000\nmp3\n192\n100");
}

void CueAudioTest::cache_key() {
    QFETCH(QString, source);
    QFETCH(double, start);
    QFETCH(double, end);
    QFETCH(bool, mp3);
    QFETCH(int, bitrate);
    QFETCH(double, rate);
    QFETCH(QString, expected);

    CueAudioRequest request;
    request.source_path = source;
    request.start = start;
    request.end = end;
    request.format = mp3 ? anki::AudioCompressionFormat::mp3 : anki::AudioCompressionFormat::aac;
    request.bitrate_kbps = bitrate;
    request.rate = rate;
    QCOMPARE(QString::fromStdString(cue_audio_cache_key(request)), expected);
}

void CueAudioTest::arguments_data() {
    QTest::addColumn<double>("start");
    QTest::addColumn<double>("end");
    QTest::addColumn<double>("rate");
    QTest::addColumn<bool>("mp3");
    QTest::addColumn<int>("bitrate");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("aac at unit rate")
        << 1.2 << 2.4 << 1.0 << false << 64
        << QStringList{"-nostdin", "-y",       "-i",  "/audio/a.m4a", "-ss", "1.200000",
                       "-t",       "1.200000", "-vn", "-ac",          "1",   "-c:a",
                       "aac",      "-b:a",     "64k", "/tmp/c.m4a"};
    QTest::newRow("mp3 with a tempo")
        << 1.2 << 2.4 << 1.5 << true << 32
        << QStringList{"-nostdin", "-y",       "-i",         "/audio/a.m4a", "-ss", "1.200000",
                       "-t",       "1.200000", "-filter:a",  "atempo=1.5",   "-vn", "-ac",
                       "1",        "-c:a",     "libmp3lame", "-b:a",         "32k", "/tmp/c.mp3"};
    QTest::newRow("above two chains tempo and clamps the bitrate")
        << 1.2 << 2.4 << 2.5 << false << 500
        << QStringList{"-nostdin", "-y",   "-i",        "/audio/a.m4a", "-ss",
                       "1.200000", "-t",   "1.200000",  "-filter:a",    "atempo=2,atempo=1.25",
                       "-vn",      "-ac",  "1",         "-c:a",         "aac",
                       "-b:a",     "192k", "/tmp/c.m4a"};
    QTest::newRow("slower tempo") << 1.2 << 2.4 << 0.5 << false << 64
                                  << QStringList{"-nostdin",  "-y",         "-i",  "/audio/a.m4a",
                                                 "-ss",       "1.200000",   "-t",  "1.200000",
                                                 "-filter:a", "atempo=0.5", "-vn", "-ac",
                                                 "1",         "-c:a",       "aac", "-b:a",
                                                 "64k",       "/tmp/c.m4a"};
    QTest::newRow("a tiny rate difference adds no filter")
        << 1.2 << 2.4 << 1.0005 << false << 64
        << QStringList{"-nostdin", "-y",       "-i",  "/audio/a.m4a", "-ss", "1.200000",
                       "-t",       "1.200000", "-vn", "-ac",          "1",   "-c:a",
                       "aac",      "-b:a",     "64k", "/tmp/c.m4a"};
    QTest::newRow("a negative start clamps to zero")
        << -0.5 << 1.0 << 1.0 << false << 64
        << QStringList{"-nostdin", "-y",       "-i",  "/audio/a.m4a", "-ss", "0.000000",
                       "-t",       "1.000000", "-vn", "-ac",          "1",   "-c:a",
                       "aac",      "-b:a",     "64k", "/tmp/c.m4a"};
    QTest::newRow("the lower bitrate clamp")
        << 1.2 << 2.4 << 1.0 << false << 10
        << QStringList{"-nostdin", "-y",       "-i",  "/audio/a.m4a", "-ss", "1.200000",
                       "-t",       "1.200000", "-vn", "-ac",          "1",   "-c:a",
                       "aac",      "-b:a",     "32k", "/tmp/c.m4a"};
}

void CueAudioTest::arguments() {
    QFETCH(double, start);
    QFETCH(double, end);
    QFETCH(double, rate);
    QFETCH(bool, mp3);
    QFETCH(int, bitrate);
    QFETCH(QStringList, expected);

    CueAudioRequest request;
    request.source_path = QStringLiteral("/audio/a.m4a");
    request.start = start;
    request.end = end;
    request.rate = rate;
    request.format = mp3 ? anki::AudioCompressionFormat::mp3 : anki::AudioCompressionFormat::aac;
    request.bitrate_kbps = bitrate;
    request.output_stem = QStringLiteral("/tmp/c");
    QCOMPARE(cue_audio_arguments(request), expected);
}

void CueAudioTest::exporter_answers_the_file_bytes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString args = directory.filePath(QStringLiteral("args.txt"));
    qputenv("ARGS_FILE", args.toUtf8());
    CueAudioExporter exporter(fake_ffmpeg(directory.path()));
    QSignalSpy finished(&exporter, &CueAudioExporter::finished);
    const CueAudioRequest request =
        clip_request(directory.filePath(QStringLiteral("clip")), 1.2, 2.4);
    exporter.start(request);

    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
    QCOMPARE(finished.at(0).at(0).toByteArray(), QByteArray("abc"));
    QVERIFY(!QFileInfo::exists(directory.filePath(QStringLiteral("clip.m4a"))));
}

void CueAudioTest::exporter_failures_data() {
    QTest::addColumn<QString>("output");
    QTest::addColumn<QString>("write_output");
    QTest::addColumn<QString>("exit_code");
    QTest::addColumn<bool>("equal_range");

    QTest::newRow("non-zero exit") << "abc" << "1" << "1" << false;
    QTest::newRow("no output file") << "abc" << "0" << "0" << false;
    QTest::newRow("empty output") << "" << "1" << "0" << false;
    QTest::newRow("empty range") << "abc" << "1" << "0" << true;
}

void CueAudioTest::exporter_failures() {
    QFETCH(QString, output);
    QFETCH(QString, write_output);
    QFETCH(QString, exit_code);
    QFETCH(bool, equal_range);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString args = directory.filePath(QStringLiteral("args.txt"));
    qputenv("ARGS_FILE", args.toUtf8());
    qputenv("FAKE_OUTPUT", output.toUtf8());
    qputenv("WRITE_OUTPUT", write_output.toUtf8());
    qputenv("FAKE_EXIT", exit_code.toUtf8());
    CueAudioExporter exporter(fake_ffmpeg(directory.path()));
    QSignalSpy finished(&exporter, &CueAudioExporter::finished);
    CueAudioRequest request = clip_request(directory.filePath(QStringLiteral("clip")), 1.2, 2.4);
    if (equal_range) {
        request.end = request.start;
    }
    exporter.start(request);

    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
    QCOMPARE(finished.at(0).at(0).toByteArray(), QByteArray());
    QVERIFY(!QFileInfo::exists(directory.filePath(QStringLiteral("clip.m4a"))));
    if (equal_range) {
        QVERIFY(!QFileInfo::exists(args));
    }
}

void CueAudioTest::exporter_caches_a_successful_export() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString args = directory.filePath(QStringLiteral("args.txt"));
    qputenv("ARGS_FILE", args.toUtf8());
    CueAudioExporter exporter(fake_ffmpeg(directory.path()));
    QSignalSpy finished(&exporter, &CueAudioExporter::finished);
    const CueAudioRequest request =
        clip_request(directory.filePath(QStringLiteral("clip")), 1.2, 2.4);

    exporter.start(request);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
    const QByteArray recorded = read_file(args);
    const QDateTime modified = QFileInfo(args).lastModified();
    QVERIFY(!recorded.isEmpty());

    exporter.start(request);
    QCOMPARE(finished.size(), 2);
    QCOMPARE(finished.at(1).at(0).toByteArray(), QByteArray("abc"));
    QTest::qWait(100);
    QCOMPARE(read_file(args), recorded);
    QCOMPARE(QFileInfo(args).lastModified(), modified);

    QVERIFY(QFile::remove(args));
    exporter.clear_cache();
    exporter.start(request);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 3, 5000);
    QVERIFY(QFileInfo::exists(args));
}

void CueAudioTest::a_failed_export_is_not_cached() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("ARGS_FILE", directory.filePath(QStringLiteral("args.txt")).toUtf8());
    qputenv("FAKE_EXIT", "1");
    CueAudioExporter exporter(fake_ffmpeg(directory.path()));
    QSignalSpy finished(&exporter, &CueAudioExporter::finished);
    const CueAudioRequest request =
        clip_request(directory.filePath(QStringLiteral("clip")), 1.2, 2.4);

    exporter.start(request);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
    QCOMPARE(finished.at(0).at(0).toByteArray(), QByteArray());

    qputenv("FAKE_EXIT", "0");
    exporter.start(request);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 5000);
    QCOMPARE(finished.at(1).at(0).toByteArray(), QByteArray("abc"));
}

void CueAudioTest::exporter_answers_in_order() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString args = directory.filePath(QStringLiteral("args.txt"));
    qputenv("ARGS_FILE", args.toUtf8());
    CueAudioExporter exporter(order_fake_ffmpeg(directory.path()));
    QSignalSpy finished(&exporter, &CueAudioExporter::finished);

    exporter.start(clip_request(directory.filePath(QStringLiteral("first")), 0.1, 0.4));
    exporter.start(clip_request(directory.filePath(QStringLiteral("second")), 0.2, 0.6));

    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 5000);
    QCOMPARE(finished.at(0).at(0).toByteArray(), QByteArray("first"));
    QCOMPARE(finished.at(1).at(0).toByteArray(), QByteArray("second"));
}

QTEST_GUILESS_MAIN(CueAudioTest)
#include "tst_cue_audio.moc"
