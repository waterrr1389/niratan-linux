// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows Features/Popup/AnkiMining.swift (preflightAnkiMining, mineAnkiEntry,
// webPayload), Core/AnkiManager.swift (getMediaDirPath, writeDirectMedia,
// addNoteAnkiConnect, syncAnkiConnect) and script/test_video_anime_mining_contract.swift.

#include "services/anki/mining.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

#include <filesystem>
#include <set>
#include <system_error>
#include <utility>

#include "services/anki/anki_requests.h"

using namespace niratan::anki;

namespace {

constexpr auto profile_changed = "The active Profile changed. Try adding the card again.";
constexpr auto abc_sha = "a9993e364706816aba3e25717850c26c9cd0d89d";
constexpr auto cover_name = "hoshi_cover_aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d.avif";

QString name(const QString& prefix, const QString& extension) {
    return prefix + abc_sha + "." + extension;
}

QJsonArray attachment(const QString& filename, const QJsonArray& fields) {
    return QJsonArray{QJsonObject{{"data", "YWJj"}, {"filename", filename}, {"fields", fields}}};
}

bool write_file(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

struct Fixture {
    QTemporaryDir data;
    DuplicateLookupService lookup{data.path().toStdString()};
    AnkiSettings settings;
    std::string repository_id = "reader";
    std::string anki_id = "old";
    bool apply_activation = true;
    QStringList events;
    std::map<std::string, AnkiConnectResult> replies;
    std::map<std::string, std::optional<QJsonObject>> params;
    std::string deferred_action;
    AnkiConnectClient::Callback pending_request;
    MiningDependencies::DownloadCallback pending_download;
    std::optional<QByteArray> audio = QByteArray("abc");
    QUrl audio_url;
    MiningImage source_image;
    bool compress = false;
    ImageCompressionFormat image_format = ImageCompressionFormat::jpeg;
    double quality = 0;
    bool saved_at_clear = false;
    std::optional<AnkiMiningResult> result;
    int completions = 0;

    MiningService service{
        lookup,
        MiningDependencies{
            .request =
                [this](std::string action, std::optional<QJsonObject> body, auto complete) {
                    events << QString::fromStdString(action);
                    params[action] = std::move(body);
                    if (action == deferred_action) {
                        pending_request = std::move(complete);
                    } else {
                        complete(replies[action]);
                    }
                },
            .download_audio =
                [this](QUrl url, auto complete) {
                    events << "download";
                    audio_url = std::move(url);
                    pending_download = std::move(complete);
                },
            .process_image =
                [this](MiningImage image, bool enabled, ImageCompressionFormat format,
                       double value) {
                    events << "image";
                    source_image = std::move(image);
                    compress = enabled;
                    image_format = format;
                    quality = value;
                    return MiningImage{"hello", "avif"};
                },
            .dictionary_media =
                [this](std::string_view dictionary, std::string_view path) {
                    events << (QString::fromUtf8(dictionary) + ":" + QString::fromUtf8(path));
                    return QByteArray("abc");
                },
            .clear_media =
                [this] {
                    events << "clear";
                    saved_at_clear = !saved_words().empty();
                },
            .repository_profile_id = [this] { return repository_id; },
            .anki_profile_id = [this] { return anki_id; },
            .activate_profile =
                [this](std::string_view id) {
                    events << ("activate:" + QString::fromUtf8(id));
                    if (apply_activation && anki_id != id) {
                        anki_id = id;
                        settings.selected_deck = "Reading::Child";
                        settings.selected_note_type = "Model";
                    }
                },
            .settings = [this]() -> const AnkiSettings& { return settings; },
        }};

    Fixture() {
        settings.available_note_types = {{"Model", {"Word"}}};
        replies["canAddNotesWithErrorDetail"] = {
            true, QJsonArray{QJsonObject{{"canAdd", true}}}, {}};
        replies["addNote"] = {true, QJsonValue(qint64(9007199254740993LL)), {}};
    }

    void mine(MiningAttempt attempt = {}) {
        result.reset();
        service.mine(std::move(attempt), [this](AnkiMiningResult outcome) {
            ++completions;
            result = std::move(outcome);
        });
    }

    void expect(AnkiMiningStatus status, const std::string& message,
                std::optional<std::int64_t> id = {}) const {
        QCOMPARE(result, std::optional(AnkiMiningResult{status, message, id}));
    }

    QJsonObject note() const {
        const auto found = params.find("addNote");
        if (found == params.end() || !found->second) {
            return {};
        }
        return found->second->value("note").toObject();
    }

    std::set<std::string> saved_words() const {
        return load_saved_words(data.path().toStdString());
    }
};

}  // namespace

class MiningTest : public QObject {
    Q_OBJECT

   private slots:
    void preflight_activates_before_reading_settings();
    void missing_selection_data();
    void missing_selection();
    void duplicate_verdict_data();
    void duplicate_verdict();
    void profile_checks_data();
    void profile_checks();
    void skipped_preflight_still_checks_capture();
    void skipped_preflight_does_not_recheck_anki_profile();
    void text_note_and_success_order();
    void decode_is_all_or_nothing();
    void note_settings_are_snapshotted_before_audio_download();
    void web_audio_data();
    void web_audio();
    void video_uploads_data();
    void video_uploads();
    void missing_attachment_data();
    void missing_attachment();
    void dictionary_upload_is_best_effort();
    void direct_media_and_cache();
    void unusable_media_directory_is_cached_data();
    void unusable_media_directory_is_cached();
    void direct_write_or_move_failure_falls_back_data();
    void direct_write_or_move_failure_falls_back();
    void add_failure_does_not_keep_old_error();
};

void MiningTest::preflight_activates_before_reading_settings() {
    Fixture f;
    int completions = 0;
    f.service.preflight({}, [&](auto result) {
        ++completions;
        QVERIFY(!result);
    });
    QCOMPARE(completions, 1);
    QCOMPARE(f.events, (QStringList{"activate:reader", "canAddNotesWithErrorDetail"}));
    QCOMPARE(f.params["canAddNotesWithErrorDetail"],
             can_add_notes_request("Reading::Child", "Model", "Word", "Entry",
                                   DuplicateScope::collection, false)
                 .params);
}

void MiningTest::missing_selection_data() {
    QTest::addColumn<bool>("passed");
    QTest::newRow("preflight") << false;
    QTest::newRow("add-guard") << true;
}

void MiningTest::missing_selection() {
    QFETCH(bool, passed);
    Fixture f;
    f.anki_id = f.repository_id;
    MiningAttempt attempt;
    attempt.preflight_already_passed = passed;
    f.mine(attempt);
    f.expect(AnkiMiningStatus::failed,
             passed ? "Failed to add card." : "Configure Anki deck and model first.");
    QCOMPARE(f.events,
             passed ? QStringList{"activate:reader"}
                    : (QStringList{"activate:reader", "activate:reader"}));
}

void MiningTest::duplicate_verdict_data() {
    QTest::addColumn<bool>("with_ids");
    QTest::newRow("first-id") << true;
    QTest::newRow("no-id") << false;
}

void MiningTest::duplicate_verdict() {
    QFETCH(bool, with_ids);
    Fixture f;
    f.settings.field_mappings = {{"StillMissing", "{book-cover}"}};
    f.replies["canAddNotesWithErrorDetail"] = {
        true, QJsonArray{QJsonObject{{"canAdd", false}}}, {}};
    f.replies["findNotes"] = {true, with_ids ? QJsonArray{72, 13} : QJsonArray{}, {}};
    MiningAttempt attempt;
    attempt.content = {{"expression", "word"}};
    attempt.context.video.emplace();
    f.mine(attempt);
    f.expect(AnkiMiningStatus::duplicate, "Already exists in Anki.",
             with_ids ? std::optional<std::int64_t>(72) : std::nullopt);
    QCOMPARE(f.events,
             (QStringList{"activate:reader", "activate:reader", "canAddNotesWithErrorDetail",
                          "findNotes"}));
    QCOMPARE(f.params["canAddNotesWithErrorDetail"],
             can_add_notes_request("Reading::Child", "Model", "Word", "word",
                                   DuplicateScope::collection, false)
                 .params);
    QCOMPARE(f.params["findNotes"], find_notes_request(R"("note:Model" "word:word")").params);
    QVERIFY(f.result);
    QJsonObject payload{{"status", "duplicate"}, {"message", "Already exists in Anki."}};
    if (with_ids) {
        payload.insert("noteID", "72");
    }
    QCOMPARE(f.result->web_payload(), payload);
}

void MiningTest::profile_checks_data() {
    QTest::addColumn<QString>("change");
    QTest::newRow("before-activation-even-after-preflight") << "target";
    QTest::newRow("repository-changed-during-lookup") << "repository";
    QTest::newRow("anki-settings-changed-during-lookup") << "anki";
    QTest::newRow("after-preflight-without-lookup") << "after";
}

void MiningTest::profile_checks() {
    QFETCH(QString, change);
    Fixture f;
    MiningAttempt attempt;
    if (change == "target") {
        attempt.profile_id = "other";
        attempt.preflight_already_passed = true;
        f.mine(attempt);
        QVERIFY(f.events.isEmpty());
    } else if (change == "after") {
        f.settings.allow_dupes = true;
        // Simulate an activation whose settings did not switch to the target.
        f.apply_activation = false;
        f.settings.selected_deck = "Reading";
        f.settings.selected_note_type = "Model";
        f.mine(attempt);
        QCOMPARE(f.events, (QStringList{"activate:reader", "activate:reader"}));
    } else {
        f.replies["canAddNotesWithErrorDetail"] = {
            true, QJsonArray{QJsonObject{{"canAdd", false}}}, {}};
        f.deferred_action = "findNotes";
        f.mine(attempt);
        QVERIFY(f.pending_request);
        QVERIFY(!f.result);
        (change == "repository" ? f.repository_id : f.anki_id) = "other";
        std::exchange(f.pending_request, {})({true, QJsonArray{72}, {}});
        QCOMPARE(f.completions, 1);
    }
    f.expect(AnkiMiningStatus::failed, profile_changed);
}

void MiningTest::skipped_preflight_still_checks_capture() {
    Fixture f;
    f.settings.field_mappings = {{"Clip", "{sasayaki-audio}"}, {"Frame", "{book-cover}"}};
    MiningAttempt attempt;
    attempt.preflight_already_passed = true;
    auto& video = attempt.context.video.emplace();
    video.audio_clip_error_message = "Capture failed";
    f.mine(attempt);
    f.expect(AnkiMiningStatus::failed, "Capture failed");
    video.audio_clip_filename = "clip.m4a";
    f.mine(attempt);
    f.expect(AnkiMiningStatus::failed, "Unable to capture the video screenshot.");
    QCOMPARE(f.events, (QStringList{"activate:reader", "activate:reader"}));
}

void MiningTest::skipped_preflight_does_not_recheck_anki_profile() {
    Fixture f;
    f.apply_activation = false;
    f.settings.selected_deck = "Reading";
    f.settings.selected_note_type = "Model";
    MiningAttempt attempt;
    attempt.profile_id = f.repository_id;
    attempt.preflight_already_passed = true;
    f.mine(attempt);
    QCOMPARE(f.events, (QStringList{"activate:reader", "addNote", "clear"}));
    QCOMPARE(f.anki_id, std::string("old"));
    f.expect(AnkiMiningStatus::added, "Added to Anki.", 9007199254740993LL);
}

void MiningTest::text_note_and_success_order() {
    Fixture f;
    f.settings.allow_dupes = true;
    f.settings.tags = "  first  second\tthird\nfourth ";
    f.settings.transport->duplicate_scope = DuplicateScope::deck_root;
    f.settings.transport->check_all_models = true;
    f.settings.transport->force_sync = true;
    f.settings.field_mappings = {{"Word", "{expression}"},
                                 {"Definition", "{single-glossary-D-brief}"},
                                 {"AudioAsText", " {audio} "},
                                 {"Blank", ""}};
    MiningAttempt attempt;
    attempt.content = {
        {"expression", "word"},
        {"audio", "https://example.test/voice"},
        {"singleGlossaries", R"({"D":"<li data-dictionary=\"D\"><i>(D)</i> gloss"})"}};
    f.mine(attempt);
    f.expect(AnkiMiningStatus::added, "Added to Anki.", 9007199254740993LL);
    QCOMPARE(f.events,
             (QStringList{"activate:reader", "activate:reader", "addNote", "clear", "sync"}));
    QVERIFY(f.saved_at_clear);
    QCOMPARE(f.saved_words(), std::set<std::string>{"word"});
    const QJsonObject options{
        {"allowDuplicate", true},
        {"duplicateScope", "deck"},
        {"duplicateScopeOptions",
         QJsonObject{{"deckName", "Reading"}, {"checkChildren", true}, {"checkAllModels", true}}}};
    const QJsonObject fields{{"Word", "word"},
                             {"Definition", "<li data-dictionary=\"D\">gloss"},
                             {"AudioAsText", " https://example.test/voice "},
                             {"Blank", ""}};
    QCOMPARE(f.note(),
             (QJsonObject{{"deckName", "Reading::Child"},
                          {"modelName", "Model"},
                          {"fields", fields},
                          {"options", options},
                          {"tags", QJsonArray{"first", "second\tthird\nfourth"}}}));
    QVERIFY(f.result);
    QCOMPARE(f.result->web_payload(),
             (QJsonObject{{"status", "added"},
                          {"message", "Added to Anki."},
                          {"noteID", "9007199254740993"}}));
}

void MiningTest::decode_is_all_or_nothing() {
    Fixture f;
    f.settings.tags = "  ";
    f.settings.field_mappings = {{"Text", "{single-glossary-D}"}};
    MiningAttempt attempt;
    attempt.content = {{"singleGlossaries", R"({"D":"valid","Other":4})"},
                       {"dictionaryMedia", R"([{"dictionary":"D","path":"p","filename":"f"},{}])"}};
    f.mine(attempt);
    f.expect(AnkiMiningStatus::added, "Added to Anki.", 9007199254740993LL);
    QCOMPARE(f.events,
             (QStringList{"activate:reader", "activate:reader", "canAddNotesWithErrorDetail",
                          "addNote", "clear"}));
    QCOMPARE(f.note().value("fields").toObject(), (QJsonObject{{"Text", ""}}));
    QCOMPARE(f.note().value("options").toObject(),
             (QJsonObject{{"allowDuplicate", false}, {"duplicateScope", "collection"}}));
    QVERIFY(!f.note().contains("tags"));
    QCOMPARE(f.saved_words(), std::set<std::string>{""});
}

void MiningTest::note_settings_are_snapshotted_before_audio_download() {
    Fixture f;
    const auto cover = f.data.filePath("cover");
    QVERIFY(write_file(cover, "abc"));
    f.settings.field_mappings = {{"Audio", "{audio}"}, {"Cover", "{book-cover}"}, {"Text", "old"}};
    f.settings.tags = "original";
    f.settings.compress_images = false;
    f.settings.image_compression_quality = 0.7;
    f.settings.image_compression_format = ImageCompressionFormat::avif;
    MiningAttempt attempt;
    attempt.preflight_already_passed = true;
    attempt.content = {{"audio", "https://example.test/voice"}};
    attempt.context.cover_path = cover.toStdString();
    f.mine(attempt);
    QVERIFY(f.pending_download);
    QVERIFY(!f.result);
    QCOMPARE(f.events, (QStringList{"activate:reader", "getMediaDirPath", "download"}));

    f.settings.selected_deck = "OtherDeck";
    f.settings.selected_note_type = "OtherModel";
    f.settings.field_mappings = {{"Text", "new"}};
    f.settings.tags = "modified";
    f.settings.allow_dupes = true;
    f.settings.transport->duplicate_scope = DuplicateScope::deck;
    f.settings.transport->check_all_models = true;
    f.settings.compress_images = true;
    f.settings.image_compression_quality = 0.2;
    f.settings.image_compression_format = ImageCompressionFormat::jpeg;
    std::exchange(f.pending_download, {})(f.audio);

    QCOMPARE(f.events,
             (QStringList{"activate:reader", "getMediaDirPath", "download", "image", "addNote",
                          "clear"}));
    QCOMPARE(f.source_image, (MiningImage{"abc", "png"}));
    QVERIFY(!f.compress);
    QCOMPARE(f.image_format, ImageCompressionFormat::avif);
    QCOMPARE(f.quality, 0.7);
    const auto note = f.note();
    QCOMPARE(note.value("deckName").toString(), QString("Reading::Child"));
    QCOMPARE(note.value("modelName").toString(), QString("Model"));
    QCOMPARE(note.value("fields").toObject(), (QJsonObject{{"Text", "old"}}));
    QCOMPARE(note.value("tags").toArray(), QJsonArray{"original"});
    QCOMPARE(note.value("options").toObject(),
             (QJsonObject{{"allowDuplicate", false}, {"duplicateScope", "collection"}}));
    QCOMPARE(note.value("audio").toArray(), attachment(name("hoshi_audio_", "mp3"), {"Audio"}));
    QCOMPARE(
        note.value("picture").toArray(),
        (QJsonArray{QJsonObject{
            {"filename", cover_name}, {"data", "aGVsbG8="}, {"fields", QJsonArray{"Cover"}}}}));
}

void MiningTest::web_audio_data() {
    QTest::addColumn<QString>("url");
    QTest::addColumn<bool>("download");
    QTest::addColumn<bool>("success");
    QTest::newRow("http") << "http://example.test/voice" << true << true;
    QTest::newRow("https-error") << "https://example.test/voice" << true << false;
    QTest::newRow("file") << "file:///voice" << true << true;
    QTest::newRow("unsupported-scheme") << "ftp://example.test/voice" << false << false;
}

void MiningTest::web_audio() {
    QFETCH(QString, url);
    QFETCH(bool, download);
    QFETCH(bool, success);
    Fixture f;
    f.settings.field_mappings = {{"WordAudio", "{audio}"}};
    MiningAttempt attempt;
    attempt.content = {{"audio", url.toStdString()}};
    attempt.preflight_already_passed = true;
    f.mine(attempt);
    if (download) {
        QVERIFY(f.pending_download);
        QVERIFY(!f.result);
        QCOMPARE(f.audio_url, QUrl(url));
        QCOMPARE(f.events, (QStringList{"activate:reader", "download"}));
        std::exchange(f.pending_download, {})(success ? f.audio : std::nullopt);
    }
    f.expect(AnkiMiningStatus::added, "Added to Anki.", 9007199254740993LL);
    QCOMPARE(f.completions, 1);
    QVERIFY(!f.params.contains("getMediaDirPath"));
    QCOMPARE(f.note().value("fields").toObject(), QJsonObject{});
    QCOMPARE(f.note().contains("audio"), success);
    if (success) {
        QCOMPARE(f.note().value("audio").toArray(),
                 attachment(name("hoshi_audio_", "mp3"), {"WordAudio"}));
    }
}

void MiningTest::video_uploads_data() {
    QTest::addColumn<bool>("use_mp3");
    QTest::addColumn<QString>("clip_name");
    QTest::addColumn<QString>("frame_name");
    QTest::addColumn<QString>("audio_ext");
    QTest::addColumn<QString>("picture_ext");
    QTest::newRow("aac-fallback") << false << "clip" << "frame.BMP" << "m4a" << "bmp";
    QTest::newRow("mp3-and-png-fallbacks") << true << "clip" << "frame" << "mp3" << "png";
    QTest::newRow("path-extension-wins") << true << "clip.WAV" << "frame.PNG" << "wav" << "png";
}

void MiningTest::video_uploads() {
    QFETCH(bool, use_mp3);
    QFETCH(QString, clip_name);
    QFETCH(QString, frame_name);
    QFETCH(QString, audio_ext);
    QFETCH(QString, picture_ext);
    Fixture f;
    QVERIFY(f.data.isValid());
    const auto clip = f.data.filePath(clip_name);
    const auto frame = f.data.filePath(frame_name);
    QVERIFY(write_file(clip, "abc"));
    QVERIFY(write_file(frame, "abc"));
    f.replies["getMediaDirPath"] = {true, frame, {}};
    if (use_mp3) {
        f.settings.audio_compression_format = AudioCompressionFormat::mp3;
    }
    f.settings.field_mappings = {{"LegacySound", "{video-audio-clip}"},
                                 {"Sound", "{sasayaki-audio}"},
                                 {"LegacyFrame", "{video-screenshot}"},
                                 {"Frame", "{book-cover}"}};
    MiningAttempt attempt;
    attempt.preflight_already_passed = true;
    auto& video = attempt.context.video.emplace();
    video.audio_clip_path = clip.toStdString();
    video.screenshot_path = frame.toStdString();
    video.screenshot_filename = "direct.jpg";
    f.mine(attempt);
    QCOMPARE(f.events, (QStringList{"activate:reader", "getMediaDirPath", "addNote", "clear"}));
    QCOMPARE(f.note().value("fields").toObject(), QJsonObject{});
    const auto sound = f.note().value("audio").toArray();
    const auto picture = f.note().value("picture").toArray();
    QCOMPARE(sound.size(), 1);
    QCOMPARE(picture.size(), 1);
    const auto compare_entry = [](QJsonObject entry, QJsonArray expected) {
        auto fields = entry.take("fields").toArray().toVariantList();
        std::ranges::sort(fields, {}, &QVariant::toString);
        entry.insert("fields", QJsonArray::fromVariantList(fields));
        QCOMPARE(QJsonArray{entry}, expected);
    };
    compare_entry(sound.at(0).toObject(),
                  attachment(name("hoshi_video_audio_", audio_ext), {"LegacySound", "Sound"}));
    compare_entry(picture.at(0).toObject(),
                  attachment(name("hoshi_video_frame_", picture_ext), {"Frame", "LegacyFrame"}));
}

void MiningTest::missing_attachment_data() {
    QTest::addColumn<QString>("mapping");
    QTest::newRow("sasayaki-bucket-alone") << "{sasayaki-audio}";
    QTest::newRow("cover-url-alone") << "{book-cover}";
}

void MiningTest::missing_attachment() {
    QFETCH(QString, mapping);
    Fixture f;
    f.settings.field_mappings = {{"Media", mapping.toStdString()}};
    MiningAttempt attempt;
    attempt.preflight_already_passed = true;
    if (mapping == "{book-cover}") {
        const auto cover = f.data.filePath("cover.png");
        QVERIFY(write_file(cover, "abc"));
        attempt.context.cover_url = QUrl::fromLocalFile(cover).toString().toStdString();
    }
    f.mine(attempt);
    QCOMPARE(f.events, (QStringList{"activate:reader", "getMediaDirPath", "addNote", "clear"}));
    QCOMPARE(f.note().value("fields").toObject(), QJsonObject{});
    QVERIFY(!f.note().contains("audio"));
    QVERIFY(!f.note().contains("picture"));
}

void MiningTest::dictionary_upload_is_best_effort() {
    Fixture f;
    f.settings.field_mappings = {
        {"A", "{glossary}"}, {"B", "asset asset"}, {"Sasayaki", "{sasayaki-audio}"}};
    f.replies["getMediaDirPath"] = {true, f.data.filePath("missing"), {}};
    f.replies["storeMediaFile"] = {false, {}, "upload failed"};
    MiningAttempt attempt;
    attempt.preflight_already_passed = true;
    attempt.content = {
        {"glossary", "<img src=\"asset\">"},
        {"dictionaryMedia", R"([{"dictionary":"D","path":"nested/item","filename":"asset"}])"}};
    f.mine(attempt);
    f.mine(attempt);
    f.expect(AnkiMiningStatus::added, "Added to Anki.", 9007199254740993LL);
    QCOMPARE(f.events,
             (QStringList{"activate:reader", "getMediaDirPath", "D:nested/item", "storeMediaFile",
                          "addNote", "clear", "activate:reader", "D:nested/item", "storeMediaFile",
                          "addNote", "clear"}));
    const QString filename = name("hoshi_dict_", "bin");
    QCOMPARE(f.params["storeMediaFile"],
             std::optional(QJsonObject{{"filename", filename}, {"data", "YWJj"}}));
    QCOMPARE(
        f.note().value("fields").toObject(),
        (QJsonObject{{"A", "<img src=\"" + filename + "\">"}, {"B", filename + " " + filename}}));
    QVERIFY(!f.note().contains("audio"));
}

void MiningTest::direct_media_and_cache() {
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    const auto cover_path = f.data.filePath("cover.PNG");
    QVERIFY(write_file(cover_path, "abc"));
    const auto cover_destination = media.filePath(cover_name);
    QVERIFY(write_file(cover_destination, "existing"));
    f.replies["getMediaDirPath"] = {true, media.path(), {}};
    f.settings.image_compression_quality = 0.7;
    f.settings.image_compression_format = ImageCompressionFormat::avif;
    f.settings.field_mappings = {
        {"Cover", "{book-cover}"}, {"Link", "url={book-cover}"}, {"Gloss", "asset"}};
    MiningAttempt attempt;
    attempt.preflight_already_passed = true;
    attempt.context.cover_path = cover_path.toStdString();
    attempt.context.cover_url = "http://example.test/cover";
    attempt.content = {
        {"dictionaryMedia", R"([{"dictionary":"D","path":"icon.PNG","filename":"asset"}])"}};
    f.mine(attempt);
    QCOMPARE(f.events,
             (QStringList{"activate:reader", "getMediaDirPath", "image", "D:icon.PNG", "addNote",
                          "clear"}));
    QCOMPARE(f.source_image, (MiningImage{"abc", "png"}));
    QVERIFY(f.compress);
    QCOMPARE(f.image_format, ImageCompressionFormat::avif);
    QCOMPARE(f.quality, 0.7);
    const auto dict = name("hoshi_dict_", "png");
    QCOMPARE(f.note().value("fields").toObject(),
             (QJsonObject{{"Cover", QString("<img src=\"%1\">").arg(cover_name)},
                          {"Link", "url=http://example.test/cover"},
                          {"Gloss", dict}}));
    QVERIFY(!f.note().contains("picture"));
    QFile existing(cover_destination);
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("existing"));
    QFile written(media.filePath(dict));
    QVERIFY(written.open(QIODevice::ReadOnly));
    QCOMPARE(written.readAll(), QByteArray("abc"));
    QCOMPARE(QDir(media.path()).entryList(QDir::Files | QDir::Hidden),
             (QStringList{cover_name, dict}));

    f.settings.field_mappings = {{"Sound", "{sasayaki-audio}"}, {"Frame", "{book-cover}"}};
    attempt.content.clear();
    auto& video = attempt.context.video.emplace();
    video.audio_clip_filename = "path/clip name.m4a";
    video.screenshot_filename = "frame image.png";
    f.mine(attempt);
    QCOMPARE(f.note().value("fields").toObject(),
             (QJsonObject{{"Sound", "[sound:clip_name.m4a]"},
                          {"Frame", "<img src=\"frame_image.png\">"}}));
    QCOMPARE(f.events.count("getMediaDirPath"), 1);
    f.settings.transport->api_key = "another-key";
    f.mine(attempt);
    QCOMPARE(f.events.count("getMediaDirPath"), 2);
    f.settings.transport->url = "http://another.test";
    f.mine(attempt);
    QCOMPARE(f.events.count("getMediaDirPath"), 3);
}

void MiningTest::unusable_media_directory_is_cached_data() {
    QTest::addColumn<bool>("request_fails");
    QTest::newRow("request-failed") << true;
    QTest::newRow("directory-not-writable") << false;
}

void MiningTest::unusable_media_directory_is_cached() {
    QFETCH(bool, request_fails);
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    const auto writable = QFile::permissions(media.path());
    if (request_fails) {
        f.replies["getMediaDirPath"] = {false, {}, "Media directory request failed"};
    } else {
        QVERIFY(
            QFile::setPermissions(media.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        if (QFileInfo(media.path()).isWritable()) {
            QSKIP("Removing write permissions did not make the test directory unwritable");
        }
        f.replies["getMediaDirPath"] = {true, media.path(), {}};
    }

    f.settings.field_mappings = {{"Clip", "{video-audio-clip}"}};
    MiningAttempt attempt;
    attempt.preflight_already_passed = true;
    attempt.context.video.emplace().audio_clip_filename = "clip.mp3";
    f.mine(attempt);
    QCOMPARE(f.events, (QStringList{"activate:reader", "getMediaDirPath", "addNote", "clear"}));
    QCOMPARE(f.note().value("fields").toObject(), QJsonObject{});
    QVERIFY(!f.note().contains("audio"));

    QVERIFY(QFile::setPermissions(media.path(), writable));
    f.replies["getMediaDirPath"] = {true, media.path(), {}};
    f.mine(attempt);
    QCOMPARE(f.events.count("getMediaDirPath"), 1);
    QCOMPARE(f.note().value("fields").toObject(), QJsonObject{});
    QVERIFY(!f.note().contains("audio"));
}

void MiningTest::direct_write_or_move_failure_falls_back_data() {
    QTest::addColumn<bool>("move_fails");
    QTest::newRow("write-fails") << false;
    QTest::newRow("move-fails") << true;
}

void MiningTest::direct_write_or_move_failure_falls_back() {
    QFETCH(bool, move_fails);
    Fixture f;
    QTemporaryDir media;
    QVERIFY(media.isValid());
    auto cover = f.data.filePath("no-extension");
    QVERIFY(write_file(cover, "abc"));
    f.replies["getMediaDirPath"] = {true, media.path(), {}};
    f.settings.field_mappings = {{"Cover", "{book-cover}"}};
    MiningAttempt attempt;
    attempt.preflight_already_passed = true;
    const auto destination = media.filePath(cover_name);
    if (move_fails) {
        // The destination does not resolve to a file, but its directory entry
        // still blocks a move that does not overwrite.
        std::error_code error;
        std::filesystem::create_symlink("missing-target", destination.toStdString(), error);
        QVERIFY(!error);
        QVERIFY(!QFileInfo::exists(destination));
        QVERIFY(QFileInfo(destination).isSymLink());
    }
    f.mine(attempt);
    QCOMPARE(f.events.count("getMediaDirPath"), 1);
    QVERIFY(!f.note().contains("picture"));
    if (!move_fails) {
        QVERIFY(media.remove());
    }
    attempt.context.cover_path = cover.toStdString();
    f.mine(attempt);
    QCOMPARE(f.events.count("getMediaDirPath"), 1);
    QCOMPARE(f.note().value("fields").toObject(), QJsonObject{});
    QCOMPARE(
        f.note().value("picture").toArray(),
        (QJsonArray{QJsonObject{
            {"filename", cover_name}, {"data", "aGVsbG8="}, {"fields", QJsonArray{"Cover"}}}}));
    if (move_fails) {
        QVERIFY(QFileInfo(destination).isSymLink());
        QCOMPARE(
            QDir(media.path())
                .entryList(QDir::AllEntries | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot),
            QStringList{cover_name});
    }
}

void MiningTest::add_failure_does_not_keep_old_error() {
    Fixture f;
    f.settings.transport->force_sync = true;
    MiningAttempt attempt;
    attempt.preflight_already_passed = true;
    f.replies["addNote"] = {false, {}, "Anki error"};
    f.mine(attempt);
    f.expect(AnkiMiningStatus::failed, "Anki error");
    f.replies["addNote"] = {true, 0, {}};
    f.mine(attempt);
    f.expect(AnkiMiningStatus::failed, "AnkiConnect did not return the added note ID.");
    f.settings.selected_note_type.reset();
    f.mine(attempt);
    f.expect(AnkiMiningStatus::failed, "Failed to add card.");
    QCOMPARE(f.events,
             (QStringList{"activate:reader", "addNote", "activate:reader", "addNote",
                          "activate:reader"}));
    QVERIFY(f.saved_words().empty());
    QVERIFY(f.result);
    QCOMPARE(f.result->web_payload(),
             (QJsonObject{{"status", "failed"}, {"message", "Failed to add card."}}));
}

QTEST_APPLESS_MAIN(MiningTest)
#include "tst_mining.moc"
