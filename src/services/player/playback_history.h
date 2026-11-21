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
//   subtitleSelections     { key: VideoSubtitleSelection }
//   externalSubtitlePaths  { key: path }
// VideoPlaybackState encodes position, duration (omitted when unknown),
// updatedAt as seconds since 2001-01-01 UTC (Foundation's default Date
// coding), isFinished and resumeOptions. Resume options contain the per-video
// speed, delays and audio selection (VideoPlaybackResumeOptions).
//
// Saving follows updatePlaybackState: a position under 2 s or an unknown
// duration removes the entry, a position within 5 s of the end records a
// finished state at the duration without resume options, and anything else
// records the options and legacy position. A state is resumable when it is not finished, is
// at least 2 s in and, with a known duration, under 98 % of it.
//
// Swift migrates UserDefaults positions once into the file. The Linux
// equivalent is the earlier <data root>/video_playback.json, whose entries
// { path: { position } } become legacy positions the first time the new file
// is read and found missing. Persistence failures are ignored, as in Swift;
// a damaged document is never replaced (storage::write_json_file refuses),
// where Swift would start over.

#pragma once

#include "services/player/resume_options.h"

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
    ResumeOptions resume_options;

    // VideoPlaybackState.progress: 1 when finished, else position over a
    // positive duration clamped to 0..1, else none.
    std::optional<double> progress() const;
    // VideoPlaybackState.isResumable.
    bool resumable() const;

    bool operator==(const PlaybackState&) const = default;
};

struct PlaybackOpenState {
    std::optional<double> position;
    ResumeOptions options;

    bool operator==(const PlaybackOpenState&) const = default;
};

// VideoSubtitleTrackIdentity matches the stream index (ID only when the
// index was absent), then falls back to unique title/language/codec metadata.
struct SubtitleTrackIdentity {
    int id = 0;
    std::optional<int> stream_index;
    QString title;
    QString language;
    QString codec;

    bool operator==(const SubtitleTrackIdentity&) const = default;
};

// VideoSubtitleSelection: monostate is explicit Off, QString is a local
// external path. Unsupported remote selections are left in the document.
using SubtitleSelection = std::variant<std::monostate, SubtitleTrackIdentity, QString>;

std::optional<int> matching_subtitle_track_id(const SubtitleTrackIdentity& identity,
                                             const QList<SubtitleTrackIdentity>& tracks);

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

    // VideoPlayerViewModel.openPlaylistItem: call after saving the outgoing
    // file. Explicit starts clear progress even when remembering is disabled.
    PlaybackOpenState prepare_open(const QString& key, bool remember, bool from_beginning);

    // savePlaybackState. Returns whether the document changed.
    bool save(const QString& key, double position, double duration,
              const QDateTime& now = QDateTime::currentDateTimeUtc());
    bool save(const QString& key, double position, double duration, const ResumeOptions& options,
              const QDateTime& now = QDateTime::currentDateTimeUtc());
    // markWatched: a finished state at the duration (0 when unknown).
    bool mark_watched(const QString& key, std::optional<double> duration,
                      const QDateTime& now = QDateTime::currentDateTimeUtc());
    // clearProgress: removes both the state and the legacy position.
    bool clear(const QString& key);

    // Independent of playback progress, including an explicit start from zero.
    std::optional<SubtitleSelection> subtitle_selection(const QString& key) const;
    bool save_subtitle_selection(const QString& key, const SubtitleSelection& selection);

    const std::filesystem::path& file() const { return file_; }

   private:
    QJsonObject read() const;
    bool write(const QJsonObject& document) const;

    std::filesystem::path file_;
    std::filesystem::path legacy_file_;
};

}  // namespace niratan::player
