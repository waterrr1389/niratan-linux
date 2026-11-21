// SPDX-License-Identifier: GPL-3.0-or-later
//
// VideoPlaybackHistoryStore.swift (VideoPlaybackResumeOptions,
// VideoAudioSelection, VideoAudioTrackIdentity) and VideoPlayerViewModel.swift
// (restoreResumeOptions). Capture omits default speed and tiny delays. Audio
// capture distinguishes no audio tracks from tracks intentionally turned off.
// Restore matches the first stream index, or the first track ID when no index
// was saved, then falls back to unique title/language/codec metadata. Metadata
// comparison does not fold canonically equivalent Unicode strings.

#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>
#include <variant>

namespace niratan::player {

struct AudioTrackIdentity {
    qint64 track_id = 0;
    std::optional<qint64> ff_index;
    QString title;
    std::optional<QString> language;
    std::optional<QString> codec;

    bool operator==(const AudioTrackIdentity&) const = default;
};

struct ResumeAudioTrack {
    AudioTrackIdentity identity;
    bool selected = false;
};

struct AudioOff {
    bool operator==(const AudioOff&) const = default;
};

using ResumeAudioSelection = std::variant<AudioOff, AudioTrackIdentity>;

struct ResumeOptions {
    std::optional<double> speed;
    std::optional<double> subtitle_delay;
    std::optional<double> audio_delay;
    std::optional<ResumeAudioSelection> audio_selection;

    bool empty() const;
    bool operator==(const ResumeOptions&) const = default;
};

ResumeOptions capture_resume_options(double speed, double subtitle_delay, double audio_delay,
                                     const QList<ResumeAudioTrack>& tracks);
// Clamp present values only. Decoding itself preserves stored values.
ResumeOptions normalize_resume_options(ResumeOptions options);
std::optional<qint64> matching_audio_track_id(const AudioTrackIdentity& identity,
                                             const QList<ResumeAudioTrack>& tracks);

// Swift Codable uses camel-case keys and encodes the enum as {"off": {}}
// or {"embedded": {"_0": { ...identity }}}. Absent optionals are omitted.
QJsonObject encode_resume_options(const ResumeOptions& options);
std::optional<ResumeOptions> decode_resume_options(const QJsonObject& object);

}  // namespace niratan::player
