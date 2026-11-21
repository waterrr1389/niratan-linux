// SPDX-License-Identifier: GPL-3.0-or-later
//
// Per-video playback progress, following VideoPlaybackHistoryStore.swift
// (position, playbackState, savePlaybackState, updatePlaybackState,
// markWatched, clearProgress, Snapshot, Storage) and VideoPlaybackState
// (progress, isResumable) in the same file. Keys are
// VideoMediaIdentity.persistenceKey for a local file: its absolute path.
//
// The document is <data root>/video_playback_history.json with the Swift
// Snapshot members:
//   positions              { key: seconds }            legacy restore points
//   playbackStates         { key: VideoPlaybackState }
//   subtitleSelections     { key: ... }                kept, not interpreted
//   externalSubtitlePaths  { key: path }               kept, not interpreted
// VideoPlaybackState encodes position, duration (omitted when unknown),
// updatedAt as seconds since 2001-01-01 UTC (Foundation's default Date
// coding), isFinished and resumeOptions. resumeOptions is kept as stored and
// written as {} for a new state; the per-video speed, delays and audio track
// it holds are not restored yet.
//
// Saving follows updatePlaybackState: a position under 2 s or an unknown
// duration removes the entry, a position within 5 s of the end records a
// finished state at the duration, anything else records a resumable state
// and its legacy position. A state is resumable when it is not finished, is
// at least 2 s in and, with a known duration, under 98 % of it.
//
// Swift migrates UserDefaults positions once into the file. The Linux
// equivalent is the earlier <data root>/video_playback.json, whose entries
// { path: { position } } become legacy positions the first time the new file
// is read and found missing. Persistence failures are ignored, as in Swift;
// a damaged document is never replaced (storage::write_json_file refuses),
// where Swift would start over.

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include <filesystem>
#include <optional>

namespace niratan::player {

struct PlaybackState {
    double position = 0;
    std::optional<double> duration;
    QDateTime updated_at;
    bool finished = false;
    QJsonObject resume_options;

    // VideoPlaybackState.progress: 1 when finished, else position over a
    // positive duration clamped to 0..1, else none.
    std::optional<double> progress() const;
    // VideoPlaybackState.isResumable.
    bool resumable() const;

    bool operator==(const PlaybackState&) const = default;
};

class PlaybackHistory {
   public:
    // <data root>/video_playback_history.json, migrating from
    // <data root>/video_playback.json. An empty data root stores nothing.
    static PlaybackHistory at_data_root();

    explicit PlaybackHistory(std::filesystem::path file, std::filesystem::path legacy_file = {});

    // playbackState(for:): the stored state, else a legacy position as a
    // state with no duration and an updatedAt of Date.distantPast.
    std::optional<PlaybackState> state(const QString& key) const;
    // position(for:): the state's position when resumable, else the legacy
    // position, else none.
    std::optional<double> resume_position(const QString& key) const;

    // savePlaybackState. Returns whether the document changed.
    bool save(const QString& key, double position, double duration,
              const QDateTime& now = QDateTime::currentDateTimeUtc());
    // markWatched: a finished state at the duration (0 when unknown).
    bool mark_watched(const QString& key, std::optional<double> duration,
                      const QDateTime& now = QDateTime::currentDateTimeUtc());
    // clearProgress: removes both the state and the legacy position.
    bool clear(const QString& key);

    const std::filesystem::path& file() const { return file_; }

   private:
    QJsonObject read() const;
    bool write(const QJsonObject& document) const;

    std::filesystem::path file_;
    std::filesystem::path legacy_file_;
};

}  // namespace niratan::player
