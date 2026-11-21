// SPDX-License-Identifier: GPL-3.0-or-later

#include "anki_config_codec.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <utility>

namespace niratan::anki::detail {
namespace {

template <typename E>
std::optional<E> parse_enum(const QJsonValue& value,
                            std::initializer_list<std::pair<std::string_view, E>> table) {
    if (!value.isString()) {
        return std::nullopt;
    }
    const std::string text = value.toString().toStdString();
    for (const auto& [name, entry] : table) {
        if (text == name) {
            return entry;
        }
    }
    return std::nullopt;
}

template <typename E>
std::string spell_enum(E value, std::initializer_list<std::pair<E, const char*>> table) {
    for (const auto& [entry, name] : table) {
        if (value == entry) {
            return name;
        }
    }
    return {};
}

std::optional<DuplicateScope> parse_scope(const QJsonValue& value) {
    return parse_enum<DuplicateScope>(value, {{"collection", DuplicateScope::collection},
                                              {"deck", DuplicateScope::deck},
                                              {"deckroot", DuplicateScope::deck_root}});
}

std::string spell(DuplicateScope value) {
    return spell_enum(value, {{DuplicateScope::collection, "collection"},
                              {DuplicateScope::deck, "deck"},
                              {DuplicateScope::deck_root, "deckroot"}});
}

std::optional<ImageCompressionFormat> parse_image_format(const QJsonValue& value) {
    return parse_enum<ImageCompressionFormat>(
        value, {{"jpeg", ImageCompressionFormat::jpeg}, {"avif", ImageCompressionFormat::avif}});
}

std::string spell(ImageCompressionFormat value) {
    return value == ImageCompressionFormat::avif ? "avif" : "jpeg";
}

std::optional<AudioCompressionFormat> parse_audio_format(const QJsonValue& value) {
    return parse_enum<AudioCompressionFormat>(
        value, {{"aac", AudioCompressionFormat::aac}, {"mp3", AudioCompressionFormat::mp3}});
}

std::string spell(AudioCompressionFormat value) {
    return value == AudioCompressionFormat::mp3 ? "mp3" : "aac";
}

// Synthesized Codable decodes an absent or null optional member as nil and
// fails on any other mismatch.
bool opt_string(const QJsonValue& value, std::optional<std::string>& out) {
    if (value.isUndefined() || value.isNull()) {
        out.reset();
        return true;
    }
    if (!value.isString()) {
        return false;
    }
    out = value.toString().toStdString();
    return true;
}

bool opt_bool(const QJsonValue& value, std::optional<bool>& out) {
    if (value.isUndefined() || value.isNull()) {
        out.reset();
        return true;
    }
    if (!value.isBool()) {
        return false;
    }
    out = value.toBool();
    return true;
}

bool req_bool(const QJsonValue& value, bool& out) {
    if (!value.isBool()) {
        return false;
    }
    out = value.toBool();
    return true;
}

bool opt_double(const QJsonValue& value, std::optional<double>& out) {
    if (value.isUndefined() || value.isNull()) {
        out.reset();
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    out = value.toDouble();
    return true;
}

// JSON numbers are doubles; Int members accept only integral values.
bool opt_int(const QJsonValue& value, std::optional<int>& out) {
    if (value.isUndefined() || value.isNull()) {
        out.reset();
        return true;
    }
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number != std::floor(number) ||
        number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(number);
    return true;
}

bool req_int(const QJsonValue& value, int& out) {
    std::optional<int> parsed;
    if (!opt_int(value, parsed) || !parsed.has_value()) {
        return false;
    }
    out = *parsed;
    return true;
}

template <typename E>
bool opt_enum(const QJsonValue& value, std::optional<E>& out,
              std::optional<E> (*parse)(const QJsonValue&)) {
    if (value.isUndefined() || value.isNull()) {
        out.reset();
        return true;
    }
    const std::optional<E> parsed = parse(value);
    if (!parsed.has_value()) {
        return false;
    }
    out = *parsed;
    return true;
}

bool decode_mappings(const QJsonValue& value,
                     std::map<std::string, std::string, std::less<>>& out) {
    if (!value.isObject()) {
        return false;
    }
    std::map<std::string, std::string, std::less<>> result;
    const QJsonObject object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!it.value().isString()) {
            return false;
        }
        result.emplace(it.key().toStdString(), it.value().toString().toStdString());
    }
    out = std::move(result);
    return true;
}

bool decode_strings(const QJsonValue& value, std::vector<std::string>& out) {
    if (!value.isArray()) {
        return false;
    }
    std::vector<std::string> result;
    for (const QJsonValue& entry : value.toArray()) {
        if (!entry.isString()) {
            return false;
        }
        result.push_back(entry.toString().toStdString());
    }
    out = std::move(result);
    return true;
}

bool decode_note_types(const QJsonValue& value, std::vector<NoteType>& out) {
    if (!value.isArray()) {
        return false;
    }
    std::vector<NoteType> result;
    for (const QJsonValue& entry : value.toArray()) {
        if (!entry.isObject()) {
            return false;
        }
        const QJsonObject object = entry.toObject();
        std::vector<std::string> fields;
        if (!object.value("name").isString() || !decode_strings(object.value("fields"), fields)) {
            return false;
        }
        result.push_back(NoteType{object.value("name").toString().toStdString(), std::move(fields)});
    }
    out = std::move(result);
    return true;
}

std::optional<TransportDocument> decode_transport(const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    TransportDocument transport;
    std::optional<bool> check_all_models;
    if (!opt_string(object.value("url"), transport.url) ||
        !req_int(object.value("timeout"), transport.timeout)) {
        return std::nullopt;
    }
    const std::optional<DuplicateScope> scope = parse_scope(object.value("duplicateScope"));
    if (!scope.has_value() || !opt_bool(object.value("checkAllModels"), check_all_models) ||
        !req_bool(object.value("forceSync"), transport.force_sync) ||
        !opt_string(object.value("apiKey"), transport.api_key)) {
        return std::nullopt;
    }
    transport.duplicate_scope = *scope;
    transport.check_all_models = check_all_models.value_or(false);
    return transport;
}

bool decode_compression(const QJsonObject& object, CompressionFields& out) {
    return opt_bool(object.value("compressVideoScreenshots"), out.compress_video_screenshots) &&
           opt_bool(object.value("compressImages"), out.compress_images) &&
           opt_double(object.value("imageCompressionQuality"), out.image_compression_quality) &&
           opt_enum(object.value("imageCompressionFormat"), out.image_compression_format,
                    parse_image_format) &&
           opt_int(object.value("animatedAVIFMaximumHeight"), out.animated_avif_maximum_height) &&
           opt_int(object.value("animatedAVIFFramesPerSecond"),
                   out.animated_avif_frames_per_second) &&
           opt_enum(object.value("audioCompressionFormat"), out.audio_compression_format,
                    parse_audio_format) &&
           opt_int(object.value("audioCompressionBitrateKbps"),
                   out.audio_compression_bitrate_kbps);
}

bool decode_common(const QJsonObject& object, CommonFields& fields) {
    return opt_string(object.value("selectedDeck"), fields.selected_deck) &&
           opt_string(object.value("selectedNoteType"), fields.selected_note_type) &&
           req_bool(object.value("allowDupes"), fields.allow_dupes) &&
           opt_bool(object.value("compactGlossaries"), fields.compact_glossaries) &&
           opt_bool(object.value("embedMedia"), fields.embed_media) &&
           decode_mappings(object.value("fieldMappings"), fields.field_mappings) &&
           opt_string(object.value("tags"), fields.tags) &&
           decode_compression(object, fields.compression);
}

void put_string(QJsonObject& object, const char* key, const std::optional<std::string>& value) {
    if (value.has_value()) {
        object.insert(QString::fromLatin1(key), QString::fromStdString(*value));
    }
}

void put_bool(QJsonObject& object, const char* key, const std::optional<bool>& value) {
    if (value.has_value()) {
        object.insert(QString::fromLatin1(key), *value);
    }
}

void put_int(QJsonObject& object, const char* key, const std::optional<int>& value) {
    if (value.has_value()) {
        object.insert(QString::fromLatin1(key), *value);
    }
}

QJsonObject encode_mappings(const std::map<std::string, std::string, std::less<>>& mappings) {
    QJsonObject object;
    for (const auto& [key, value] : mappings) {
        object.insert(QString::fromStdString(key), QString::fromStdString(value));
    }
    return object;
}

void encode_compression(QJsonObject& object, const CompressionFields& compression) {
    put_bool(object, "compressVideoScreenshots", compression.compress_video_screenshots);
    put_bool(object, "compressImages", compression.compress_images);
    if (compression.image_compression_quality.has_value()) {
        object.insert("imageCompressionQuality", *compression.image_compression_quality);
    }
    if (compression.image_compression_format.has_value()) {
        object.insert("imageCompressionFormat",
                      QString::fromStdString(spell(*compression.image_compression_format)));
    }
    put_int(object, "animatedAVIFMaximumHeight", compression.animated_avif_maximum_height);
    put_int(object, "animatedAVIFFramesPerSecond", compression.animated_avif_frames_per_second);
    if (compression.audio_compression_format.has_value()) {
        object.insert("audioCompressionFormat",
                      QString::fromStdString(spell(*compression.audio_compression_format)));
    }
    put_int(object, "audioCompressionBitrateKbps", compression.audio_compression_bitrate_kbps);
}

void encode_common(QJsonObject& object, const CommonFields& fields) {
    put_string(object, "selectedDeck", fields.selected_deck);
    put_string(object, "selectedNoteType", fields.selected_note_type);
    object.insert("allowDupes", fields.allow_dupes);
    put_bool(object, "compactGlossaries", fields.compact_glossaries);
    put_bool(object, "embedMedia", fields.embed_media);
    object.insert("fieldMappings", encode_mappings(fields.field_mappings));
    put_string(object, "tags", fields.tags);
    encode_compression(object, fields.compression);
}

double effective_quality(const CompressionFields& compression) {
    return std::min(0.95, std::max(0.40, compression.image_compression_quality.value_or(0.80)));
}

bool effective_compress_images(const CompressionFields& compression) {
    return compression.compress_images.value_or(
        compression.compress_video_screenshots.value_or(true));
}

int clamp_int(int value, int low, int high) {
    return std::min(high, std::max(low, value));
}

}  // namespace

std::optional<ProfileDocument> decode_profile(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    ProfileDocument profile;
    const std::optional<DuplicateScope> scope = parse_scope(object.value("duplicateScope"));
    if (!decode_common(object, profile) || !profile.compact_glossaries.has_value() ||
        !profile.embed_media.has_value() || !profile.tags.has_value() || !scope.has_value() ||
        !req_bool(object.value("checkAllModels"), profile.check_all_models)) {
        return std::nullopt;
    }
    profile.duplicate_scope = *scope;
    return profile;
}

std::optional<GlobalDocument> decode_global(const QJsonDocument& document) {
    if (!document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = document.object();
    GlobalDocument global;
    if (!decode_common(object, global) ||
        !decode_strings(object.value("availableDecks"), global.available_decks) ||
        !decode_note_types(object.value("availableNoteTypes"), global.available_note_types) ||
        !opt_bool(object.value("useAnkiConnect"), global.use_anki_connect)) {
        return std::nullopt;
    }
    const QJsonValue transport = object.value("ankiConnectConfig");
    if (!transport.isUndefined() && !transport.isNull()) {
        std::optional<TransportDocument> parsed = decode_transport(transport);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        global.anki_connect_config = std::move(parsed);
    }
    return global;
}

QJsonObject encode_profile(const ProfileDocument& profile) {
    QJsonObject object;
    encode_common(object, profile);
    object.insert("duplicateScope", QString::fromStdString(spell(profile.duplicate_scope)));
    object.insert("checkAllModels", profile.check_all_models);
    return object;
}

QJsonObject encode_global(const GlobalDocument& global) {
    QJsonObject object;
    encode_common(object, global);
    QJsonArray decks;
    for (const std::string& deck : global.available_decks) {
        decks.append(QString::fromStdString(deck));
    }
    object.insert("availableDecks", decks);
    QJsonArray note_types;
    for (const NoteType& note_type : global.available_note_types) {
        QJsonArray fields;
        for (const std::string& field : note_type.fields) {
            fields.append(QString::fromStdString(field));
        }
        note_types.append(
            QJsonObject{{"name", QString::fromStdString(note_type.name)}, {"fields", fields}});
    }
    object.insert("availableNoteTypes", note_types);
    put_bool(object, "useAnkiConnect", global.use_anki_connect);
    if (global.anki_connect_config.has_value()) {
        const TransportDocument& stored = *global.anki_connect_config;
        QJsonObject transport;
        put_string(transport, "url", stored.url);
        transport.insert("timeout", stored.timeout);
        transport.insert("duplicateScope", QString::fromStdString(spell(stored.duplicate_scope)));
        transport.insert("checkAllModels", stored.check_all_models);
        transport.insert("forceSync", stored.force_sync);
        put_string(transport, "apiKey", stored.api_key);
        object.insert("ankiConnectConfig", transport);
    }
    return object;
}

CompressionFields effective_compression(const CompressionFields& compression) {
    return {std::nullopt,
            effective_compress_images(compression),
            effective_quality(compression),
            compression.image_compression_format.value_or(ImageCompressionFormat::jpeg),
            clamp_int(compression.animated_avif_maximum_height.value_or(350), 100, 1080),
            clamp_int(compression.animated_avif_frames_per_second.value_or(10), 1, 30),
            compression.audio_compression_format.value_or(AudioCompressionFormat::aac),
            clamp_int(compression.audio_compression_bitrate_kbps.value_or(64), 32, 192)};
}

void apply_profile(const ProfileDocument& profile, AnkiSettings& settings) {
    settings.selected_deck = profile.selected_deck;
    settings.selected_note_type = profile.selected_note_type;
    settings.allow_dupes = profile.allow_dupes;
    settings.compact_glossaries = profile.compact_glossaries.value_or(false);
    settings.embed_media = profile.embed_media.value_or(false);
    settings.compress_images = effective_compress_images(profile.compression);
    settings.image_compression_quality = effective_quality(profile.compression);
    settings.image_compression_format =
        profile.compression.image_compression_format.value_or(ImageCompressionFormat::jpeg);
    settings.animated_avif_maximum_height = clamp_int(
        profile.compression.animated_avif_maximum_height.value_or(350), 100, 1080);
    settings.animated_avif_frames_per_second = clamp_int(
        profile.compression.animated_avif_frames_per_second.value_or(10), 1, 30);
    settings.audio_compression_format =
        profile.compression.audio_compression_format.value_or(AudioCompressionFormat::aac);
    settings.audio_compression_bitrate_kbps = clamp_int(
        profile.compression.audio_compression_bitrate_kbps.value_or(64), 32, 192);
    settings.field_mappings = profile.field_mappings;
    settings.tags = profile.tags.value_or("");
    if (settings.transport.has_value()) {
        settings.transport->duplicate_scope = profile.duplicate_scope;
        settings.transport->check_all_models = profile.check_all_models;
    }
}

void reset_profile(AnkiSettings& settings) {
    settings.selected_deck.reset();
    settings.selected_note_type.reset();
    settings.allow_dupes = false;
    settings.compact_glossaries = false;
    settings.embed_media = false;
    settings.compress_images = true;
    settings.image_compression_quality = 0.80;
    settings.image_compression_format = ImageCompressionFormat::jpeg;
    settings.animated_avif_maximum_height = 350;
    settings.animated_avif_frames_per_second = 10;
    settings.audio_compression_format = AudioCompressionFormat::aac;
    settings.audio_compression_bitrate_kbps = 64;
    settings.field_mappings.clear();
    settings.tags.clear();
    if (settings.transport.has_value()) {
        settings.transport->duplicate_scope = DuplicateScope::collection;
        settings.transport->check_all_models = false;
    }
}

}  // namespace niratan::anki::detail
