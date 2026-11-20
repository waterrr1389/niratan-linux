// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video mining history, following Features/Video/VideoMiningHistoryStore.swift
// (VideoMiningHistoryItem, VideoMiningHistoryStore.loadItems, record,
// pruneAndSave, delete, clear, defaultFileURL), Core/UserConfig.swift
// (videoMiningHistoryLimit, default 25, clamped at 0),
// Features/Video/Remote/RemoteVideoSource.swift (RemoteVideoIdentity) and
// Features/Video/Remote/YouTubeURLParser.swift (isYouTubeURL).
//
// Records live in <app data>/video_mining_history.json as a bare JSON array, the
// shape JSONEncoder writes for the store's [VideoMiningHistoryItem]:
//
//   [ <record>, ... ]
//
// Loading also unwraps an object whose "items" member holds that array, so a file
// written by an earlier build of this port still reads; writing always emits the
// bare array.
//
// Member names are the Swift property names. Optional members are omitted when
// absent, which is what JSONEncoder does for a nil optional. createdAt counts
// seconds since 2001-01-01T00:00:00Z, the unit Foundation's default JSON date
// strategy writes for a Date; Unix seconds are created_at + 978307200. cueStart
// and cueEnd are seconds, and a remote identity's duration is seconds as well.
// subtitleFormat holds the SubtitleFormat raw value: "srt", "webVTT", "ass",
// "ssa" or "embedded".
//
// A record keeps the identity members RemoteVideoIdentity.encode(to:) writes:
// providerID, remoteID, originalURL, canonicalURL, title, thumbnailURL and
// duration. Decoding follows RemoteVideoIdentity.init(from:) for the provider:
// providerID, or the legacy "provider" member when providerID is absent or null,
// and a "ytdlp" provider becomes "youtube" when the canonical URL, else the
// original URL, is a YouTube URL. The YouTube test here is the host only (http or
// https, youtube.com or youtu.be or a subdomain of either); isYouTubeURL also
// checks the path shape, youtube-nocookie.com and an 11-character video id, which
// this test leaves out. So a channel or playlist page on youtube.com is rewritten
// here where Swift keeps "ytdlp", while a youtube-nocookie.com video Swift
// rewrites stays "ytdlp" here.
//
// Store order is createdAt ascending, then id ascending, both compared in
// reference seconds. add appends the record and restores that order;
// pruned_items drops the oldest records while the count exceeds the limit, and a
// limit of 0 yields an empty list. A negative limit is treated as 0, following
// the clamp in the store's initializer. The limit comes from
// UserConfig.videoMiningHistoryLimit. Unlike the Swift store's initializer, load
// neither sorts nor prunes: a file keeps its stored order until add runs.
//
// Reading never repairs. A missing file, an unreadable or unparsable file, a
// document of another shape, a document holding one member of the wrong type or
// a subtitleFormat outside the five raw values all read as an empty list, as
// loadItems does for a document it cannot decode. load never writes.
//
// Saving goes through storage::write_json_file: indented UTF-8 written to a
// temporary file and committed over the target, and the writer refuses to replace
// a target that is not valid JSON, so damaged bytes stay untouched until the user
// removes them. A save reports false instead of throwing, and leaves the file
// unchanged when a value cannot be written as JSON (a non-finite number).
// subtitle_format is written verbatim, so a caller has to pass one of the five raw
// values: any other string makes the next load reject the whole document. History
// is optional context and must never block playback or mining.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::player {

struct MiningHistoryItem {
    // The identity as a record stores it, following RemoteVideoIdentity's
    // encode(to:). Decoding accepts the legacy "provider" member for provider_id
    // and rewrites a "ytdlp" provider on a YouTube URL to "youtube", following
    // that struct's init(from:).
    struct RemoteVideoIdentity {
        std::string provider_id;
        std::string remote_id;
        std::string original_url;
        std::optional<std::string> canonical_url;
        std::string title;
        std::optional<std::string> thumbnail_url;
        std::optional<double> duration;

        bool operator==(const RemoteVideoIdentity&) const = default;
    };

    // A UUID string in the form Foundation prints. The store passes it through
    // without parsing, because an id only has to stay stable between records.
    std::string id;
    // Seconds since 2001-01-01T00:00:00Z, Foundation's reference date; Unix
    // seconds are this plus 978307200.
    double created_at = 0.0;
    std::string subtitle_text;
    // The video's file name as displayed. video_title is the displayed title and
    // falls back to video_file_name when the stored member is absent or null.
    std::string video_file_name;
    std::string video_title;
    std::optional<std::string> video_path;
    std::optional<RemoteVideoIdentity> remote_video_identity;
    // The subtitle source name, falling back to video_file_name when the stored
    // member is absent or null.
    std::string subtitle_source_name;
    std::optional<std::string> subtitle_source_path;
    std::optional<std::string> subtitle_format;
    // Swift's Int is 64-bit; this field is an int, so a stored track id beyond int
    // range reads as a damaged document.
    std::optional<int> embedded_subtitle_track_id;
    // Seconds, the earliest cue start and the latest cue end of the record.
    double cue_start = 0.0;
    double cue_end = 0.0;

    bool operator==(const MiningHistoryItem&) const = default;
};

std::filesystem::path mining_history_path(const std::filesystem::path& app_directory);

// Records in stored order. A document that is an object with an "items" array is
// unwrapped first, since an earlier build of the port wrote that shape. Empty when
// the file is missing or damaged, and when it holds no record.
std::vector<MiningHistoryItem> load(const std::filesystem::path& app_directory);

// Writes items in the given order without sorting or pruning.
bool save(const std::filesystem::path& app_directory, const std::vector<MiningHistoryItem>& items);

// createdAt ascending, then id ascending. Keeps the newest limit records; a limit
// of 0 or less leaves no record. A NaN createdAt, which save refuses, sorts before
// every real timestamp so the comparator stays a strict weak ordering.
std::vector<MiningHistoryItem> pruned_items(std::vector<MiningHistoryItem> items, int limit);

// Appends item to the stored records, prunes to limit and saves. False when the
// file could not be written, including a damaged file the writer refuses to
// replace; the stored records are then unchanged.
bool add(const std::filesystem::path& app_directory, const MiningHistoryItem& item, int limit);

// Removes every record with id and saves. False, without touching the file, when
// no record has that id, and when the save fails.
bool remove(const std::filesystem::path& app_directory, std::string_view id);

// Stores an empty list.
bool clear(const std::filesystem::path& app_directory);

}  // namespace niratan::player
