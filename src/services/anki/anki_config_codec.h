// SPDX-License-Identifier: GPL-3.0-or-later
//
// The JSON codec for services/anki/anki_config.h, following Models/Anki.swift
// (AnkiProfileConfig, AnkiConfig, AnkiConnectConfig) and the effective getters
// of AnkiProfileConfig and AnkiAnimatedAVIFConfiguration.
//
// This header is private to the module: the public header and its callers see
// only AnkiConfigStore. Decoding mirrors synthesized Codable: an absent or null
// optional member is nil, a required member or a wrong type fails the whole
// document, and stored enum raw values must match exactly. Encoding omits nil
// optional members and keeps only the members the Swift structs have.
//
// Known difference: integer members decode as 32-bit, while Swift's Int is
// 64-bit, so a value beyond the 32-bit range makes the document undecodable
// here where Swift would decode and clamp it. Real configuration values stay
// far inside that range.

#pragma once

#include <QJsonDocument>
#include <QJsonObject>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "models/anki/note_type.h"
#include "services/anki/anki_config.h"
#include "services/anki/duplicate_scope.h"

namespace niratan::anki::detail {

// Compression members shared by AnkiProfileConfig and AnkiConfig.
struct CompressionFields {
    std::optional<bool> compress_video_screenshots;
    std::optional<bool> compress_images;
    std::optional<double> image_compression_quality;
    std::optional<ImageCompressionFormat> image_compression_format;
    std::optional<int> animated_avif_maximum_height;
    std::optional<int> animated_avif_frames_per_second;
    std::optional<AudioCompressionFormat> audio_compression_format;
    std::optional<int> audio_compression_bitrate_kbps;
};

struct CommonFields {
    std::optional<std::string> selected_deck;
    std::optional<std::string> selected_note_type;
    bool allow_dupes = false;
    std::optional<bool> compact_glossaries;
    std::optional<bool> embed_media;
    std::map<std::string, std::string, std::less<>> field_mappings;
    std::optional<std::string> tags;
    CompressionFields compression;
};

// AnkiProfileConfig: the per-profile file.
struct ProfileDocument : CommonFields {
    DuplicateScope duplicate_scope = DuplicateScope::collection;
    bool check_all_models = false;
};

// AnkiConnectConfig as stored in the global file.
struct TransportDocument {
    std::optional<std::string> url;
    int timeout = 10;
    DuplicateScope duplicate_scope = DuplicateScope::collection;
    bool check_all_models = false;
    bool force_sync = false;
    std::optional<std::string> api_key;
};

// AnkiConfig: the global file.
struct GlobalDocument : CommonFields {
    std::vector<std::string> available_decks;
    std::vector<NoteType> available_note_types;
    std::optional<bool> use_anki_connect;
    std::optional<TransportDocument> anki_connect_config;
};

std::optional<ProfileDocument> decode_profile(const QJsonDocument& document);
std::optional<GlobalDocument> decode_global(const QJsonDocument& document);

QJsonObject encode_profile(const ProfileDocument& profile);
QJsonObject encode_global(const GlobalDocument& global);

// AnkiProfileConfig.effective* and AnkiAnimatedAVIFConfiguration.clamped*.
CompressionFields effective_compression(const CompressionFields& compression);

// AnkiManager.apply(_:) and the defaults of loadProfile's missing and
// fallthrough branches.
void apply_profile(const ProfileDocument& profile, AnkiSettings& settings);
void reset_profile(AnkiSettings& settings);

}  // namespace niratan::anki::detail
