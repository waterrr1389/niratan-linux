// SPDX-License-Identifier: GPL-3.0-or-later
//
// Text for an embedded subtitle track, following VideoSubtitleTrackExtractor
// .extract in Features/Video/Subtitles/VideoSubtitleTrackExtractor.swift and
// the extraction task in VideoPlayerScreen+Subtitles.swift
// (synchronizeSelectedSubtitleTrack, cancelSubtitleTrackExtraction).
//
// Swift demuxes the stream in process through HSSubtitleTrackExtractor and
// keeps the packets for ASS reconstruction. Here ffmpeg converts the stream
// at mpv's ff-index to SRT, which keeps only the cue text. The result is
// cached under <data root>/Cache/video-subtitles/<sha256 of the video path>-
// <track id>.srt, stamped with the video's modification time; a cache file
// with the same time is reused without running ffmpeg. ffmpeg writes to a
// temporary file beside the cache file, so a partial output never appears
// under the cache name, and the output is discarded when the video changed
// while it ran.
//
// Subtitle documents, extracted or external, are read up to 16 MiB; a larger
// or unreadable file is reported instead of being loaded.

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>

#include <optional>

class QProcess;

namespace niratan::player {

inline constexpr qint64 kMaxSubtitleDocumentBytes = 16 * 1024 * 1024;

// The file's bytes, or none when it is not a readable regular file of at most
// limit bytes.
std::optional<QByteArray> read_subtitle_document(const QString& path,
                                                 qint64 limit = kMaxSubtitleDocumentBytes);

// <cache directory>/<sha256 hex of video_path>-<track_id>.srt
QString transcript_cache_file(const QString& cache_directory, const QString& video_path,
                              int track_id);

class TranscriptExtractor : public QObject {
    Q_OBJECT

   public:
    // <data root>/Cache/video-subtitles, empty without a data root.
    static QString default_cache_directory();

    // ffmpeg, when empty, is looked up on PATH at each start.
    explicit TranscriptExtractor(QString cache_directory, QString ffmpeg = {},
                                 QObject* parent = nullptr);
    ~TranscriptExtractor() override;

    bool running() const { return process_ != nullptr; }

    // Extracts the stream at stream_index of the local video. Emits finished
    // once, possibly before returning, unless cancelled first. Ignored while
    // an extraction runs.
    void start(const QString& video_path, int track_id, int stream_index);
    // Stops a running extraction without emitting finished.
    void cancel();

   signals:
    void runningChanged();
    // Exactly one of transcript and error is non-empty.
    void finished(const QByteArray& transcript, const QString& error);

   private:
    void read_transcript(const QString& path);
    void complete(const QString& staging, const QString& cache, const QString& video_path,
                  const QDateTime& modified);
    void stop();

    QString cache_directory_;
    QString ffmpeg_;
    QProcess* process_ = nullptr;
};

}  // namespace niratan::player
