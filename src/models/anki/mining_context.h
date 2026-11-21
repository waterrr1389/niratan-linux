// SPDX-License-Identifier: GPL-3.0-or-later
//
// What card field rendering and mining read from the mining session,
// matching MiningContext and VideoMiningContext in Models/Anki.swift
// and DictionaryInfo and DictionaryCategory in Models/Dictionary.swift.
//
// Swift serves the book cover and the Sasayaki audio from LocalFileServer
// while it renders. The caller supplies the served URLs for rendering and
// a separate local cover path for attachments. An empty string means absent.

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace niratan::anki {

enum class DictionaryCategory { none, monolingual, bilingual, exclude };

// A term dictionary in the order the user ranked it.
struct TermDictionary {
    std::string title;
    DictionaryCategory category = DictionaryCategory::none;
};

struct VideoMiningContext {
    std::string file_name;
    std::string cue_text;
    double cue_start = 0;
    double cue_end = 0;
    std::optional<std::string> previous_cue_text;
    std::optional<std::string> next_cue_text;
    std::optional<std::string> screenshot_filename;
    std::optional<std::string> audio_clip_filename;
    // Paths of the files when no file name was set, as URL.path.
    std::optional<std::string> screenshot_path;
    std::optional<std::string> audio_clip_path;
    // Why the capture failed, shown by the mining gate instead of its own
    // message. Empty when the capture did not report a reason.
    std::optional<std::string> screenshot_error_message;
    std::optional<std::string> audio_clip_error_message;
};

struct MiningContext {
    std::string sentence;
    std::optional<std::string> document_title;
    std::string cover_url;
    std::string sasayaki_audio_url;
    std::optional<VideoMiningContext> video;
    // Empty means no cover attachment source, even when cover_url is set.
    std::string cover_path;
};

}  // namespace niratan::anki
