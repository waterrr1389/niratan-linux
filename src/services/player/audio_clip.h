// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cut the cue's padded range from the video's selected audio stream and
// encode it for the card. Follows Features/Video/Playback/
// VideoAudioClipExporter.swift (VideoAudioClipExporter.export, the end > start
// guard and VideoAudioClipExporterError.failed),
// Features/Video/Playback/HSMpvClient.mm
// (exportAudioFromURL:toURL:startTime:endTime:httpHeaders:audioTrackID:
// errorMessage: and its vid=no, aid, audio-channels=mono, oac=aac, of=mp4,
// oacopts=b=64k options), Core/AnkiAudioCompressor.swift
// (AnkiAudioCompressor.data, both formats and the clamped bitrate),
// Features/Video/VideoMiningMediaStore+Audio.swift (preparedAudioClip, the
// destination extension), Models/Anki.swift
// (AnkiAudioCompressionFormat.fileExtension) and
// Features/Video/VideoMiningCoordinator.swift (context, where every export
// failure becomes the card's audio error).
//
// One ffmpeg pass cuts and encodes the clip at
// <output_stem>.<format extension>, where the stem carries no extension:
//   -nostdin -y -i <source> -ss <start> -t <duration> [-map 0:<index>]
//   -vn -ac 1 -c:a <codec> -b:a <bitrate>k <output>
// The start is max(0, start), the duration is end - max(0, start), and both
// are printed with six decimals in the C locale. A non-negative
// audio_stream_index maps that ffmpeg stream, which is the video's mpv
// ff-index for the selected audio track; a negative index omits -map and lets
// ffmpeg pick the default audio stream, Swift's aid=auto. The bitrate is
// clamped to 32...192 kbps. AAC writes m4a with codec "aac", MP3 writes mp3
// with codec "libmp3lame".
//
// start removes any file already at the output path, runs ffmpeg and emits
// finished exactly once per start, possibly before start returns. A successful
// run emits the output path and an empty error, and requires a non-empty
// output file. An end not greater than start fails without starting ffmpeg. A
// missing ffmpeg executable, a process that fails to start, a non-zero exit or
// crash, and a missing or empty output file all remove the output and emit an
// empty path with "Unable to capture the subtitle audio clip." The same
// message replaces the executor's intermediate failure texts, which never
// reach the card.
//
// Callers run one export at a time and start the next only after finished.
// There is no cancel.
//
// Recorded differences:
//   - The bytes differ from Swift's: this one ffmpeg pass encodes mono audio
//     directly, where Swift exports mpv AAC to WAV and re-encodes with
//     AVFoundation AAC or SwiftLAME MP3 (Decision D7). Extension, mono, the
//     bitrate setting and the range match.
//   - The stream is selected with ffmpeg's -map and mpv's ff-index, where
//     Swift passes mpv's track id as aid.
//   - Only a local file path is exported; remote sources and HTTP headers are
//     out of flow 2 (Decision D9).

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "models/anki/media_compression.h"

class QProcess;

namespace niratan::player {

struct AudioClipRequest {
    QString source_path;
    // The video's mpv ff-index for the selected audio track; negative means
    // the default audio stream.
    int audio_stream_index = -1;
    double start = 0;
    double end = 0;
    anki::AudioCompressionFormat format = anki::AudioCompressionFormat::aac;
    int bitrate_kbps = 64;
    // Without an extension; the format's extension is appended.
    QString output_stem;

    bool operator==(const AudioClipRequest&) const = default;
};

QString audio_clip_output_path(const QString& output_stem, anki::AudioCompressionFormat format);

QStringList audio_clip_arguments(const AudioClipRequest& request);

class AudioClipExporter : public QObject {
    Q_OBJECT

   public:
    // An empty ffmpeg path means "ffmpeg", looked up on PATH at each start.
    explicit AudioClipExporter(QString ffmpeg = {}, QObject* parent = nullptr);
    ~AudioClipExporter() override;

    void start(AudioClipRequest request);

   signals:
    void finished(const QString& path, const QString& error);

   private:
    void complete(const QString& path, const QString& error);
    void fail();

    QString ffmpeg_;
    QProcess* process_ = nullptr;
    QString output_path_;
    bool completed_ = false;
};

}  // namespace niratan::player
