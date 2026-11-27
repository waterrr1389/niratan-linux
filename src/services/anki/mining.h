// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows Features/Popup/AnkiMining.swift (preflightAnkiMining, mineAnkiEntry,
// AnkiMiningResult.webPayload), Core/AnkiManager.swift (addNote,
// NoteBuildConfiguration, addNoteAnkiConnect, getMediaDirPath, writeDirectMedia,
// syncAnkiConnect), Core/LocalFileServer.swift (clearMedia), Models/Anki.swift
// (MiningContext, VideoMiningContext, DictionaryMedia, Handlebars,
// AnkiAudioCompressionFormat.fileExtension), and Util/Extensions.swift (Data.sha1).
//
// Preflight activates the repository profile, gates deck/model and duplicates,
// and checks both active profile IDs after lookup before returning a verdict.
// Mining always checks and activates the target profile first. Without an
// already-passed preflight it then runs preflight and rechecks both IDs. Video
// capture checks still run even with an already-passed preflight.
//
// At the start of the add, snapshot the deck, model, field mappings, tags,
// duplicate options and compression values. Only exact media mappings become
// attachments; other mappings are rendered as text. The directory lookup
// caches both usable and unavailable results by transport URL and API key.
// Video names are used directly with a usable directory, without checking
// the named files. Cover and dictionary writes reuse an existing file;
// otherwise a dot-prefixed temporary file in that directory is moved into
// place without replacing the destination. A failed write or move uses the
// upload branch; a failed write leaves no temporary file behind, where Swift
// can leave a partial one. A positive add ID saves the expression, clears media and
// awaits best-effort sync if currently configured.
//
// Decisions: failures do not inherit old error text; pending is popup-only.
// Only cover_path supplies cover bytes; cover_url is rendered as text. A
// non-video Sasayaki bucket attaches context.sasayaki_audio_data; a video
// keeps its video-audio bucket. Audio downloads accept http, https and file.
// Until AnkiMediaProcessor.image has a port, the app supplies an identity
// image processor. Request parsing follows anki_requests, including its
// NSNumber/Bool differences and first-element-only canAdd verdict.

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QUrl>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "models/anki/media_compression.h"
#include "models/anki/mining_context.h"
#include "services/anki/duplicate_lookup.h"
#include "services/anki/field_content.h"

namespace niratan::anki {

enum class AnkiMiningStatus { added, duplicate, failed };

struct AnkiMiningResult {
    AnkiMiningStatus status = AnkiMiningStatus::failed;
    std::string message;
    std::optional<std::int64_t> note_id;

    // noteID, when present, is a decimal string, not a JSON number.
    QJsonObject web_payload() const;
    bool operator==(const AnkiMiningResult&) const = default;
};

struct MiningAttempt {
    Strings content;
    MiningContext context;
    std::vector<TermDictionary> dictionaries;
    // Profile captured by the caller alongside the context. If absent, use
    // the repository's current profile at the start of the attempt.
    std::optional<std::string> profile_id;
    bool preflight_already_passed = false;
};

struct MiningImage {
    QByteArray bytes;
    std::string extension;

    bool operator==(const MiningImage&) const = default;
};

struct MiningDependencies {
    using DownloadCallback = std::function<void(std::optional<QByteArray>)>;

    AnkiRequestFunction request;
    std::function<void(QUrl, DownloadCallback)> download_audio;
    std::function<MiningImage(MiningImage, bool, ImageCompressionFormat, double)> process_image;
    // DictionaryQueryService::media_file: missing media returns empty bytes.
    std::function<QByteArray(std::string_view dictionary, std::string_view path)> dictionary_media;
    std::function<void()> clear_media;
    std::function<std::string()> repository_profile_id;
    std::function<std::string()> anki_profile_id;
    std::function<void(std::string_view)> activate_profile;
    std::function<const AnkiSettings&()> settings;
};

class MiningService {
   public:
    using PreflightCallback = std::function<void(std::optional<AnkiMiningResult>)>;
    using Callback = std::function<void(AnkiMiningResult)>;

    // The service, duplicate lookup and seam targets must outlive pending
    // calls. Use one thread; seams complete once on that thread and may
    // complete inline. Completion may also be inline.
    MiningService(DuplicateLookupService& duplicate_lookup, MiningDependencies dependencies);

    // No result means preflight passed; the caller can now capture media.
    void preflight(Strings content, PreflightCallback callback);
    void mine(MiningAttempt attempt, Callback callback);

    // The active transport's Anki media directory for video capture, following
    // getMediaDirPath. The answer is the path only when it is an existing
    // writable directory. One getMediaDirPath is sent per transport URL and
    // API key, and both usable and unavailable answers are cached; card
    // building reads the same cache, so a capture followed by an add sends one
    // request. A failed request and a non-string result are unavailable.
    // Completes once on the calling thread, possibly inline.
    void media_directory(std::function<void(std::optional<std::filesystem::path>)> callback);

   private:
    DuplicateLookupService& duplicate_lookup_;
    MiningDependencies dependencies_;
    std::map<std::pair<std::string, std::string>, std::optional<std::filesystem::path>>
        media_directories_;
};

}  // namespace niratan::anki
