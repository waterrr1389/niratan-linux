// SPDX-License-Identifier: GPL-3.0-or-later

#include "anki_config.h"

#include <QJsonDocument>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "services/anki/anki_config_codec.h"
#include "services/anki/field_mapping.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/json_file.h"

namespace niratan::anki {
namespace {

using detail::GlobalDocument;
using detail::ProfileDocument;
using detail::TransportDocument;

constexpr std::string_view kDefaultUrl = "http://127.0.0.1:8765";
constexpr std::string_view kConfigFile = "anki_config.json";

// AnkiManager.makeLegacyConfig(from:): the profile state with the global
// lists, useAnkiConnect true and the in-memory transport.
GlobalDocument make_legacy_config(const ProfileDocument& profile, const AnkiSettings& settings) {
    GlobalDocument global;
    global.selected_deck = profile.selected_deck;
    global.selected_note_type = profile.selected_note_type;
    global.allow_dupes = profile.allow_dupes;
    global.compact_glossaries = profile.compact_glossaries.value_or(false);
    global.embed_media = profile.embed_media.value_or(false);
    global.field_mappings = profile.field_mappings;
    global.tags = profile.tags.value_or("");
    global.available_decks = settings.available_decks;
    global.available_note_types = settings.available_note_types;
    global.use_anki_connect = true;
    if (settings.transport.has_value()) {
        global.anki_connect_config = TransportDocument{
            settings.transport->url, settings.transport->timeout,
            settings.transport->duplicate_scope, settings.transport->check_all_models,
            settings.transport->force_sync, settings.transport->api_key};
    }
    global.compression = detail::effective_compression(profile.compression);
    return global;
}

// AnkiConfig.updatingSharedState: keep the stored profile fields and duplicate
// settings, replace the lists and the transport's own fields from memory.
GlobalDocument updating_shared_state(GlobalDocument existing, const AnkiSettings& settings) {
    existing.available_decks = settings.available_decks;
    existing.available_note_types = settings.available_note_types;
    if (!settings.transport.has_value()) {
        existing.anki_connect_config.reset();
        return existing;
    }
    const bool stored = existing.anki_connect_config.has_value();
    existing.anki_connect_config = TransportDocument{
        settings.transport->url, settings.transport->timeout,
        stored ? existing.anki_connect_config->duplicate_scope : DuplicateScope::collection,
        stored && existing.anki_connect_config->check_all_models,
        settings.transport->force_sync, settings.transport->api_key};
    return existing;
}

// The AnkiProfileConfig projection AnkiManager.save writes.
ProfileDocument profile_document_from(const AnkiSettings& settings) {
    ProfileDocument profile;
    profile.selected_deck = settings.selected_deck;
    profile.selected_note_type = settings.selected_note_type;
    profile.allow_dupes = settings.allow_dupes;
    profile.compact_glossaries = settings.compact_glossaries;
    profile.embed_media = settings.embed_media;
    profile.field_mappings = settings.field_mappings;
    profile.tags = settings.tags;
    profile.duplicate_scope = settings.transport.has_value() ? settings.transport->duplicate_scope
                                                             : DuplicateScope::collection;
    profile.check_all_models =
        settings.transport.has_value() ? settings.transport->check_all_models : false;
    profile.compression = {std::nullopt, settings.compress_images,
                           settings.image_compression_quality, settings.image_compression_format,
                           settings.animated_avif_maximum_height,
                           settings.animated_avif_frames_per_second,
                           settings.audio_compression_format,
                           settings.audio_compression_bitrate_kbps};
    return profile;
}

std::string trim_whitespace(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<int>(text.size())).trimmed().toStdString();
}

// AnkiManager.ensureAnkiConnectURL: a missing transport or a blank URL becomes
// the default URL, creating the transport when it is unset.
void ensure_transport_url(AnkiSettings& settings) {
    const std::optional<std::string> trimmed =
        settings.transport.has_value() && settings.transport->url.has_value()
            ? std::optional<std::string>(trim_whitespace(*settings.transport->url))
            : std::nullopt;
    if (trimmed.has_value() && !trimmed->empty()) {
        return;
    }
    if (!settings.transport.has_value()) {
        AnkiTransportConfig transport;
        transport.url = std::string(kDefaultUrl);
        settings.transport = std::move(transport);
    } else {
        settings.transport->url = std::string(kDefaultUrl);
    }
}

std::optional<GlobalDocument> read_global(const std::filesystem::path& path) {
    const storage::JsonReadResult read = storage::read_json_file(path);
    if (read.status != storage::JsonReadStatus::success) {
        return std::nullopt;
    }
    return detail::decode_global(read.document);
}

std::optional<ProfileDocument> read_profile(const std::filesystem::path& path) {
    const storage::JsonReadResult read = storage::read_json_file(path);
    if (read.status != storage::JsonReadStatus::success) {
        return std::nullopt;
    }
    return detail::decode_profile(read.document);
}

}  // namespace

AnkiConfigStore::AnkiConfigStore(std::filesystem::path data_root,
                                 const profiles::ProfileRepository& profile_repository)
    : data_root_(std::move(data_root)),
      profile_repository_(&profile_repository),
      active_profile_id_(profile_repository.active_profile().id) {}

const AnkiSettings& AnkiConfigStore::settings() const {
    return settings_;
}

AnkiSettings& AnkiConfigStore::settings() {
    return settings_;
}

std::string_view AnkiConfigStore::active_profile_id() const {
    return active_profile_id_;
}

std::optional<std::filesystem::path> AnkiConfigStore::profile_config_path(
    std::string_view profile_id) const {
    const std::optional<std::filesystem::path> directory =
        profile_repository_->profile_directory(profile_id);
    if (!directory.has_value()) {
        return std::nullopt;
    }
    return *directory / std::string(kConfigFile);
}

std::filesystem::path AnkiConfigStore::global_config_path() const {
    return data_root_ / std::string(kConfigFile);
}

void AnkiConfigStore::load() {
    // loadTransport: a decodable global file always replaces the deck lists;
    // its transport keeps url, timeout, forceSync and, unlike Swift, the API
    // key, while the duplicate policy is forced.
    if (const std::optional<GlobalDocument> global = read_global(global_config_path())) {
        settings_.available_decks = global->available_decks;
        settings_.available_note_types = global->available_note_types;
        if (global->anki_connect_config.has_value()) {
            const TransportDocument& stored = *global->anki_connect_config;
            AnkiTransportConfig transport;
            transport.url = stored.url;
            transport.timeout = stored.timeout;
            transport.duplicate_scope = DuplicateScope::collection;
            transport.check_all_models = false;
            transport.force_sync = stored.force_sync;
            transport.api_key = stored.api_key;
            settings_.transport = std::move(transport);
        }
    }

    // loadProfile: the profile file replaces every profile-owned value, or the
    // defaults when it is missing, damaged or not an AnkiProfileConfig.
    std::optional<ProfileDocument> profile;
    if (const std::optional<std::filesystem::path> path =
            profile_config_path(active_profile_id_)) {
        profile = read_profile(*path);
    }
    if (profile.has_value()) {
        detail::apply_profile(*profile, settings_);
    } else {
        detail::reset_profile(settings_);
    }
    ensure_transport_url(settings_);
}

AnkiSaveResult AnkiConfigStore::save() {
    const ProfileDocument profile = profile_document_from(settings_);

    // JSONEncoder rejects non-finite numbers; Qt would write null, so this
    // guard keeps the same behaviour and touches neither file.
    if (!std::isfinite(settings_.image_compression_quality)) {
        return {false, "image compression quality is not a finite number"};
    }

    std::string profile_error = "unsafe profile id";
    bool profile_written = false;
    if (const std::optional<std::filesystem::path> path =
            profile_config_path(active_profile_id_)) {
        profile_written = storage::write_json_file(
            *path, QJsonDocument(detail::encode_profile(profile)), &profile_error);
    }

    GlobalDocument global;
    const std::string& default_profile_id = profile_repository_->index().default_profile_id;
    if (active_profile_id_ == default_profile_id) {
        global = make_legacy_config(profile, settings_);
    } else if (std::optional<GlobalDocument> existing = read_global(global_config_path())) {
        global = updating_shared_state(std::move(*existing), settings_);
    } else {
        std::optional<ProfileDocument> default_profile;
        if (const std::optional<std::filesystem::path> path =
                profile_config_path(default_profile_id)) {
            default_profile = read_profile(*path);
        }
        global = make_legacy_config(
            default_profile.has_value() ? *default_profile : profile, settings_);
    }

    std::string global_error;
    const bool global_written =
        storage::write_json_file(global_config_path(),
                                 QJsonDocument(detail::encode_global(global)), &global_error);

    AnkiSaveResult result;
    result.ok = profile_written && global_written;
    if (!profile_written) {
        result.error = profile_error;
    } else if (!global_written) {
        result.error = global_error;
    }
    return result;
}

void AnkiConfigStore::activate_profile(std::string_view profile_id) {
    if (!profile_repository_->profile(profile_id).has_value() ||
        active_profile_id_ == profile_id) {
        return;
    }
    if (profile_repository_->profile(active_profile_id_).has_value()) {
        save();
    }
    active_profile_id_ = std::string(profile_id);

    // loadProfile only: the transport and the URL stay as they are.
    std::optional<ProfileDocument> profile;
    if (const std::optional<std::filesystem::path> path =
            profile_config_path(active_profile_id_)) {
        profile = read_profile(*path);
    }
    if (profile.has_value()) {
        detail::apply_profile(*profile, settings_);
    } else {
        detail::reset_profile(settings_);
    }
}

bool AnkiConfigStore::autofill_field_mappings() {
    if (!settings_.selected_note_type.has_value()) {
        return false;
    }
    const auto& models = settings_.available_note_types;
    const auto found = std::ranges::find(models, *settings_.selected_note_type, &NoteType::name);
    if (found == models.end()) {
        return false;
    }
    auto updated = autofill_mappings(found->name, found->fields, settings_.field_mappings);
    if (updated == settings_.field_mappings) {
        return false;
    }
    settings_.field_mappings = std::move(updated);
    return true;
}

AnkiSaveResult AnkiConfigStore::set_anki_connect_url(std::string url) {
    if (settings_.transport.has_value()) {
        settings_.transport->url = std::move(url);
    }
    return save();
}

AnkiSaveResult AnkiConfigStore::set_anki_connect_api_key(std::string api_key) {
    if (settings_.transport.has_value()) {
        settings_.transport->api_key = std::move(api_key);
    }
    return save();
}

AnkiSaveResult AnkiConfigStore::set_field_mapping(std::string field, std::string value) {
    if (trim_whitespace(value).empty()) {
        settings_.field_mappings[std::move(field)] = "";
    } else {
        settings_.field_mappings[std::move(field)] = std::move(value);
    }
    return save();
}

AnkiSaveResult AnkiConfigStore::set_tags(std::string tags) {
    settings_.tags = std::move(tags);
    return save();
}

std::optional<std::filesystem::path> AnkiConfigStore::active_profile_directory() const {
    const std::optional<std::filesystem::path> path =
        profile_config_path(active_profile_id_);
    if (!path.has_value()) {
        return std::nullopt;
    }
    return path->parent_path();
}

}  // namespace niratan::anki
