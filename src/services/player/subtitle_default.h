// SPDX-License-Identifier: GPL-3.0-or-later
//
// First-open subtitle policy, following Models/Subtitle.swift
// (VideoSubtitleAutoloadCandidate.bestCandidate,
// matchesLanguageSuffixedSidecar, compareCandidateOrder),
// Features/Video/Playback/VideoTrack.swift (VideoTrack,
// VideoEmbeddedSubtitleDefault.preferredTrack, isJapanese, rank),
// Features/Video/VideoPlaybackHistoryStore.swift
// (VideoSubtitleRestoreResolver.resolve) and
// Features/Video/VideoPlayerScreen+Subtitles.swift
// (restoreRememberedSubtitleSelectionOrAutoload, autoloadSubtitleIfAvailable,
// applyEmbeddedSubtitleDefaultIfReady, loadPrimarySubtitle,
// selectSubtitleTrack, applySubtitlesOff).
//
// A sidecar is chosen before any embedded track: the exact same-folder stem
// with a lowercase srt, vtt, ass or ssa extension wins in that extension
// order. Only on an exact miss are immediate directory entries considered,
// ignoring hidden names; their extension is case-insensitive, and the stem
// must start with the exact media stem followed by "." or a space. Such
// suffixed candidates still rank by extension and then by a natural,
// case-insensitive filename comparison, so "Episode 2.srt" precedes
// "Episode 10.srt". Non-file URLs yield no path. A directory listing is
// required only when no exact match exists. Every result is a standardized
// absolute file URL.
//
// A remembered choice is resolved before either default: Off returns off even
// before tracks load; a stored external path exists or reads unavailable; an
// embedded identity waits before the snapshot is loaded and, once loaded,
// matches by stream index (or track ID when no index was saved), then by
// unique title/language/codec metadata. A loaded nonempty track list without a
// match falls back to autoload; an empty list keeps waiting.
//
// Without a remembered choice or sidecar the embedded default is pending until
// the loaded snapshot arrives. It is consumed exactly once, and an existing
// primary subtitle document or any selected subtitle track leaves subtitles
// alone. A new media file, a primary subtitle load, an explicit track
// selection or Off invalidates the pending default.
//
// The default considers embedded subtitle tracks only: Japanese means language
// ja, jp, jpn, japanese or a ja- prefix case-insensitively, or a title
// containing 日本語 or case-insensitive japanese. Candidates rank by a
// partial-title penalty (sign, song, forced, 看板, 歌詞 substrings,
// case-insensitive) of 2 plus an image penalty of 1; lower wins and equal
// ranks keep track order. No candidate leaves subtitles off. The chosen track
// is activated by the ordinary subtitle selection without remembering it or
// showing an OSD, so it supplies both subtitles and the embedded transcript.
//
// An external sidecar is loaded through the primary subtitle path even when
// its document cannot be parsed; the embedded default is never attempted as a
// substitute. Only a successful primary parse is remembered, as the
// standardized external path, and only when playback history is enabled.
//
// mpv's sub-auto may have added the sidecar before the track snapshot arrives.
// An existing external subtitle track is found by symlink-resolved path; the
// internal ASS effects track is ignored. A found track is reused instead of
// adding a duplicate, and the sidecar is otherwise added with selection.
//
// Linux keeps native subtitle rendering and the existing text document wiring
// (Decision: no interactive ASS styling), so the internal effects track is a
// defensive exclusion rather than a track this port creates.

#pragma once

#include "services/player/playback_history.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace niratan::player {

enum class VideoTrackType { video, audio, subtitle };

// VideoTrack.swift VideoTrack. Empty metadata and absent optionals are
// distinct: an absent external filename marks an embedded track, while an
// empty one belongs to a track mpv reports without a usable filename.
struct VideoTrack {
    int id = 0;
    VideoTrackType type = VideoTrackType::video;
    QString title;
    std::optional<QString> language;
    std::optional<QString> codec;
    std::optional<int> ff_index;
    std::optional<QString> external_filename;
    bool is_image = false;
    bool is_selected = false;

    bool operator==(const VideoTrack&) const = default;
};

// VideoEmbeddedSubtitleDefault.preferredTrack.
std::optional<VideoTrack> preferred_embedded_subtitle_track(const QList<VideoTrack>& tracks);
// VideoEmbeddedSubtitleDefault.isJapanese.
bool is_japanese_subtitle_track(const VideoTrack& track);

// VideoSubtitleAutoloadCandidate.bestCandidate.
std::optional<QUrl> best_subtitle_sidecar(const QUrl& media_url);

struct SubtitleFirstOpenPlan {
    std::optional<QUrl> sidecar;
    bool wait_for_embedded_default = false;

    bool operator==(const SubtitleFirstOpenPlan&) const = default;
};

// VideoPlayerScreen+Subtitles.autoloadSubtitleIfAvailable: a sidecar takes the
// primary external path; an exact miss leaves the embedded default pending.
SubtitleFirstOpenPlan plan_subtitle_first_open(const QUrl& media_url);

// VideoPlayerScreen+Subtitles.loadPrimarySubtitle: the standardized path to
// remember when the primary document parsed, otherwise none, so a failed parse
// never stores a subtitle selection.
std::optional<QUrl> primary_subtitle_selection_path(const QUrl& file, const QByteArray& bytes);

enum class SubtitleRestoreKind { off, external, embedded_track, waiting_for_tracks, unavailable };

struct SubtitleRestoreResolution {
    SubtitleRestoreKind kind = SubtitleRestoreKind::unavailable;
    QUrl external;
    int track_id = 0;

    bool operator==(const SubtitleRestoreResolution&) const = default;
};

// VideoSubtitleRestoreResolver.resolve for the local-video selections this
// port stores. Remote selections are out of scope.
SubtitleRestoreResolution resolve_subtitle_restore(const SubtitleSelection& selection,
                                                   const QList<VideoTrack>& tracks, bool is_loaded);

struct EmbeddedSubtitleActivation {
    int track_id = 0;
    bool remember_selection = false;
    bool show_osd = false;
    bool extract_transcript = true;

    bool operator==(const EmbeddedSubtitleActivation&) const = default;
};

// VideoPlayerScreen+Subtitles.applyEmbeddedSubtitleDefaultIfReady. wait starts
// the pending default after a sidecar miss; invalidate stops it for new media
// and explicit subtitle choices. apply_if_ready consumes it on the first loaded
// snapshot, whether or not a track is chosen.
class EmbeddedSubtitleDefault {
   public:
    void wait_for_tracks();
    void invalidate();
    bool awaiting() const { return awaiting_; }

    std::optional<EmbeddedSubtitleActivation>
    apply_if_ready(bool is_loaded, const QList<VideoTrack>& tracks, bool has_primary_document);

   private:
    bool awaiting_ = false;
};

// HSMpvClient.existingExternalSubtitleTrackIDForURL: the non-effects external
// subtitle track whose symlink-resolved filename is the sidecar's.
std::optional<int> existing_external_subtitle_track_id(const QUrl& sidecar,
                                                       const QList<VideoTrack>& tracks);

}  // namespace niratan::player
