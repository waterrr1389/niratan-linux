// SPDX-License-Identifier: GPL-3.0-or-later

#include "mining_gate.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

#include <QCoreApplication>
#include <QString>

namespace niratan::anki {

// AnkiMining.swift preflightAnkiMining: content["expression"] ?? "Entry".
// A present empty value is kept; only a missing key falls back.
std::string duplicate_expression(const Strings& content) {
    const auto expression = content.find("expression");
    if (expression == content.end()) {
        return "Entry";
    }
    return expression->second;
}

// AnkiManager.swift needsVideoScreenshot: an exact match of either handlebar as
// a whole mapped value. The value is not trimmed or normalised.
bool needs_video_screenshot(const FieldMappings& mappings) {
    return std::ranges::any_of(mappings, [](const auto& mapping) {
        return mapping.second == "{book-cover}" || mapping.second == "{video-screenshot}";
    });
}

// AnkiManager.swift needsVideoAudioClip, with the same exact-value rule.
bool needs_video_audio_clip(const FieldMappings& mappings) {
    return std::ranges::any_of(mappings, [](const auto& mapping) {
        return mapping.second == "{sasayaki-audio}" || mapping.second == "{video-audio-clip}";
    });
}

// AnkiManager.swift needsSasayakiAudio: an exact match of the whole mapped
// value. Field names and other fields do not matter.
bool needs_sasayaki_audio(const FieldMappings& mappings) {
    return std::ranges::any_of(
        mappings, [](const auto& mapping) { return mapping.second == "{sasayaki-audio}"; });
}

std::optional<MiningResult> gate_anki_mining(const MiningSettings& settings, const Strings& content,
                                             const MiningContext& context,
                                             const DuplicateLookup& duplicate_lookup,
                                             bool preflight_passed) {
    // mineAnkiEntry runs preflightAnkiMining only when the caller has not
    // already done so. An empty string is a selected deck or note type.
    if (!preflight_passed) {
        if (!settings.selected_deck.has_value() || !settings.selected_note_type.has_value()) {
            return MiningResult{
                MiningStatus::failed,
                QCoreApplication::translate("AnkiMining", "Configure Anki deck and model first.")
                    .toStdString(),
                std::nullopt,
            };
        }

        // The lookup runs at most once, after the deck and note type check.
        if (!settings.allow_dupes) {
            const DuplicateLookupResult duplicate =
                duplicate_lookup(duplicate_expression(content));
            if (duplicate.is_duplicate) {
                std::optional<std::int64_t> note_id;
                if (!duplicate.note_ids.empty()) {
                    note_id = duplicate.note_ids.front();
                }
                return MiningResult{
                    MiningStatus::duplicate,
                    QCoreApplication::translate("AnkiMining", "Already exists in Anki.")
                        .toStdString(),
                    note_id,
                };
            }
        }
    }

    // mineAnkiEntry: the audio clip check comes before the screenshot check.
    // Both file name and path absent means the clip was not captured; a
    // present empty file name is a value, as for the rendered field.
    if (settings.needs_video_audio_clip && context.video.has_value()) {
        const VideoMiningContext& video = *context.video;
        if (!video.audio_clip_filename.has_value() && !video.audio_clip_path.has_value()) {
            const std::string message =
                video.audio_clip_error_message.has_value()
                    ? *video.audio_clip_error_message
                    : QCoreApplication::translate("AnkiMining",
                                                  "Unable to capture the subtitle audio clip.")
                          .toStdString();
            return MiningResult{MiningStatus::failed, message, std::nullopt};
        }
    }

    if (settings.needs_video_screenshot && context.video.has_value()) {
        const VideoMiningContext& video = *context.video;
        if (!video.screenshot_filename.has_value() && !video.screenshot_path.has_value()) {
            const std::string message =
                video.screenshot_error_message.has_value()
                    ? *video.screenshot_error_message
                    : QCoreApplication::translate("AnkiMining",
                                                  "Unable to capture the video screenshot.")
                          .toStdString();
            return MiningResult{MiningStatus::failed, message, std::nullopt};
        }
    }

    return std::nullopt;
}

}  // namespace niratan::anki
