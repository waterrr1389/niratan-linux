// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for TranscriptExtractor, following the outcomes of
// VideoSubtitleTrackExtractor.extract and the extraction task in
// VideoPlayerScreen+Subtitles.swift: a transcript or one error, nothing after
// cancellation, and the Linux SRT cache described in transcript_extractor.h.
// ffmpeg is replaced by a script that writes FAKE_OUTPUT to its last argument
// and exits with FAKE_EXIT. QProcess needs an event loop, hence GUILESS.

#include "services/player/transcript_extractor.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using niratan::player::read_subtitle_document;
using niratan::player::transcript_cache_file;
using niratan::player::TranscriptExtractor;

namespace {

const QByteArray kCue = "1\n00:00:01,000 --> 00:00:02,000\n日本語\n";

void write_file(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(bytes);
}

QString fake_ffmpeg(const QTemporaryDir& dir) {
    const QString path = dir.filePath(QStringLiteral("ffmpeg"));
    write_file(path, "#!/bin/sh\nfor last; do :; done\n"
                     "printf '%s' \"$FAKE_OUTPUT\" > \"$last\"\nexit ${FAKE_EXIT:-0}\n");
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

struct Fixture {
    QTemporaryDir dir;
    QString video = dir.filePath(QStringLiteral("Show 01.mkv"));
    QString cache = dir.filePath(QStringLiteral("cache"));

    Fixture() { write_file(video, "video"); }
};

}  // namespace

class TranscriptExtractorTest : public QObject {
    Q_OBJECT

   private slots:
    void init() {
        qputenv("FAKE_OUTPUT", kCue);
        qputenv("FAKE_EXIT", "0");
    }

    void cacheFile_isHashOfPathAndTrack() {
        const auto hash = QCryptographicHash::hash(QByteArray("/v/a.mkv"), QCryptographicHash::Sha256)
                              .toHex();
        QCOMPARE(transcript_cache_file(QStringLiteral("/c"), QStringLiteral("/v/a.mkv"), 3),
                 QStringLiteral("/c/%1-3.srt").arg(QString::fromLatin1(hash)));
    }

    void readDocument_boundsSize() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.srt"));
        write_file(path, "12345");
        QCOMPARE(read_subtitle_document(path, 5), std::optional<QByteArray>("12345"));
        QCOMPARE(read_subtitle_document(path, 4), std::nullopt);
        QCOMPARE(read_subtitle_document(dir.path()), std::nullopt);
        QCOMPARE(read_subtitle_document(dir.filePath(QStringLiteral("missing.srt"))), std::nullopt);
    }

    void extract_emitsTranscriptAndStampsCache() {
        Fixture fixture;
        TranscriptExtractor extractor(fixture.cache, fake_ffmpeg(fixture.dir));
        QSignalSpy finished(&extractor, &TranscriptExtractor::finished);
        QSignalSpy running(&extractor, &TranscriptExtractor::runningChanged);
        extractor.start(fixture.video, 3, 2);
        QVERIFY(extractor.running());
        QVERIFY(finished.wait(5000));
        QVERIFY(!extractor.running());
        QCOMPARE(running.count(), 2);
        QCOMPARE(finished.first().at(0).toByteArray(), kCue);
        QCOMPARE(finished.first().at(1).toString(), QString());
        const QString cache = transcript_cache_file(fixture.cache, fixture.video, 3);
        QCOMPARE(QFileInfo(cache).lastModified(), QFileInfo(fixture.video).lastModified());
        QCOMPARE(QDir(fixture.cache).entryList(QDir::Files).size(), 1);
    }

    void stampedCache_isReusedWithoutFfmpeg() {
        Fixture fixture;
        QDir().mkpath(fixture.cache);
        const QString cache = transcript_cache_file(fixture.cache, fixture.video, 3);
        write_file(cache, "cached");
        QFile stamp(cache);
        QVERIFY(stamp.open(QIODevice::ReadWrite));
        QVERIFY(stamp.setFileTime(QFileInfo(fixture.video).lastModified(),
                                  QFileDevice::FileModificationTime));
        stamp.close();
        TranscriptExtractor extractor(fixture.cache, fixture.dir.filePath(QStringLiteral("none")));
        QSignalSpy finished(&extractor, &TranscriptExtractor::finished);
        extractor.start(fixture.video, 3, 2);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toByteArray(), QByteArray("cached"));
        QVERIFY(!extractor.running());
    }

    void ffmpegFailure_reportsError() {
        Fixture fixture;
        qputenv("FAKE_EXIT", "1");
        TranscriptExtractor extractor(fixture.cache, fake_ffmpeg(fixture.dir));
        QSignalSpy finished(&extractor, &TranscriptExtractor::finished);
        extractor.start(fixture.video, 3, 2);
        QVERIFY(finished.wait(5000));
        QVERIFY(finished.first().at(0).toByteArray().isEmpty());
        QCOMPARE(finished.first().at(1).toString(),
                 QStringLiteral("ffmpeg could not extract this subtitle track as text."));
    }

    void emptyOutput_reportsNoCues() {
        Fixture fixture;
        qputenv("FAKE_OUTPUT", "");
        TranscriptExtractor extractor(fixture.cache, fake_ffmpeg(fixture.dir));
        QSignalSpy finished(&extractor, &TranscriptExtractor::finished);
        extractor.start(fixture.video, 3, 2);
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.first().at(1).toString(),
                 QStringLiteral("No readable cues were found in the embedded subtitle track."));
    }

    void missingProgram_reportsStartFailure() {
        Fixture fixture;
        TranscriptExtractor extractor(fixture.cache, fixture.dir.filePath(QStringLiteral("none")));
        QSignalSpy finished(&extractor, &TranscriptExtractor::finished);
        extractor.start(fixture.video, 3, 2);
        QVERIFY(finished.count() == 1 || finished.wait(5000));
        QCOMPARE(finished.first().at(1).toString(),
                 QStringLiteral("Could not start ffmpeg to extract the transcript."));
        QVERIFY(!extractor.running());
    }

    void noCacheDirectory_reportsError() {
        Fixture fixture;
        TranscriptExtractor extractor(QString{}, fake_ffmpeg(fixture.dir));
        QSignalSpy finished(&extractor, &TranscriptExtractor::finished);
        extractor.start(fixture.video, 3, 2);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(1).toString(),
                 QStringLiteral("Could not create the embedded transcript cache."));
    }

    void cancel_stopsWithoutResult() {
        Fixture fixture;
        TranscriptExtractor extractor(fixture.cache, fake_ffmpeg(fixture.dir));
        QSignalSpy finished(&extractor, &TranscriptExtractor::finished);
        QSignalSpy running(&extractor, &TranscriptExtractor::runningChanged);
        extractor.start(fixture.video, 3, 2);
        extractor.cancel();
        QVERIFY(!extractor.running());
        QCOMPARE(running.count(), 2);
        QVERIFY(!finished.wait(500));
    }
};

QTEST_GUILESS_MAIN(TranscriptExtractorTest)
#include "tst_transcript_extractor.moc"
