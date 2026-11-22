// SPDX-License-Identifier: GPL-3.0-or-later
//
// The private state of SasayakiViewModel, shared by sasayaki_view_model.cpp and
// its playback, matching and audiobook splits. The playback and matching fields keep the meaning
// the class comment gives them; audiobook_metadata, audiobook_chapters and
// loading_audiobook_chapters follow SasayakiPlayer.audiobookMetadata,
// audiobookChapters and isLoadingAudiobookChapters.
//
// pending_cue is the cue of a chapter the model asked the reader to load,
// reveal_pending_cue_on_restore its reveal flag, chapter_transition whether a
// chapter change is in flight, and should_resume whether playback returns
// after it, following SasayakiPlayer's properties of the same names. The
// pending cue's start is the reader progress the chapter loads at.
// find_match_by_id is SasayakiPlayer's match lookup by id, shared by the
// playback and popup splits. cue_audio_exporter owns the cue-audio cache of
// SasayakiPlayer.miningAudioCache; sasayaki_view_model_mining.cpp creates it
// with this model as its parent.

#pragma once

#include "sasayaki_view_model.h"

#include <QVariantList>

#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "models/sasayaki/sasayaki.h"
#include "services/sasayaki/cue_timeline.h"

class QAudioOutput;
class QMediaPlayer;
class QTimer;

namespace niratan::sasayaki {
class CueAudioExporter;
}

namespace niratan::viewmodels {

// SasayakiPlayer's match lookup by id, shared by the playback and popup
// splits.
inline const sasayaki::SasayakiMatch*
find_match_by_id(const std::optional<sasayaki::SasayakiMatchData>& match, std::string_view id) {
    if (!match.has_value()) {
        return nullptr;
    }
    for (const sasayaki::SasayakiMatch& candidate : match->matches) {
        if (candidate.id == id) {
            return &candidate;
        }
    }
    return nullptr;
}

struct SasayakiViewModel::State {
    QMediaPlayer* player = nullptr;
    QAudioOutput* output = nullptr;
    QTimer* seek_timer = nullptr;

    // The open book's folder under Books, set while has_root.
    std::string folder;
    bool has_root = false;

    sasayaki::SasayakiPlaybackData playback;
    std::optional<sasayaki::SasayakiMatchData> match;
    std::vector<sasayaki::SasayakiCue> match_cues;
    std::vector<sasayaki::SasayakiCue> subtitle_cues;
    sasayaki::CueTimeline timeline;

    sasayaki::SasayakiAudiobookMetadata audiobook_metadata;
    std::vector<sasayaki::SasayakiAudiobookChapter> audiobook_chapters;
    bool loading_audiobook_chapters = false;
    int last_audiobook_chapter_id = -1;

    QString audio_path;
    QString audio_name;
    QString subtitle_name;
    QString match_error;
    QString error;
    QString current_cue_id;
    QString current_cue_text;
    QVariantList cue_list;

    // SasayakiPlayer.pendingCue and its restore flags.
    std::optional<sasayaki::SasayakiMatch> pending_cue;
    bool reveal_pending_cue_on_restore = false;
    bool chapter_transition = false;
    bool should_resume = false;

    // Created on the first requestCueAudio.
    sasayaki::CueAudioExporter* cue_audio_exporter = nullptr;

    double position = 0.0;
    double duration = 0.0;
    double rate = 1.0;
    double delay = 0.0;
    double match_progress = 0.0;
    // Default search window from SasayakiMatchView's 50...1000 range.
    int match_search_window = 200;
    std::optional<int> pending_match_window;
    std::jthread match_worker;

    bool has_audio = false;
    bool has_match = false;
    bool playing = false;
    bool reloading = false;
    bool needs_initial_seek = false;
    // SasayakiPlayer.hasPlayedOnce: set on the first start, never reset.
    bool has_played_once = false;
    // The sasayakiAutoScroll and sasayakiAutoPause preferences.
    bool auto_scroll = true;
    bool auto_pause = true;
    std::optional<double> pending_seek;
    // The landing options of the seek in flight.
    bool seek_start_playback = false;
    bool seek_update_cue = true;
    std::optional<double> seek_stop_time;
    // B2: armed by playCue(stop: true), cleared when reached.
    std::optional<double> stop_playback_time;
    int last_second = -1;
};

}  // namespace niratan::viewmodels
