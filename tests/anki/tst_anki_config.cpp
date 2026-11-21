// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/anki_config.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/profiles/profile_index.h"
#include "services/profiles/profile_repository.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"

using niratan::anki::AnkiConfigStore;
using niratan::anki::AnkiSaveResult;
using niratan::anki::AnkiSettings;
using niratan::anki::AnkiTransportConfig;
using niratan::anki::AudioCompressionFormat;
using niratan::anki::DuplicateScope;
using niratan::anki::ImageCompressionFormat;
using niratan::anki::NoteType;
using niratan::profiles::ContentLanguage;
using niratan::profiles::ProfileRepository;

// Core/AnkiManager.swift (init, loadTransport, loadProfile, apply(_:),
// ensureAnkiConnectURL, save, makeLegacyConfig(from:), activateProfile and the
// setters), Models/Anki.swift (AnkiProfileConfig, AnkiConfig,
// AnkiConnectConfig, DuplicateScope and the format enums) and
// Core/ProfileRepository.swift (ankiConfigURL(for:), profileDirectory(for:),
// isSafeProfileID) define these cases, together with Decisions in
// notes/specs/anki-anki_config.md for the deprecated legacy migration, the
// persisted API key and the refusal to overwrite damaged JSON.
namespace {

constexpr std::string_view kDefaultProfileId = "default-ja";

class TempDataRoot {
   public:
    TempDataRoot() : previous_(qgetenv("XDG_DATA_HOME")) {
        qputenv("XDG_DATA_HOME", directory_.path().toUtf8());
    }
    ~TempDataRoot() {
        if (previous_.isEmpty()) {
            qunsetenv("XDG_DATA_HOME");
        } else {
            qputenv("XDG_DATA_HOME", previous_);
        }
    }
    TempDataRoot(const TempDataRoot&) = delete;
    TempDataRoot& operator=(const TempDataRoot&) = delete;

    bool valid() const { return directory_.isValid(); }
    std::filesystem::path root() const { return niratan::storage::data_root(); }

   private:
    QTemporaryDir directory_;
    QByteArray previous_;
};

std::filesystem::path profile_file(const std::filesystem::path& root, std::string_view profile_id) {
    return root / "Profiles" / std::string(profile_id) / "anki_config.json";
}

std::filesystem::path global_file(const std::filesystem::path& root) {
    return root / "anki_config.json";
}

void write_bytes(const std::filesystem::path& path, const QByteArray& bytes) {
    std::filesystem::create_directories(path.parent_path());
    QFile file(QString::fromStdString(path.string()));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(bytes);
    }
}

void write_json(const std::filesystem::path& path, const QJsonObject& object) {
    write_bytes(path, QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QByteArray read_bytes(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

std::optional<QJsonObject> read_json_object(const std::filesystem::path& path) {
    const niratan::storage::JsonReadResult read = niratan::storage::read_json_file(path);
    if (read.status != niratan::storage::JsonReadStatus::success ||
        !read.document.isObject()) {
        return std::nullopt;
    }
    return read.document.object();
}

QJsonObject profile_json() {
    return QJsonObject{{"selectedDeck", "Deck"},
                       {"selectedNoteType", "Basic"},
                       {"allowDupes", false},
                       {"compactGlossaries", false},
                       {"embedMedia", false},
                       {"fieldMappings", QJsonObject{}},
                       {"tags", ""},
                       {"duplicateScope", "collection"},
                       {"checkAllModels", false}};
}

QJsonObject transport_json() {
    return QJsonObject{{"url", "http://127.0.0.1:8765"},
                       {"timeout", 10},
                       {"duplicateScope", "collection"},
                       {"forceSync", false}};
}

QJsonObject global_json() {
    return QJsonObject{{"allowDupes", false},
                       {"fieldMappings", QJsonObject{}},
                       {"availableDecks", QJsonArray{}},
                       {"availableNoteTypes", QJsonArray{}}};
}

void expect_default_profile(const AnkiSettings& settings) {
    QVERIFY(!settings.selected_deck.has_value());
    QVERIFY(!settings.selected_note_type.has_value());
    QVERIFY(!settings.allow_dupes);
    QVERIFY(!settings.compact_glossaries);
    QVERIFY(!settings.embed_media);
    QVERIFY(settings.compress_images);
    QCOMPARE(settings.image_compression_quality, 0.80);
    QVERIFY(settings.image_compression_format == ImageCompressionFormat::jpeg);
    QCOMPARE(settings.animated_avif_maximum_height, 350);
    QCOMPARE(settings.animated_avif_frames_per_second, 10);
    QVERIFY(settings.audio_compression_format == AudioCompressionFormat::aac);
    QCOMPARE(settings.audio_compression_bitrate_kbps, 64);
    QVERIFY(settings.field_mappings.empty());
    QVERIFY(settings.tags.empty());
}

void expect_default_transport(const AnkiSettings& settings) {
    QVERIFY(settings.transport.has_value());
    const AnkiTransportConfig& transport = *settings.transport;
    QVERIFY(transport.url.has_value());
    QCOMPARE(*transport.url, std::string("http://127.0.0.1:8765"));
    QCOMPARE(transport.timeout, 10);
    QVERIFY(transport.duplicate_scope == DuplicateScope::collection);
    QVERIFY(!transport.check_all_models);
    QVERIFY(!transport.force_sync);
    QVERIFY(!transport.api_key.has_value());
}

std::string create_profile(ProfileRepository& profiles, const std::string& name) {
    const auto created = profiles.create_profile(
        name, ContentLanguage::english, std::nullopt);
    if (!created.profile.has_value()) {
        return {};
    }
    return created.profile->id;
}

}  // namespace

class AnkiConfigTest : public QObject {
    Q_OBJECT

   private slots:
    void profile_and_global_paths_follow_the_data_root();
    void unsafe_profile_ids_have_no_config_path();
    void a_missing_profile_file_loads_defaults_and_writes_nothing();
    void an_undecodable_profile_file_loads_defaults_and_writes_nothing();
    void a_profile_file_missing_a_required_member_loads_defaults_data();
    void a_profile_file_missing_a_required_member_loads_defaults();
    void an_unknown_enum_spelling_fails_the_profile_decode_data();
    void an_unknown_enum_spelling_fails_the_profile_decode();
    void an_unknown_enum_spelling_fails_the_global_decode();
    void compression_members_resolve_with_swift_fallbacks_data();
    void compression_members_resolve_with_swift_fallbacks();
    void compression_members_clamp_on_load_data();
    void compression_members_clamp_on_load();
    void the_profile_file_holds_only_profile_owned_state();
    void a_profile_file_loads_its_stored_members();
    void the_global_file_adds_transport_and_note_type_state();
    void a_save_omits_nil_optionals_and_never_writes_compress_video_screenshots();
    void a_save_persists_the_loaded_compression_values();
    void files_are_pretty_printed_with_sorted_keys_and_utf8();
    void the_transport_load_replaces_deck_lists_and_forces_transport_values();
    void transport_defaults_survive_a_global_file_without_a_transport_config();
    void a_blank_transport_url_becomes_the_default_url();
    void a_missing_transport_is_created_with_the_default_url();
    void profile_values_override_transport_values_after_the_load_sequence();
    void activating_a_profile_saves_the_outgoing_profile_and_loads_the_new_one();
    void activating_an_unknown_or_already_active_profile_writes_nothing();
    void activating_away_from_a_removed_profile_does_not_save_it();
    void a_save_writes_the_profile_file_before_the_global_file();
    void the_default_profile_save_writes_the_full_state();
    void a_non_default_save_merges_into_a_decodable_global_file();
    void a_non_default_save_merges_a_global_file_without_a_transport_config();
    void a_non_default_save_falls_back_to_the_default_profile_file();
    void a_non_default_save_uses_the_active_state_when_the_default_file_is_unreadable();
    void unknown_members_are_ignored_and_dropped();
    void field_mappings_pass_through_unvalidated();
    void setters_store_values_verbatim_and_save_at_once();
    void a_failed_profile_write_still_writes_the_global_file();
    void a_non_finite_compression_value_is_not_encoded();
    void a_damaged_profile_file_is_reported_and_left_untouched();
    void a_damaged_global_file_is_reported_and_left_untouched();
    void the_transport_api_key_survives_a_reload();
};

// B1/B2: Core/ProfileRepository.swift:ProfileRepository.ankiConfigURL(for:),
// Core/AnkiManager.swift:AnkiManager.loadTransport, BookStorage.getAppDirectory.
void AnkiConfigTest::profile_and_global_paths_follow_the_data_root() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QVERIFY(store.active_profile_id() == kDefaultProfileId);
    const std::optional<std::filesystem::path> path =
        store.profile_config_path(std::string(kDefaultProfileId));
    QVERIFY(path.has_value());
    QVERIFY(*path ==
            env.root() / "Profiles" / std::string(kDefaultProfileId) / "anki_config.json");
    QVERIFY(store.global_config_path() == env.root() / "anki_config.json");
    QVERIFY(store.global_config_path().parent_path() != env.root() / "Profiles");
}

// B30: Core/ProfileRepository.swift:ProfileRepository.isSafeProfileID,
// ankiConfigURL(for:), profileDirectory(for:).
void AnkiConfigTest::unsafe_profile_ids_have_no_config_path() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    for (const std::string_view id : {std::string_view(""), std::string_view("."),
                                      std::string_view(".."), std::string_view("a/b"),
                                      std::string_view("a\\b")}) {
        QVERIFY2(!store.profile_config_path(id).has_value(), id.data());
    }
    QVERIFY(store.profile_config_path("default-ja").has_value());
    QVERIFY(store.profile_config_path("profile-1").has_value());
}

// B11: Core/AnkiManager.swift:AnkiManager.loadProfile (missing-file branch).
void AnkiConfigTest::a_missing_profile_file_loads_defaults_and_writes_nothing() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    QVERIFY(!std::filesystem::exists(profile_file(env.root(), kDefaultProfileId)));

    store.load();

    expect_default_profile(store.settings());
    expect_default_transport(store.settings());
    QVERIFY(!std::filesystem::exists(profile_file(env.root(), kDefaultProfileId)));
    QVERIFY(!std::filesystem::exists(global_file(env.root())));
}

// B12, Decisions: Core/AnkiManager.swift:loadProfile (fallthrough branch). A
// legacy AnkiConfig-shaped per-profile file is undecodable in this port.
void AnkiConfigTest::an_undecodable_profile_file_loads_defaults_and_writes_nothing() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    const QJsonObject legacy{{"selectedDeck", "Deck"},
                             {"allowDupes", true},
                             {"fieldMappings", QJsonObject{}},
                             {"availableDecks", QJsonArray{"Deck"}},
                             {"availableNoteTypes", QJsonArray{}}};
    const std::filesystem::path path = profile_file(env.root(), kDefaultProfileId);
    write_json(path, legacy);
    const QByteArray before = read_bytes(path);

    store.load();

    expect_default_profile(store.settings());
    QCOMPARE(read_bytes(path), before);
    QVERIFY(!std::filesystem::exists(global_file(env.root())));
}

// B13: Models/Anki.swift:AnkiProfileConfig, AnkiManager.loadProfile. Every
// required member missing makes the whole file undecodable.
void AnkiConfigTest::a_profile_file_missing_a_required_member_loads_defaults_data() {
    QTest::addColumn<QString>("member");
    for (const char* member : {"allowDupes", "compactGlossaries", "embedMedia",
                               "fieldMappings", "tags", "duplicateScope",
                               "checkAllModels"}) {
        QTest::newRow(member) << QString::fromLatin1(member);
    }
}

void AnkiConfigTest::a_profile_file_missing_a_required_member_loads_defaults() {
    QFETCH(QString, member);
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject object = profile_json();
    object.remove(member);
    const std::filesystem::path path = profile_file(env.root(), kDefaultProfileId);
    write_json(path, object);

    store.load();

    expect_default_profile(store.settings());
}

// B7: Models/Anki.swift:DuplicateScope, AnkiImageCompressionFormat,
// AnkiAudioCompressionFormat. Unknown raw values fail the whole decode.
void AnkiConfigTest::an_unknown_enum_spelling_fails_the_profile_decode_data() {
    QTest::addColumn<QString>("member");
    QTest::addColumn<QString>("spelling");
    QTest::newRow("scope") << "duplicateScope" << "everything";
    QTest::newRow("image") << "imageCompressionFormat" << "png";
    QTest::newRow("audio") << "audioCompressionFormat" << "flac";
}

void AnkiConfigTest::an_unknown_enum_spelling_fails_the_profile_decode() {
    QFETCH(QString, member);
    QFETCH(QString, spelling);
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject object = profile_json();
    object[member] = spelling;
    write_json(profile_file(env.root(), kDefaultProfileId), object);

    store.load();

    expect_default_profile(store.settings());
}

void AnkiConfigTest::an_unknown_enum_spelling_fails_the_global_decode() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject transport = transport_json();
    transport["duplicateScope"] = "everything";
    QJsonObject object = global_json();
    object["availableDecks"] = QJsonArray{"Deck"};
    object["ankiConnectConfig"] = transport;
    write_json(global_file(env.root()), object);

    store.load();

    // The undecodable transport leaves the deck lists untouched, while the
    // missing profile file still resets the profile state.
    QVERIFY(store.settings().available_decks.empty());
    expect_default_transport(store.settings());
    expect_default_profile(store.settings());
}

// B14: Models/Anki.swift:AnkiProfileConfig.effectiveCompressImages.
void AnkiConfigTest::compression_members_resolve_with_swift_fallbacks_data() {
    QTest::addColumn<QString>("compress_images");
    QTest::addColumn<QString>("compress_video_screenshots");
    QTest::addColumn<bool>("expected");
    QTest::newRow("both absent") << "absent" << "absent" << true;
    QTest::newRow("video false") << "absent" << "false" << false;
    QTest::newRow("video true") << "absent" << "true" << true;
    QTest::newRow("images false wins") << "false" << "true" << false;
    QTest::newRow("images true wins") << "true" << "false" << true;
}

void AnkiConfigTest::compression_members_resolve_with_swift_fallbacks() {
    QFETCH(QString, compress_images);
    QFETCH(QString, compress_video_screenshots);
    QFETCH(bool, expected);
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject object = profile_json();
    if (compress_images != "absent") {
        object["compressImages"] = compress_images == "true";
    }
    if (compress_video_screenshots != "absent") {
        object["compressVideoScreenshots"] = compress_video_screenshots == "true";
    }
    write_json(profile_file(env.root(), kDefaultProfileId), object);

    store.load();

    QCOMPARE(store.settings().compress_images, expected);
    QCOMPARE(store.settings().image_compression_quality, 0.80);
}

// B14: AnkiProfileConfig.effective*, AnkiAnimatedAVIFConfiguration.clamped*.
void AnkiConfigTest::compression_members_clamp_on_load_data() {
    QTest::addColumn<QString>("member");
    QTest::addColumn<double>("value");
    QTest::addColumn<double>("expected");
    QTest::newRow("quality low") << "imageCompressionQuality" << 0.1 << 0.40;
    QTest::newRow("quality high") << "imageCompressionQuality" << 1.0 << 0.95;
    QTest::newRow("bitrate low") << "audioCompressionBitrateKbps" << 1.0 << 32.0;
    QTest::newRow("bitrate high") << "audioCompressionBitrateKbps" << 1000.0 << 192.0;
    QTest::newRow("height low") << "animatedAVIFMaximumHeight" << 0.0 << 100.0;
    QTest::newRow("height high") << "animatedAVIFMaximumHeight" << 2000.0 << 1080.0;
    QTest::newRow("fps low") << "animatedAVIFFramesPerSecond" << 0.0 << 1.0;
    QTest::newRow("fps high") << "animatedAVIFFramesPerSecond" << 99.0 << 30.0;
}

void AnkiConfigTest::compression_members_clamp_on_load() {
    QFETCH(QString, member);
    QFETCH(double, value);
    QFETCH(double, expected);
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject object = profile_json();
    object[member] = value;
    write_json(profile_file(env.root(), kDefaultProfileId), object);

    store.load();

    const AnkiSettings& settings = store.settings();
    if (member == "imageCompressionQuality") {
        QCOMPARE(settings.image_compression_quality, expected);
    } else if (member == "audioCompressionBitrateKbps") {
        QCOMPARE(settings.audio_compression_bitrate_kbps, static_cast<int>(expected));
    } else if (member == "animatedAVIFMaximumHeight") {
        QCOMPARE(settings.animated_avif_maximum_height, static_cast<int>(expected));
    } else {
        QCOMPARE(settings.animated_avif_frames_per_second, static_cast<int>(expected));
    }
}

// B3/B5: Core/AnkiManager.swift:AnkiManager.save (the AnkiProfileConfig
// projection) and Models/Anki.swift:AnkiProfileConfig members.
void AnkiConfigTest::the_profile_file_holds_only_profile_owned_state() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();

    AnkiSettings& settings = store.settings();
    QVERIFY(settings.transport.has_value());
    settings.selected_deck = "Deck";
    settings.selected_note_type = "Basic";
    settings.allow_dupes = true;
    settings.compact_glossaries = true;
    settings.embed_media = true;
    settings.field_mappings = {{"Front", "{expression}"}, {"Empty", ""}};
    settings.tags = "tag1 tag2";
    settings.transport->duplicate_scope = DuplicateScope::deck_root;
    settings.transport->check_all_models = true;
    settings.compress_images = false;
    settings.image_compression_quality = 0.5;
    settings.image_compression_format = ImageCompressionFormat::avif;
    settings.animated_avif_maximum_height = 400;
    settings.animated_avif_frames_per_second = 12;
    settings.audio_compression_format = AudioCompressionFormat::mp3;
    settings.audio_compression_bitrate_kbps = 96;

    const AnkiSaveResult result = store.save();
    QVERIFY2(result.ok, result.error.c_str());

    const std::optional<QJsonObject> object =
        read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(object.has_value());
    const QStringList keys = object->keys();
    const QStringList expected{
        "allowDupes",       "animatedAVIFFramesPerSecond", "animatedAVIFMaximumHeight",
        "audioCompressionBitrateKbps", "audioCompressionFormat", "checkAllModels",
        "compactGlossaries", "compressImages", "duplicateScope", "embedMedia",
        "fieldMappings",    "imageCompressionFormat",    "imageCompressionQuality",
        "selectedDeck",     "selectedNoteType",          "tags"};
    QCOMPARE(keys, expected);
    QVERIFY((*object)["selectedDeck"].isString());
    QCOMPARE((*object)["selectedDeck"].toString(), QString("Deck"));
    QVERIFY((*object)["allowDupes"].isBool());
    QVERIFY((*object)["fieldMappings"].isObject());
    QVERIFY((*object)["tags"].isString());
    QVERIFY((*object)["duplicateScope"].isString());
    QCOMPARE((*object)["duplicateScope"].toString(), QString("deckroot"));
    QVERIFY((*object)["checkAllModels"].isBool());
    QVERIFY((*object)["compressImages"].isBool());
    QVERIFY((*object)["imageCompressionQuality"].isDouble());
    QVERIFY((*object)["imageCompressionFormat"].isString());
    QCOMPARE((*object)["imageCompressionFormat"].toString(), QString("avif"));
    QVERIFY((*object)["animatedAVIFMaximumHeight"].isDouble());
    QVERIFY((*object)["audioCompressionFormat"].isString());
    QCOMPARE((*object)["audioCompressionFormat"].toString(), QString("mp3"));
}

// B5: Models/Anki.swift:AnkiProfileConfig and AnkiManager.apply(_:). Every
// member loads with its stored value and type.
void AnkiConfigTest::a_profile_file_loads_its_stored_members() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject object = profile_json();
    object["selectedDeck"] = "Mining";
    object["selectedNoteType"] = "Japanese";
    object["allowDupes"] = true;
    object["compactGlossaries"] = true;
    object["embedMedia"] = true;
    object["fieldMappings"] = QJsonObject{{"Expression", "{expression}"}};
    object["tags"] = "immersion";
    object["duplicateScope"] = "deckroot";
    object["checkAllModels"] = true;
    object["compressImages"] = false;
    object["imageCompressionQuality"] = 0.9;
    object["imageCompressionFormat"] = "avif";
    object["animatedAVIFMaximumHeight"] = 500;
    object["animatedAVIFFramesPerSecond"] = 20;
    object["audioCompressionFormat"] = "mp3";
    object["audioCompressionBitrateKbps"] = 128;
    write_json(profile_file(env.root(), kDefaultProfileId), object);

    store.load();

    const AnkiSettings& settings = store.settings();
    QCOMPARE(settings.selected_deck.value_or(""), std::string("Mining"));
    QCOMPARE(settings.selected_note_type.value_or(""), std::string("Japanese"));
    QVERIFY(settings.allow_dupes);
    QVERIFY(settings.compact_glossaries);
    QVERIFY(settings.embed_media);
    QCOMPARE(settings.field_mappings.at("Expression"), std::string("{expression}"));
    QCOMPARE(settings.tags, std::string("immersion"));
    QVERIFY(settings.transport.has_value());
    QVERIFY(settings.transport->duplicate_scope == DuplicateScope::deck_root);
    QVERIFY(settings.transport->check_all_models);
    QVERIFY(!settings.compress_images);
    QCOMPARE(settings.image_compression_quality, 0.9);
    QVERIFY(settings.image_compression_format == ImageCompressionFormat::avif);
    QCOMPARE(settings.animated_avif_maximum_height, 500);
    QCOMPARE(settings.animated_avif_frames_per_second, 20);
    QVERIFY(settings.audio_compression_format == AudioCompressionFormat::mp3);
    QCOMPARE(settings.audio_compression_bitrate_kbps, 128);
}

// B4/B6: Models/Anki.swift:AnkiConfig, AnkiNoteType, AnkiConnectConfig and
// AnkiManager.makeLegacyConfig(from:).
void AnkiConfigTest::the_global_file_adds_transport_and_note_type_state() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();

    AnkiSettings& settings = store.settings();
    QVERIFY(settings.transport.has_value());
    settings.selected_deck = "Deck";
    settings.selected_note_type = "Basic";
    settings.available_decks = {"Deck A", "Deck B"};
    settings.available_note_types = {NoteType{"Basic", {"Front", "Back"}}};
    settings.transport->url = "http://remote:9999";
    settings.transport->timeout = 7;
    settings.transport->duplicate_scope = DuplicateScope::deck;
    settings.transport->check_all_models = true;
    settings.transport->force_sync = true;
    settings.transport->api_key = "secret";

    QVERIFY(store.save().ok);

    const std::optional<QJsonObject> object = read_json_object(global_file(env.root()));
    QVERIFY(object.has_value());
    const QStringList expected{
        "allowDupes",       "animatedAVIFFramesPerSecond", "animatedAVIFMaximumHeight",
        "ankiConnectConfig", "audioCompressionBitrateKbps", "audioCompressionFormat",
        "availableDecks",   "availableNoteTypes",          "compactGlossaries",
        "compressImages",   "embedMedia",                  "fieldMappings",
        "imageCompressionFormat", "imageCompressionQuality", "selectedDeck",
        "selectedNoteType", "tags",                        "useAnkiConnect"};
    QCOMPARE(object->keys(), expected);
    QVERIFY(!object->contains("duplicateScope"));
    QVERIFY(!object->contains("checkAllModels"));
    QCOMPARE((*object)["selectedDeck"].toString(), QString("Deck"));
    QVERIFY((*object)["availableDecks"].isArray());
    QCOMPARE((*object)["availableDecks"].toArray(), QJsonArray({"Deck A", "Deck B"}));
    QVERIFY((*object)["availableNoteTypes"].isArray());
    const QJsonArray note_types = (*object)["availableNoteTypes"].toArray();
    QCOMPARE(note_types.size(), 1);
    QVERIFY(note_types.at(0).isObject());
    QCOMPARE(note_types.at(0).toObject()["name"].toString(), QString("Basic"));
    QCOMPARE(note_types.at(0).toObject()["fields"].toArray(), QJsonArray({"Front", "Back"}));
    QVERIFY((*object)["useAnkiConnect"].isBool());
    QVERIFY((*object)["useAnkiConnect"].toBool());
    QVERIFY((*object)["ankiConnectConfig"].isObject());
    const QJsonObject transport = (*object)["ankiConnectConfig"].toObject();
    QCOMPARE(transport["url"].toString(), QString("http://remote:9999"));
    QVERIFY(transport["timeout"].isDouble());
    QCOMPARE(transport["timeout"].toInt(), 7);
    QCOMPARE(transport["duplicateScope"].toString(), QString("deck"));
    QVERIFY(transport["checkAllModels"].isBool());
    QVERIFY(transport["checkAllModels"].toBool());
    QVERIFY(transport["forceSync"].isBool());
    QVERIFY(transport["forceSync"].toBool());
    QCOMPARE(transport["apiKey"].toString(), QString("secret"));
}

// B8: AnkiManager.save (compressVideoScreenshots nil) and synthesized Encodable.
void AnkiConfigTest::a_save_omits_nil_optionals_and_never_writes_compress_video_screenshots() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();
    QVERIFY(store.save().ok);

    for (const std::filesystem::path& path :
         {profile_file(env.root(), kDefaultProfileId), global_file(env.root())}) {
        const std::optional<QJsonObject> object = read_json_object(path);
        QVERIFY2(object.has_value(), path.string().c_str());
        QVERIFY(!object->contains("selectedDeck"));
        QVERIFY(!object->contains("selectedNoteType"));
        QVERIFY(!object->contains("compressVideoScreenshots"));
        for (auto it = object->begin(); it != object->end(); ++it) {
            QVERIFY2(!it.value().isNull(), it.key().toUtf8().constData());
        }
    }
}

// B9: AnkiManager.save writes the values loaded through the effective getters.
void AnkiConfigTest::a_save_persists_the_loaded_compression_values() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject object = profile_json();
    object["compressVideoScreenshots"] = false;
    object["imageCompressionQuality"] = 2.0;
    object["audioCompressionBitrateKbps"] = 1000;
    object["animatedAVIFMaximumHeight"] = 5;
    object["animatedAVIFFramesPerSecond"] = 99;
    write_json(profile_file(env.root(), kDefaultProfileId), object);

    store.load();
    QVERIFY(!store.settings().compress_images);
    QCOMPARE(store.settings().image_compression_quality, 0.95);
    QVERIFY(store.save().ok);

    const std::optional<QJsonObject> saved =
        read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(saved.has_value());
    QVERIFY((*saved)["compressImages"].isBool());
    QVERIFY(!(*saved)["compressImages"].toBool());
    QCOMPARE((*saved)["imageCompressionQuality"].toDouble(), 0.95);
    QCOMPARE((*saved)["imageCompressionFormat"].toString(), QString("jpeg"));
    QCOMPARE((*saved)["animatedAVIFMaximumHeight"].toInt(), 100);
    QCOMPARE((*saved)["animatedAVIFFramesPerSecond"].toInt(), 30);
    QCOMPARE((*saved)["audioCompressionFormat"].toString(), QString("aac"));
    QCOMPARE((*saved)["audioCompressionBitrateKbps"].toInt(), 192);
    QVERIFY(!saved->contains("compressVideoScreenshots"));
}

// B10: AnkiManager.save (prettyPrinted, sortedKeys, atomic) through
// storage::write_json_file.
void AnkiConfigTest::files_are_pretty_printed_with_sorted_keys_and_utf8() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();
    store.settings().tags = "日本語";
    store.settings().field_mappings = {{"表層形", "{expression}"}};
    QVERIFY(store.save().ok);

    const std::filesystem::path path = profile_file(env.root(), kDefaultProfileId);
    const QByteArray text = read_bytes(path);
    QVERIFY(!text.isEmpty());
    QVERIFY(text.contains('\n'));
    QVERIFY(text.startsWith('{'));
    QVERIFY(text.trimmed().endsWith('}'));
    const int dupes = text.indexOf("\"allowDupes\"");
    const int check = text.indexOf("\"checkAllModels\"");
    const int tags = text.indexOf("\"tags\"");
    QVERIFY(dupes >= 0);
    QVERIFY(check > dupes);
    QVERIFY(tags > check);
    QVERIFY(text.contains(QString("表層形").toUtf8()));
    QVERIFY(text.contains(QString("日本語").toUtf8()));

    const std::optional<QJsonObject> object = read_json_object(path);
    QVERIFY(object.has_value());
    QCOMPARE((*object)["fieldMappings"].toObject()["表層形"].toString(),
             QString("{expression}"));
    QCOMPARE((*object)["tags"].toString(), QString("日本語"));
}

// B17, Decisions: AnkiManager.loadTransport. The deck lists are always
// replaced, the transport keeps url/timeout/forceSync and, unlike Swift, the
// API key; duplicateScope and checkAllModels are forced.
void AnkiConfigTest::the_transport_load_replaces_deck_lists_and_forces_transport_values() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.settings().available_decks = {"Prefilled"};

    QJsonObject transport = transport_json();
    transport["url"] = "http://remote:9999";
    transport["timeout"] = 30;
    transport["duplicateScope"] = "deck";
    transport["checkAllModels"] = true;
    transport["forceSync"] = true;
    transport["apiKey"] = "stored-key";
    QJsonObject object = global_json();
    object["selectedDeck"] = "GlobalDeck";
    object["allowDupes"] = true;
    object["fieldMappings"] = QJsonObject{{"Global", "value"}};
    object["tags"] = "global";
    object["availableDecks"] = QJsonArray{"Deck A"};
    object["availableNoteTypes"] =
        QJsonArray{QJsonObject{{"name", "Basic"}, {"fields", QJsonArray{"Front"}}}};
    object["ankiConnectConfig"] = transport;
    write_json(global_file(env.root()), object);

    store.load();

    const AnkiSettings& settings = store.settings();
    QCOMPARE(settings.available_decks, std::vector<std::string>{"Deck A"});
    QVERIFY((settings.available_note_types ==
             std::vector<NoteType>{NoteType{"Basic", {"Front"}}}));
    QVERIFY(settings.transport.has_value());
    QCOMPARE(*settings.transport->url, std::string("http://remote:9999"));
    QCOMPARE(settings.transport->timeout, 30);
    QVERIFY(settings.transport->force_sync);
    QVERIFY(settings.transport->duplicate_scope == DuplicateScope::collection);
    QVERIFY(!settings.transport->check_all_models);
    QVERIFY(settings.transport->api_key.has_value());
    QCOMPARE(*settings.transport->api_key, std::string("stored-key"));
    // The global file's profile-owned members are not applied.
    QVERIFY(!settings.selected_deck.has_value());
    QVERIFY(!settings.allow_dupes);
    QVERIFY(settings.field_mappings.empty());
    QVERIFY(settings.tags.empty());
}

// B18: AnkiManager.ankiConnectConfig initial value, loadTransport.
void AnkiConfigTest::transport_defaults_survive_a_global_file_without_a_transport_config() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    expect_default_transport(store.settings());

    QJsonObject object = global_json();
    object["availableDecks"] = QJsonArray{"Deck"};
    write_json(global_file(env.root()), object);

    store.load();

    QCOMPARE(store.settings().available_decks, std::vector<std::string>{"Deck"});
    expect_default_transport(store.settings());
}

// B19: AnkiManager.ensureAnkiConnectURL (blank URL branch).
void AnkiConfigTest::a_blank_transport_url_becomes_the_default_url() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject transport = transport_json();
    transport["url"] = " \t\n ";
    QJsonObject object = global_json();
    object["ankiConnectConfig"] = transport;
    write_json(global_file(env.root()), object);

    store.load();

    QVERIFY(store.settings().transport.has_value());
    QCOMPARE(*store.settings().transport->url, std::string("http://127.0.0.1:8765"));
    QCOMPARE(store.settings().transport->timeout, 10);

    // A null URL takes the same branch as a blank one.
    QJsonObject null_url = global_json();
    QJsonObject null_transport = transport_json();
    null_transport["url"] = QJsonValue::Null;
    null_url["ankiConnectConfig"] = null_transport;
    write_json(global_file(env.root()), null_url);

    store.load();

    QVERIFY(store.settings().transport.has_value());
    QCOMPARE(*store.settings().transport->url, std::string("http://127.0.0.1:8765"));
}

// B19: AnkiManager.ensureAnkiConnectURL (nil config branch).
void AnkiConfigTest::a_missing_transport_is_created_with_the_default_url() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.settings().transport.reset();

    store.load();

    expect_default_transport(store.settings());
}

// B20: AnkiManager.init, loadProfile, apply(_:). The profile's duplicate
// settings overwrite the transport load's forced values.
void AnkiConfigTest::profile_values_override_transport_values_after_the_load_sequence() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject transport = transport_json();
    transport["duplicateScope"] = "deck";
    transport["checkAllModels"] = false;
    transport["url"] = "http://global:5000";
    transport["timeout"] = 5;
    transport["forceSync"] = true;
    QJsonObject global = global_json();
    global["ankiConnectConfig"] = transport;
    write_json(global_file(env.root()), global);

    QJsonObject profile = profile_json();
    profile["duplicateScope"] = "deckroot";
    profile["checkAllModels"] = true;
    profile["selectedDeck"] = "ProfileDeck";
    write_json(profile_file(env.root(), kDefaultProfileId), profile);

    store.load();

    const AnkiSettings& settings = store.settings();
    QCOMPARE(settings.selected_deck.value_or(""), std::string("ProfileDeck"));
    QVERIFY(settings.transport.has_value());
    QVERIFY(settings.transport->duplicate_scope == DuplicateScope::deck_root);
    QVERIFY(settings.transport->check_all_models);
    QCOMPARE(*settings.transport->url, std::string("http://global:5000"));
    QCOMPARE(settings.transport->timeout, 5);
    QVERIFY(settings.transport->force_sync);
}

// B21: AnkiManager.activateProfile.
void AnkiConfigTest::activating_a_profile_saves_the_outgoing_profile_and_loads_the_new_one() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();
    store.settings().selected_deck = "DefaultDeck";

    const std::string second = create_profile(profiles, "Second");
    QVERIFY(!second.empty());

    store.activate_profile(second);

    QVERIFY(store.active_profile_id() == second);
    expect_default_profile(store.settings());
    const std::optional<QJsonObject> outgoing =
        read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(outgoing.has_value());
    QCOMPARE((*outgoing)["selectedDeck"].toString(), QString("DefaultDeck"));
    QVERIFY(!std::filesystem::exists(profile_file(env.root(), second)));
}

// B21: AnkiManager.activateProfile guards.
void AnkiConfigTest::activating_an_unknown_or_already_active_profile_writes_nothing() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();

    store.activate_profile("missing");
    QVERIFY(store.active_profile_id() == kDefaultProfileId);
    QVERIFY(!std::filesystem::exists(profile_file(env.root(), kDefaultProfileId)));
    QVERIFY(!std::filesystem::exists(global_file(env.root())));

    const std::string second = create_profile(profiles, "Second");
    QVERIFY(!second.empty());
    store.activate_profile(second);
    QVERIFY(store.active_profile_id() == second);
    const QByteArray global_before = read_bytes(global_file(env.root()));

    store.settings().selected_deck = "Unsaved";
    store.activate_profile(second);

    QVERIFY(!std::filesystem::exists(profile_file(env.root(), second)));
    QCOMPARE(read_bytes(global_file(env.root())), global_before);
}

// B21: AnkiManager.activateProfile only saves the outgoing profile while
// ProfileRepository still knows it.
void AnkiConfigTest::activating_away_from_a_removed_profile_does_not_save_it() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    const std::string second = create_profile(profiles, "Second");
    QVERIFY(!second.empty());
    QVERIFY(!profiles.set_active_profile(second).has_value());
    AnkiConfigStore store(env.root(), profiles);
    store.load();
    QVERIFY(store.active_profile_id() == second);
    store.settings().selected_deck = "SecondDeck";

    QVERIFY(!profiles.delete_profile(second).has_value());

    store.activate_profile(std::string(kDefaultProfileId));

    QVERIFY(store.active_profile_id() == kDefaultProfileId);
    QVERIFY(!std::filesystem::exists(profile_file(env.root(), second)));
    QVERIFY(!std::filesystem::exists(profile_file(env.root(), kDefaultProfileId)));
    QVERIFY(!std::filesystem::exists(global_file(env.root())));
}

// B22: AnkiManager.save writes the profile file first.
void AnkiConfigTest::a_save_writes_the_profile_file_before_the_global_file() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();
    store.settings().selected_deck = "Deck";

    // A directory at the global path makes the global write fail.
    std::filesystem::create_directory(global_file(env.root()));

    const AnkiSaveResult result = store.save();

    QVERIFY(!result.ok);
    QVERIFY(!result.error.empty());
    const std::optional<QJsonObject> profile =
        read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(profile.has_value());
    QCOMPARE((*profile)["selectedDeck"].toString(), QString("Deck"));
}

// B22/B25: AnkiManager.save (default branch) and makeLegacyConfig(from:).
void AnkiConfigTest::the_default_profile_save_writes_the_full_state() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();

    AnkiSettings& settings = store.settings();
    QVERIFY(settings.transport.has_value());
    settings.selected_deck = "Deck";
    settings.selected_note_type = "Basic";
    settings.allow_dupes = true;
    settings.field_mappings = {{"Front", "{expression}"}};
    settings.tags = "tags";
    settings.transport->url = "http://transport:8765";
    settings.transport->timeout = 12;
    settings.transport->force_sync = true;
    settings.transport->api_key = "key";

    QVERIFY(store.save().ok);

    const std::optional<QJsonObject> profile =
        read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(profile.has_value());
    const std::optional<QJsonObject> global = read_json_object(global_file(env.root()));
    QVERIFY(global.has_value());
    QCOMPARE((*global)["selectedDeck"].toString(), QString("Deck"));
    QCOMPARE((*global)["selectedNoteType"].toString(), QString("Basic"));
    QVERIFY((*global)["allowDupes"].toBool());
    QCOMPARE((*global)["fieldMappings"].toObject()["Front"].toString(),
             QString("{expression}"));
    QCOMPARE((*global)["tags"].toString(), QString("tags"));
    QVERIFY((*global)["useAnkiConnect"].isBool());
    QVERIFY((*global)["useAnkiConnect"].toBool());
    const QJsonObject transport = (*global)["ankiConnectConfig"].toObject();
    QCOMPARE(transport["url"].toString(), QString("http://transport:8765"));
    QCOMPARE(transport["timeout"].toInt(), 12);
    QVERIFY(transport["forceSync"].toBool());
    QCOMPARE(transport["apiKey"].toString(), QString("key"));
}

// B23, Decisions: Models/Anki.swift:AnkiConfig.updatingSharedState and
// AnkiManager.save. The global file keeps its profile state and duplicate
// settings; the lists and transport fields come from memory. The file's
// useAnkiConnect is preserved as Swift does, not forced true.
void AnkiConfigTest::a_non_default_save_merges_into_a_decodable_global_file() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    const std::string second = create_profile(profiles, "Second");
    QVERIFY(!second.empty());
    QVERIFY(!profiles.set_active_profile(second).has_value());

    QJsonObject existing_transport = transport_json();
    existing_transport["url"] = "http://old:1";
    existing_transport["timeout"] = 3;
    existing_transport["duplicateScope"] = "deck";
    existing_transport["checkAllModels"] = false;
    existing_transport["forceSync"] = true;
    existing_transport["apiKey"] = "old-key";
    QJsonObject existing = global_json();
    existing["selectedDeck"] = "GlobalDeck";
    existing["allowDupes"] = true;
    existing["fieldMappings"] = QJsonObject{{"Global", "x"}};
    existing["tags"] = "global-tags";
    existing["compressImages"] = false;
    existing["availableDecks"] = QJsonArray{"OldDeck"};
    existing["useAnkiConnect"] = false;
    existing["ankiConnectConfig"] = existing_transport;
    write_json(global_file(env.root()), existing);

    AnkiConfigStore store(env.root(), profiles);
    store.load();

    AnkiSettings& settings = store.settings();
    QVERIFY(settings.transport.has_value());
    settings.selected_deck = "SecondDeck";
    settings.available_decks = {"NewDeck"};
    settings.available_note_types = {NoteType{"Basic", {"Front"}}};
    settings.transport->url = "http://new:2";
    settings.transport->timeout = 9;
    settings.transport->force_sync = false;
    settings.transport->api_key = "new-key";
    settings.transport->duplicate_scope = DuplicateScope::deck_root;
    settings.transport->check_all_models = true;

    QVERIFY(store.save().ok);

    const std::optional<QJsonObject> global = read_json_object(global_file(env.root()));
    QVERIFY(global.has_value());
    QCOMPARE((*global)["selectedDeck"].toString(), QString("GlobalDeck"));
    QVERIFY((*global)["allowDupes"].toBool());
    QCOMPARE((*global)["fieldMappings"].toObject()["Global"].toString(), QString("x"));
    QCOMPARE((*global)["tags"].toString(), QString("global-tags"));
    QVERIFY(!(*global)["compressImages"].toBool());
    QCOMPARE((*global)["availableDecks"].toArray(), QJsonArray({"NewDeck"}));
    QVERIFY(!(*global)["useAnkiConnect"].toBool());
    const QJsonObject transport = (*global)["ankiConnectConfig"].toObject();
    QCOMPARE(transport["url"].toString(), QString("http://new:2"));
    QCOMPARE(transport["timeout"].toInt(), 9);
    QVERIFY(!transport["forceSync"].toBool());
    QCOMPARE(transport["apiKey"].toString(), QString("new-key"));
    QCOMPARE(transport["duplicateScope"].toString(), QString("deck"));
    QVERIFY(!transport["checkAllModels"].toBool());

    // The profile file is the active profile's state.
    const std::optional<QJsonObject> profile = read_json_object(profile_file(env.root(), second));
    QVERIFY(profile.has_value());
    QCOMPARE((*profile)["selectedDeck"].toString(), QString("SecondDeck"));
}

// B23, Decisions: an existing global file without ankiConnectConfig keeps
// collection/false for the merged transport, as AnkiConfig.updatingSharedState
// does.
void AnkiConfigTest::a_non_default_save_merges_a_global_file_without_a_transport_config() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    const std::string second = create_profile(profiles, "Second");
    QVERIFY(!second.empty());
    QVERIFY(!profiles.set_active_profile(second).has_value());

    QJsonObject existing = global_json();
    existing["selectedDeck"] = "GlobalDeck";
    write_json(global_file(env.root()), existing);

    AnkiConfigStore store(env.root(), profiles);
    store.load();

    AnkiSettings& settings = store.settings();
    QVERIFY(settings.transport.has_value());
    settings.transport->url = "http://new:3";
    settings.transport->timeout = 4;
    settings.transport->force_sync = true;
    settings.transport->api_key = "key";
    settings.transport->duplicate_scope = DuplicateScope::deck_root;
    settings.transport->check_all_models = true;

    QVERIFY(store.save().ok);

    const std::optional<QJsonObject> global = read_json_object(global_file(env.root()));
    QVERIFY(global.has_value());
    QCOMPARE((*global)["selectedDeck"].toString(), QString("GlobalDeck"));
    QVERIFY((*global)["ankiConnectConfig"].isObject());
    const QJsonObject transport = (*global)["ankiConnectConfig"].toObject();
    QCOMPARE(transport["url"].toString(), QString("http://new:3"));
    QCOMPARE(transport["timeout"].toInt(), 4);
    QVERIFY(transport["forceSync"].toBool());
    QCOMPARE(transport["apiKey"].toString(), QString("key"));
    QCOMPARE(transport["duplicateScope"].toString(), QString("collection"));
    QVERIFY(transport["checkAllModels"].isBool());
    QVERIFY(!transport["checkAllModels"].toBool());
}

// B24: AnkiManager.save (default-profile fallback).
void AnkiConfigTest::a_non_default_save_falls_back_to_the_default_profile_file() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    const std::string second = create_profile(profiles, "Second");
    QVERIFY(!second.empty());
    QVERIFY(!profiles.set_active_profile(second).has_value());

    QJsonObject default_profile = profile_json();
    default_profile["selectedDeck"] = "DefaultDeck";
    write_json(profile_file(env.root(), kDefaultProfileId), default_profile);

    AnkiConfigStore store(env.root(), profiles);
    store.load();
    store.settings().selected_deck = "SecondDeck";
    QVERIFY(store.save().ok);

    const std::optional<QJsonObject> global = read_json_object(global_file(env.root()));
    QVERIFY(global.has_value());
    QCOMPARE((*global)["selectedDeck"].toString(), QString("DefaultDeck"));
    QVERIFY((*global)["useAnkiConnect"].isBool());
    QVERIFY((*global)["useAnkiConnect"].toBool());
}

// B24: AnkiManager.save (active-profile fallback when the default file is
// unreadable).
void AnkiConfigTest::a_non_default_save_uses_the_active_state_when_the_default_file_is_unreadable() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    const std::string second = create_profile(profiles, "Second");
    QVERIFY(!second.empty());
    QVERIFY(!profiles.set_active_profile(second).has_value());
    write_bytes(profile_file(env.root(), kDefaultProfileId), "not json");

    AnkiConfigStore store(env.root(), profiles);
    store.load();
    store.settings().selected_deck = "SecondDeck";
    QVERIFY(store.save().ok);

    const std::optional<QJsonObject> global = read_json_object(global_file(env.root()));
    QVERIFY(global.has_value());
    QCOMPARE((*global)["selectedDeck"].toString(), QString("SecondDeck"));
    QVERIFY((*global)["useAnkiConnect"].isBool());
    QVERIFY((*global)["useAnkiConnect"].toBool());
}

// B26: JSONDecoder/JSONEncoder through AnkiProfileConfig and AnkiConfig.
void AnkiConfigTest::unknown_members_are_ignored_and_dropped() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    QJsonObject profile = profile_json();
    profile["selectedDeck"] = "Deck";
    profile["futureMember"] = 7;
    write_json(profile_file(env.root(), kDefaultProfileId), profile);
    QJsonObject global = global_json();
    global["futureMember"] = true;
    write_json(global_file(env.root()), global);

    store.load();
    QCOMPARE(store.settings().selected_deck.value_or(""), std::string("Deck"));
    QVERIFY(store.save().ok);

    for (const std::filesystem::path& path :
         {profile_file(env.root(), kDefaultProfileId), global_file(env.root())}) {
        const std::optional<QJsonObject> object = read_json_object(path);
        QVERIFY(object.has_value());
        QVERIFY(!object->contains("futureMember"));
    }
}

// B27: AnkiManager.loadProfile, apply(_:), save. Mappings are unvalidated.
void AnkiConfigTest::field_mappings_pass_through_unvalidated() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    const QJsonObject mappings{{"NotAField", ""},
                               {"表層形", "{expression}"},
                               {"", "value"},
                               {"Key", "値"}};
    QJsonObject profile = profile_json();
    profile["fieldMappings"] = mappings;
    write_json(profile_file(env.root(), kDefaultProfileId), profile);

    store.load();
    QCOMPARE(static_cast<int>(store.settings().field_mappings.size()), 4);
    QVERIFY(store.settings().field_mappings.contains("NotAField"));
    QVERIFY(store.settings().field_mappings.contains("表層形"));
    QVERIFY(store.settings().field_mappings.contains(""));
    QVERIFY(store.settings().field_mappings.contains("Key"));
    QCOMPARE(store.settings().field_mappings.at("NotAField"), std::string(""));
    QCOMPARE(store.settings().field_mappings.at("表層形"), std::string("{expression}"));
    QCOMPARE(store.settings().field_mappings.at(""), std::string("value"));
    QCOMPARE(store.settings().field_mappings.at("Key"), std::string("値"));

    QVERIFY(store.save().ok);
    const std::optional<QJsonObject> saved =
        read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(saved.has_value());
    QCOMPARE((*saved)["fieldMappings"].toObject(), mappings);
}

// B28: AnkiManager.setFieldMapping, setAnkiConnectURL, setAnkiConnectAPIKey,
// setTags. Every setter saves at once.
void AnkiConfigTest::setters_store_values_verbatim_and_save_at_once() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();

    QVERIFY(store.set_field_mapping("Front", "   ").ok);
    QVERIFY(store.settings().field_mappings.contains("Front"));
    QCOMPARE(store.settings().field_mappings.at("Front"), std::string(""));
    std::optional<QJsonObject> profile =
        read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(profile.has_value());
    QCOMPARE((*profile)["fieldMappings"].toObject()["Front"].toString(), QString(""));

    QVERIFY(store.set_field_mapping("Back", "  {expression}  ").ok);
    QVERIFY(store.settings().field_mappings.contains("Back"));
    QCOMPARE(store.settings().field_mappings.at("Back"), std::string("  {expression}  "));
    profile = read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(profile.has_value());
    QCOMPARE((*profile)["fieldMappings"].toObject()["Back"].toString(),
             QString("  {expression}  "));

    QVERIFY(store.set_anki_connect_url(" http://host:1234 ").ok);
    QVERIFY(store.settings().transport.has_value());
    QVERIFY(store.settings().transport->url.has_value());
    QCOMPARE(*store.settings().transport->url, std::string(" http://host:1234 "));
    std::optional<QJsonObject> global = read_json_object(global_file(env.root()));
    QVERIFY(global.has_value());
    QCOMPARE((*global)["ankiConnectConfig"].toObject()["url"].toString(),
             QString(" http://host:1234 "));

    QVERIFY(store.set_anki_connect_api_key(" key with spaces ").ok);
    QVERIFY(store.settings().transport->api_key.has_value());
    QCOMPARE(*store.settings().transport->api_key, std::string(" key with spaces "));
    global = read_json_object(global_file(env.root()));
    QVERIFY(global.has_value());
    QCOMPARE((*global)["ankiConnectConfig"].toObject()["apiKey"].toString(),
             QString(" key with spaces "));

    QVERIFY(store.set_tags(" tag  ").ok);
    QCOMPARE(store.settings().tags, std::string(" tag  "));
    profile = read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(profile.has_value());
    QCOMPARE((*profile)["tags"].toString(), QString(" tag  "));
}

// B29: AnkiManager.save (try?). A failed profile write is best effort and the
// global file is still written.
void AnkiConfigTest::a_failed_profile_write_still_writes_the_global_file() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();
    store.settings().selected_deck = "Deck";

    const std::filesystem::path path = profile_file(env.root(), kDefaultProfileId);
    std::filesystem::remove_all(path.parent_path());
    write_bytes(path, "not json");

    const AnkiSaveResult result = store.save();

    QVERIFY(!result.ok);
    QCOMPARE(read_bytes(path), QByteArray("not json"));
    const std::optional<QJsonObject> global = read_json_object(global_file(env.root()));
    QVERIFY(global.has_value());
    QCOMPARE((*global)["selectedDeck"].toString(), QString("Deck"));
}

// B29: AnkiManager.save (guard let encoded). A non-finite compression value
// cannot be encoded, so neither file is touched.
void AnkiConfigTest::a_non_finite_compression_value_is_not_encoded() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();
    store.settings().image_compression_quality =
        std::numeric_limits<double>::infinity();

    const AnkiSaveResult result = store.save();

    QVERIFY(!result.ok);
    QVERIFY(!std::filesystem::exists(profile_file(env.root(), kDefaultProfileId)));
    QVERIFY(!std::filesystem::exists(global_file(env.root())));
}

// B29, Decisions: a damaged profile file is not overwritten and the failure
// is reported.
void AnkiConfigTest::a_damaged_profile_file_is_reported_and_left_untouched() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);

    const std::filesystem::path path = profile_file(env.root(), kDefaultProfileId);
    write_bytes(path, "{ broken");
    store.load();
    expect_default_profile(store.settings());

    const AnkiSaveResult result = store.save();

    QVERIFY(!result.ok);
    QCOMPARE(read_bytes(path), QByteArray("{ broken"));
}

// B29, Decisions: a damaged global file is not overwritten and the failure is
// reported after the profile file was written.
void AnkiConfigTest::a_damaged_global_file_is_reported_and_left_untouched() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    AnkiConfigStore store(env.root(), profiles);
    store.load();
    store.settings().selected_deck = "Deck";

    const std::filesystem::path path = global_file(env.root());
    write_bytes(path, "{ broken");

    const AnkiSaveResult result = store.save();

    QVERIFY(!result.ok);
    QCOMPARE(read_bytes(path), QByteArray("{ broken"));
    const std::optional<QJsonObject> profile =
        read_json_object(profile_file(env.root(), kDefaultProfileId));
    QVERIFY(profile.has_value());
    QCOMPARE((*profile)["selectedDeck"].toString(), QString("Deck"));
}

// Decisions: AnkiManager.loadTransport drops the API key; this port keeps it
// across restarts.
void AnkiConfigTest::the_transport_api_key_survives_a_reload() {
    TempDataRoot env;
    QVERIFY(env.valid());
    ProfileRepository profiles(env.root());
    {
        AnkiConfigStore store(env.root(), profiles);
        store.load();
        QVERIFY(store.set_anki_connect_api_key("persisted-key").ok);
    }

    AnkiConfigStore reloaded(env.root(), profiles);
    reloaded.load();

    QVERIFY(reloaded.settings().transport.has_value());
    QVERIFY(reloaded.settings().transport->api_key.has_value());
    QCOMPARE(*reloaded.settings().transport->api_key, std::string("persisted-key"));
}

QTEST_APPLESS_MAIN(AnkiConfigTest)
#include "tst_anki_config.moc"
