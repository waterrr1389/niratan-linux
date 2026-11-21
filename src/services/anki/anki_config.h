// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Anki configuration store, following Core/AnkiManager.swift
// (AnkiManager stored state, init, loadTransport, loadProfile, apply(_:),
// ensureAnkiConnectURL, save, makeLegacyConfig(from:), activateProfile,
// setAnkiConnectURL, setAnkiConnectAPIKey, setFieldMapping, setTags),
// Models/Anki.swift (AnkiProfileConfig, AnkiConfig, AnkiConnectConfig,
// AnkiNoteType, DuplicateScope, AnkiImageCompressionFormat,
// AnkiAudioCompressionFormat, AnkiAnimatedAVIFConfiguration) and
// Core/ProfileRepository.swift (ankiConfigURL(for:), profileDirectory(for:),
// isSafeProfileID).
//
// Two files hold the settings. The per-profile file is
// <data root>/Profiles/<profile id>/anki_config.json and is the only source of
// the profile-owned state. The global file is <data root>/anki_config.json and
// carries the deck and note type lists plus the transport, alongside the
// AnkiConfig projection of the profile state (its duplicate scope and
// check-all-models live inside ankiConnectConfig, not at the top level).
// Loading reads the global file first, then the active profile file, then
// repairs the transport URL, so the profile's duplicate scope and
// check-all-models win over the global values.
//
// A save writes the active profile file and then the global file. The global
// file is the active state when the active profile is the default one; for any
// other profile a decodable global file keeps its profile state and duplicate
// settings while its lists and transport fields are replaced, and an
// undecodable one is rebuilt from the default profile's file, falling back to
// the active profile's state. availableDecks, availableNoteTypes and
// useAnkiConnect are never read from the per-profile file, and the per-profile
// file only ever holds the AnkiProfileConfig shape.
//
// The stored enums use the Swift raw values: duplicateScope is "collection",
// "deck" or "deckroot", imageCompressionFormat is "jpeg" or "avif" and
// audioCompressionFormat is "aac" or "mp3". Any other spelling, or a missing
// required member, makes the whole file undecodable.
//
// useAnkiConnect is ignored on load and a default-profile save writes true. A
// non-default save keeps the decodable global file's useAnkiConnect value,
// following AnkiConfig.updatingSharedState.
//
// As Swift's JSONEncoder does, a non-finite image compression quality is not
// encoded: save reports the failure before touching either file.
//
// Deviations from Swift, agreed in notes/specs/anki-anki_config.md:
// - A legacy AnkiConfig-shaped per-profile file is treated as undecodable and
//   loads the defaults; this port never migrates it (B15 and B16 dropped).
// - The transport API key is read back from the global file instead of being
//   dropped on load, so it survives a restart.
// - A damaged JSON file is not overwritten: storage::write_json_file refuses
//   the target, load yields the defaults, and save reports the failure instead
//   of replacing the bytes as Swift's `try? write` would.
//
// The store never calls the network and never touches media. The in-memory
// settings are concrete: every compression value is post-clamp, so a saved
// file always carries a concrete value for each of them.

#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/anki/note_type.h"
#include "services/anki/duplicate_scope.h"

namespace niratan::profiles {
class ProfileRepository;
}

namespace niratan::anki {

enum class ImageCompressionFormat { jpeg, avif };

enum class AudioCompressionFormat { aac, mp3 };

// Models/Anki.swift:AnkiConnectConfig. The URL is optional in the stored file;
// an absent or whitespace-only URL becomes the default after a load.
struct AnkiTransportConfig {
    std::optional<std::string> url = std::string("http://127.0.0.1:8765");
    int timeout = 10;
    DuplicateScope duplicate_scope = DuplicateScope::collection;
    bool check_all_models = false;
    bool force_sync = false;
    std::optional<std::string> api_key;

    bool operator==(const AnkiTransportConfig&) const = default;
};

// The applied configuration. Defaults match AnkiManager's stored state and
// AnkiProfileConfig.effective* fallbacks.
struct AnkiSettings {
    std::optional<std::string> selected_deck;
    std::optional<std::string> selected_note_type;
    std::map<std::string, std::string, std::less<>> field_mappings;
    std::string tags;
    bool allow_dupes = false;
    bool compact_glossaries = false;
    bool embed_media = false;
    bool compress_images = true;
    double image_compression_quality = 0.80;
    ImageCompressionFormat image_compression_format = ImageCompressionFormat::jpeg;
    int animated_avif_maximum_height = 350;
    int animated_avif_frames_per_second = 10;
    AudioCompressionFormat audio_compression_format = AudioCompressionFormat::aac;
    int audio_compression_bitrate_kbps = 64;
    std::vector<std::string> available_decks;
    std::vector<NoteType> available_note_types;
    // The shared transport; duplicate_scope and check_all_models are the values
    // both files carry. An empty optional means the transport is unset.
    std::optional<AnkiTransportConfig> transport = AnkiTransportConfig{};

    bool operator==(const AnkiSettings&) const = default;
};

struct AnkiSaveResult {
    bool ok = false;
    // Empty on success, otherwise the reason reported by storage.
    std::string error;

    bool operator==(const AnkiSaveResult&) const = default;
};

class AnkiConfigStore {
   public:
    // The profile directories come from profile_repository, which must be
    // rooted at data_root; the global file is data_root/anki_config.json. The
    // repository must outlive the store. The active profile is fixed at
    // construction from the repository.
    AnkiConfigStore(std::filesystem::path data_root,
                    const profiles::ProfileRepository& profile_repository);

    const AnkiSettings& settings() const;
    // Mutations are not written until save() or a setter runs.
    AnkiSettings& settings();

    std::string_view active_profile_id() const;

    // ProfileRepository.ankiConfigURL(for:). nullopt when the ID is not a safe
    // profile ID (empty, ".", "..", or containing '/' or '\').
    std::optional<std::filesystem::path> profile_config_path(
        std::string_view profile_id) const;
    // BookStorage.getAppDirectory()/anki_config.json.
    std::filesystem::path global_config_path() const;

    // The init sequence: load the transport, load the active profile, then
    // ensure the transport URL. Writes nothing.
    void load();

    // Writes the active profile file first, then the global file. False when
    // either write failed, including a damaged target that is left untouched.
    AnkiSaveResult save();

    // Saves the outgoing profile when it still exists, then switches and loads
    // the new profile. Unknown and already active IDs are ignored.
    void activate_profile(std::string_view profile_id);

    // The setters store their value and save at once. set_field_mapping stores
    // "" for a whitespace-only value and the value verbatim otherwise.
    AnkiSaveResult set_anki_connect_url(std::string url);
    AnkiSaveResult set_anki_connect_api_key(std::string api_key);
    AnkiSaveResult set_field_mapping(std::string field, std::string value);
    AnkiSaveResult set_tags(std::string tags);

   private:
    std::optional<std::filesystem::path> active_profile_directory() const;

    std::filesystem::path data_root_;
    const profiles::ProfileRepository* profile_repository_;
    std::string active_profile_id_;
    AnkiSettings settings_;
};

}  // namespace niratan::anki
