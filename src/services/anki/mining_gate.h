// SPDX-License-Identifier: GPL-3.0-or-later
//
// Decision order of an Anki mining attempt, following
// Features/Popup/AnkiMining.swift (preflightAnkiMining, mineAnkiEntry) and
// Core/AnkiManager.swift (needsVideoScreenshot, needsVideoAudioClip).
//
// The gate covers the decisions that need neither the network nor the active
// profile. The caller activates the profile, runs the duplicate lookup the
// gate asks for, and adds the note itself when the gate passes.
//
// Order, first match wins:
//   1. Unless the caller already ran the preflight, a missing selected deck
//      or note type fails with "Configure Anki deck and model first.". An
//      empty string is a selected value.
//   2. Unless allow_dupes is set, the caller's duplicate lookup runs for
//      duplicate_expression(content). A duplicate fails with "Already exists
//      in Anki." and note_id set to the first reported note ID, or to nothing
//      when the lookup reported none.
//   3. When needs_video_audio_clip is set and the context has a video whose
//      audio clip file name and path are both missing, it fails with the
//      video's audio_clip_error_message, else "Unable to capture the
//      subtitle audio clip.".
//   4. The screenshot check follows the audio clip check with the same rule
//      and the messages "screenshot_error_message" and "Unable to capture
//      the video screenshot.".
//
// A context without a video skips both media checks. The whole preflight,
// duplicate lookup included, is skipped when the caller says it already
// passed, but the media checks still run. A nullopt return means every gate
// passed. Messages are English source text through QCoreApplication::translate
// and reach the popup unchanged otherwise.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/anki/mining_context.h"
#include "services/anki/field_content.h"
#include "services/anki/field_mapping.h"

namespace niratan::anki {

enum class MiningStatus { added, duplicate, failed, pending };

struct MiningResult {
    MiningStatus status = MiningStatus::failed;
    std::string message;
    // AnkiConnect note ID. Only an added or duplicate result carries one.
    std::optional<std::int64_t> note_id;

    bool operator==(const MiningResult&) const = default;
};

struct DuplicateLookupResult {
    bool is_duplicate = false;
    std::vector<std::int64_t> note_ids;

    bool operator==(const DuplicateLookupResult&) const = default;
};

// AnkiManager's settings that the gate reads. The needs flags come from
// needs_video_screenshot and needs_video_audio_clip.
struct MiningSettings {
    std::optional<std::string> selected_deck;
    std::optional<std::string> selected_note_type;
    bool allow_dupes = false;
    bool needs_video_screenshot = false;
    bool needs_video_audio_clip = false;
};

// The word a duplicate check searches for: content["expression"], or "Entry"
// when the key is missing. A present empty value stays empty.
std::string duplicate_expression(const Strings& content);

// A mapped field value equal to the handlebar, compared exactly, is the only
// thing that raises a flag. Field names and unmapped fields do not matter.
bool needs_video_screenshot(const FieldMappings& mappings);
bool needs_video_audio_clip(const FieldMappings& mappings);
// AnkiManager.swift needsSasayakiAudio: the Sasayaki handlebar is needed only
// when some mapped value is exactly "{sasayaki-audio}".
bool needs_sasayaki_audio(const FieldMappings& mappings);

// The caller's duplicate lookup. It runs at most once and only when the gate
// reaches the duplicate check.
using DuplicateLookup = std::function<DuplicateLookupResult(std::string_view expression)>;

// Returns the failure or duplicate the order above produces, or nothing when
// the caller may add the note. The lookup may be empty only when it is never
// reached.
std::optional<MiningResult> gate_anki_mining(const MiningSettings& settings, const Strings& content,
                                             const MiningContext& context,
                                             const DuplicateLookup& duplicate_lookup,
                                             bool preflight_passed = false);

}  // namespace niratan::anki
