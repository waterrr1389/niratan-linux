// SPDX-License-Identifier: GPL-3.0-or-later
//
// The lookup popup's cue audio for the card, following
// Features/Sasayaki/SasayakiPlayer.swift (cueSentenceAudio, expandCue,
// rateAdjustedComposition and miningAudioCache),
// Core/AnkiAudioCompressor.swift
// (data(from:format:bitrateKbps:destinationURL:)), Models/Anki.swift
// (AnkiAudioCompressionFormat.fileExtension) and
// Util/ReaderCharacterNormalizer.swift (filteredText).
//
// expand_cue keeps the cue's chapter in stored order, finds the cue by id and
// widens the range over neighbours whose filtered text the filtered sentence
// contains, first backwards from the cue and then forwards. It widens one
// neighbour at a time and stops at the first one the sentence does not
// contain. Without match data or with a missing cue the cue's own start and
// end are used.
//
// cue_clip_range adds the playback delay to both ends: start = max(0,
// range.start + delay) and end = max(start, range.end + delay).
//
// cue_audio_arguments builds one ffmpeg pass:
//   -nostdin -y -i <source> -ss <start> -t <duration> [-filter:a <atempo>]
//   -vn -ac 1 -c:a <codec> -b:a <bitrate>k <stem>.<extension>
// The start and duration are printed with six decimals, the bitrate is clamped
// to 32...192 kbps, and AAC writes m4a with codec "aac" while MP3 writes mp3
// with "libmp3lame". The rate is applied only when |rate - 1| >= 0.001,
// preserving pitch: atempo=<rate> up to 2 and atempo=2,atempo=<rate/2> above
// it. The stored rate is clamped elsewhere to 0.5...2.5, so no further stage
// is needed.
//
// The cache key is the source path, the start and end milliseconds (rounded to
// nearest), the format's "aac"/"mp3" raw value, the bitrate and round(rate *
// 100), joined with '\n' in that order. A hit answers the stored bytes without
// a new export. clear_cache() drops them; the player calls it when the audio
// source changes and when the book closes.
//
// start answers finished exactly once per request, in request order, running
// one export at a time. A successful export reads the output file, removes it
// and caches its bytes. A missing ffmpeg, a process that fails to start, a
// non-zero exit, a missing or empty output file, and an end not greater than
// the start answer empty bytes without a message, so the card is still added
// without audio.
//
// The bytes differ from Swift's: one ffmpeg pass encodes mono with atempo,
// where Swift exports mp4 AAC and re-encodes with AVFoundation or SwiftLAME.
// Extension, mono, bitrate, range and the pitch-preserving rate match.

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <deque>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "models/anki/media_compression.h"

class QProcess;

namespace niratan::sasayaki {

struct CueAudioRange {
    double start = 0.0;
    double end = 0.0;

    bool operator==(const CueAudioRange&) const = default;
};

struct CueAudioRequest {
    QString source_path;
    double start = 0.0;
    double end = 0.0;
    double rate = 1.0;
    anki::AudioCompressionFormat format = anki::AudioCompressionFormat::aac;
    int bitrate_kbps = 64;
    // Without an extension; the format's extension is appended.
    QString output_stem;

    bool operator==(const CueAudioRequest&) const = default;
};

CueAudioRange expand_cue(const std::vector<SasayakiMatch>& matches, const SasayakiMatch& cue,
                         std::string_view sentence);

CueAudioRange cue_clip_range(const CueAudioRange& range, double delay);

std::string cue_audio_cache_key(const CueAudioRequest& request);

QStringList cue_audio_arguments(const CueAudioRequest& request);

class CueAudioExporter : public QObject {
    Q_OBJECT

   public:
    // An empty ffmpeg path means "ffmpeg", looked up on PATH at each start.
    explicit CueAudioExporter(QString ffmpeg = {}, QObject* parent = nullptr);
    ~CueAudioExporter() override;

    void start(const CueAudioRequest& request);
    void clear_cache();

   signals:
    void finished(QByteArray data);

   private:
    void process_next();
    void complete(const QByteArray& data);
    void fail();

    QString ffmpeg_;
    QProcess* process_ = nullptr;
    QString output_path_;
    std::string cache_key_;
    std::map<std::string, QByteArray> cache_;
    std::deque<CueAudioRequest> pending_;
    bool completed_ = false;
};

}  // namespace niratan::sasayaki
