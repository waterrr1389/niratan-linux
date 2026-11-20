// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/player/mining_history.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using niratan::player::MiningHistoryItem;
using niratan::player::add;
using niratan::player::clear;
using niratan::player::load;
using niratan::player::mining_history_path;
using niratan::player::remove;
using niratan::player::save;

// Features/Video/VideoMiningHistoryStore.swift and the cases in
// script/test_video_mining_history.swift, with RemoteVideoIdentity from
// Features/Video/Remote/RemoteVideoSource.swift and isYouTubeURL from
// Features/Video/Remote/YouTubeURLParser.swift. The stored document is a bare
// array and createdAt is in Foundation reference seconds, as the synthesized
// Codable and the default JSON date strategy write them.
namespace {

// Foundation's reference date, 2001-01-01T00:00:00Z, in Unix seconds.
constexpr double kReferenceDateOffset = 978307200.0;

double reference_seconds(double unix_seconds) { return unix_seconds - kReferenceDateOffset; }

std::filesystem::path test_directory(const QTemporaryDir& directory) {
    return std::filesystem::path(directory.path().toStdString()) / "app";
}

MiningHistoryItem base_item(std::string id, double created_at) {
    MiningHistoryItem item;
    item.id = std::move(id);
    item.created_at = created_at;
    item.subtitle_text = "星を見ています。\nきれいですね。";
    item.video_file_name = "Episode 1.mkv";
    item.video_title = "Episode 1.mkv";
    item.subtitle_source_name = "Episode 1.ja.srt";
    item.cue_start = 65.25;
    item.cue_end = 70.0;
    return item;
}

// A record carrying every optional member, including a remote identity and an
// embedded subtitle track.
MiningHistoryItem full_item() {
    MiningHistoryItem item =
        base_item("11111111-2222-4333-8444-555555555555", reference_seconds(1760000000.5));
    item.video_title = "Episode One";
    item.video_path = "/media/Episode 1.mkv";

    MiningHistoryItem::RemoteVideoIdentity identity;
    identity.provider_id = "youtube";
    identity.remote_id = "dQw4w9WgXcQ";
    identity.original_url = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
    identity.canonical_url = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
    identity.title = "Remote Episode";
    identity.thumbnail_url = "https://i.ytimg.com/vi/dQw4w9WgXcQ/hqdefault.jpg";
    identity.duration = 42.5;
    item.remote_video_identity = identity;

    item.subtitle_source_path = "/media/Episode 1.ja.srt";
    item.subtitle_format = "webVTT";
    item.embedded_subtitle_track_id = 7;
    return item;
}

// A record with every required member, as the writer emits it.
QJsonObject record_object(const QString& id, double created_at) {
    QJsonObject record;
    record.insert(QStringLiteral("createdAt"), created_at);
    record.insert(QStringLiteral("cueEnd"), 70.0);
    record.insert(QStringLiteral("cueStart"), 65.25);
    record.insert(QStringLiteral("id"), id);
    record.insert(QStringLiteral("subtitleSourceName"), QStringLiteral("Episode 1.ja.srt"));
    record.insert(QStringLiteral("subtitleText"), QStringLiteral("字幕"));
    record.insert(QStringLiteral("videoFileName"), QStringLiteral("Episode 1.mkv"));
    record.insert(QStringLiteral("videoTitle"), QStringLiteral("Episode 1.mkv"));
    return record;
}

QJsonObject identity_object(const QString& provider_key, const QString& provider_id,
                            const QString& original_url) {
    QJsonObject identity;
    identity.insert(provider_key, provider_id);
    identity.insert(QStringLiteral("originalURL"), original_url);
    identity.insert(QStringLiteral("remoteID"), QStringLiteral("dQw4w9WgXcQ"));
    identity.insert(QStringLiteral("title"), QStringLiteral("Remote Episode"));
    return identity;
}

std::vector<std::string> ids_of(const std::vector<MiningHistoryItem>& items) {
    std::vector<std::string> ids;
    ids.reserve(items.size());
    for (const MiningHistoryItem& item : items) {
        ids.push_back(item.id);
    }
    return ids;
}

std::vector<std::string> provider_ids_of(const std::vector<MiningHistoryItem>& items) {
    std::vector<std::string> providers;
    providers.reserve(items.size());
    for (const MiningHistoryItem& item : items) {
        providers.push_back(item.remote_video_identity.has_value()
                                ? item.remote_video_identity.value().provider_id
                                : std::string());
    }
    return providers;
}

QByteArray read_bytes(const std::filesystem::path& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool write_bytes(const std::filesystem::path& path, const QByteArray& bytes) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(bytes) == bytes.size();
}

QJsonDocument read_document(const std::filesystem::path& path) {
    const QByteArray bytes = read_bytes(path);
    if (bytes.isEmpty()) {
        return {};
    }
    return QJsonDocument::fromJson(bytes);
}

// Writes the shape the store itself emits.
bool write_records(const std::filesystem::path& path, const QJsonArray& records) {
    return write_bytes(path, QJsonDocument(records).toJson(QJsonDocument::Indented));
}

// Writes the object an earlier build of the port emitted.
bool write_items_object(const std::filesystem::path& path, const QJsonArray& records) {
    QJsonObject document;
    document.insert(QStringLiteral("items"), records);
    return write_bytes(path, QJsonDocument(document).toJson(QJsonDocument::Indented));
}

}  // namespace

class MiningHistoryTest : public QObject {
    Q_OBJECT

   private slots:
    void round_trip_keeps_optional_members();
    void round_trip_omits_absent_optionals();
    void stored_json_uses_the_swift_member_names();
    void absent_title_and_source_name_fall_back_to_the_video_file_name();
    void an_items_object_from_an_earlier_build_still_reads();
    void remote_identity_accepts_the_legacy_provider_member();
    void a_ytdlp_provider_on_a_youtube_url_becomes_youtube();
    void add_orders_by_created_at_then_id();
    void add_prunes_the_oldest_records();
    void add_with_zero_limit_stores_an_empty_list();
    void remove_deletes_only_the_given_id();
    void remove_of_an_absent_id_leaves_the_file_alone();
    void clear_stores_an_empty_list();
    void a_missing_file_reads_as_an_empty_list();
    void a_damaged_or_foreign_document_reads_as_an_empty_list();
};

void MiningHistoryTest::round_trip_keeps_optional_members() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    const MiningHistoryItem item = full_item();
    QVERIFY(save(root, {item}));

    const std::vector<MiningHistoryItem> items = load(root);
    QCOMPARE(items.size(), std::size_t(1));
    const MiningHistoryItem& stored = items.at(0);

    QCOMPARE(stored.id, item.id);
    QCOMPARE(stored.created_at, item.created_at);
    QCOMPARE(stored.subtitle_text, item.subtitle_text);
    QCOMPARE(stored.video_file_name, item.video_file_name);
    QCOMPARE(stored.video_title, item.video_title);

    QVERIFY(stored.video_path.has_value());
    QCOMPARE(stored.video_path.value(), item.video_path.value());
    QVERIFY(stored.subtitle_source_path.has_value());
    QCOMPARE(stored.subtitle_source_path.value(), item.subtitle_source_path.value());
    QVERIFY(stored.subtitle_format.has_value());
    QCOMPARE(stored.subtitle_format.value(), std::string("webVTT"));
    QVERIFY(stored.embedded_subtitle_track_id.has_value());
    QCOMPARE(stored.embedded_subtitle_track_id.value(), 7);

    QVERIFY(stored.remote_video_identity.has_value());
    const MiningHistoryItem::RemoteVideoIdentity& identity = stored.remote_video_identity.value();
    const MiningHistoryItem::RemoteVideoIdentity& expected_identity =
        item.remote_video_identity.value();
    QCOMPARE(identity.provider_id, expected_identity.provider_id);
    QCOMPARE(identity.remote_id, expected_identity.remote_id);
    QCOMPARE(identity.original_url, expected_identity.original_url);
    QVERIFY(identity.canonical_url.has_value());
    QCOMPARE(identity.canonical_url.value(), expected_identity.canonical_url.value());
    QCOMPARE(identity.title, expected_identity.title);
    QVERIFY(identity.thumbnail_url.has_value());
    QCOMPARE(identity.thumbnail_url.value(), expected_identity.thumbnail_url.value());
    QVERIFY(identity.duration.has_value());
    QCOMPARE(identity.duration.value(), 42.5);

    QCOMPARE(stored.cue_start, 65.25);
    QCOMPARE(stored.cue_end, 70.0);
    QVERIFY(stored == item);
}

void MiningHistoryTest::round_trip_omits_absent_optionals() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    const MiningHistoryItem item =
        base_item("11111111-2222-4333-8444-555555555555", reference_seconds(1760000000.0));
    QVERIFY(save(root, {item}));

    const QJsonDocument document = read_document(mining_history_path(root));
    QVERIFY(document.isArray());
    const QJsonArray records = document.array();
    QCOMPARE(static_cast<int>(records.size()), 1);
    const QJsonObject record = records.at(0).toObject();

    const QStringList absent = {
        QStringLiteral("videoPath"),
        QStringLiteral("remoteVideoIdentity"),
        QStringLiteral("subtitleSourcePath"),
        QStringLiteral("subtitleFormat"),
        QStringLiteral("embeddedSubtitleTrackID"),
    };
    for (const QString& key : absent) {
        QVERIFY2(!record.contains(key), qPrintable(key));
    }

    const std::vector<MiningHistoryItem> items = load(root);
    QCOMPARE(items.size(), std::size_t(1));
    QVERIFY(items.at(0) == item);
}

void MiningHistoryTest::stored_json_uses_the_swift_member_names() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    QVERIFY(save(root, {full_item()}));

    const QJsonDocument document = read_document(mining_history_path(root));
    QVERIFY(document.isArray());
    const QJsonArray records = document.array();
    QCOMPARE(static_cast<int>(records.size()), 1);
    const QJsonObject record = records.at(0).toObject();

    const QStringList record_keys = {
        QStringLiteral("createdAt"),
        QStringLiteral("cueEnd"),
        QStringLiteral("cueStart"),
        QStringLiteral("embeddedSubtitleTrackID"),
        QStringLiteral("id"),
        QStringLiteral("remoteVideoIdentity"),
        QStringLiteral("subtitleFormat"),
        QStringLiteral("subtitleSourceName"),
        QStringLiteral("subtitleSourcePath"),
        QStringLiteral("subtitleText"),
        QStringLiteral("videoFileName"),
        QStringLiteral("videoPath"),
        QStringLiteral("videoTitle"),
    };
    QCOMPARE(record.keys(), record_keys);

    // Foundation's default date strategy writes reference seconds, so adding the
    // reference offset has to give back the Unix instant.
    const QJsonValue created_at = record.value(QStringLiteral("createdAt"));
    QVERIFY(created_at.isDouble());
    QCOMPARE(created_at.toDouble() + kReferenceDateOffset, 1760000000.5);

    QCOMPARE(record.value(QStringLiteral("cueStart")).toDouble(), 65.25);
    QCOMPARE(record.value(QStringLiteral("cueEnd")).toDouble(), 70.0);
    QCOMPARE(record.value(QStringLiteral("subtitleFormat")).toString(), QStringLiteral("webVTT"));
    QCOMPARE(record.value(QStringLiteral("embeddedSubtitleTrackID")).toInt(), 7);

    const QStringList identity_keys = {
        QStringLiteral("canonicalURL"),
        QStringLiteral("duration"),
        QStringLiteral("originalURL"),
        QStringLiteral("providerID"),
        QStringLiteral("remoteID"),
        QStringLiteral("thumbnailURL"),
        QStringLiteral("title"),
    };
    const QJsonObject identity = record.value(QStringLiteral("remoteVideoIdentity")).toObject();
    QCOMPARE(identity.keys(), identity_keys);
}

void MiningHistoryTest::absent_title_and_source_name_fall_back_to_the_video_file_name() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    // A record written before videoTitle and subtitleSourceName were stored, as
    // the Swift legacy case in script/test_video_mining_history.swift.
    QJsonObject record;
    record.insert(QStringLiteral("createdAt"), 0.0);
    record.insert(QStringLiteral("cueEnd"), 14.0);
    record.insert(QStringLiteral("cueStart"), 12.5);
    record.insert(QStringLiteral("id"), QStringLiteral("legacy"));
    record.insert(QStringLiteral("subtitleText"), QStringLiteral("古い字幕"));
    record.insert(QStringLiteral("videoFileName"), QStringLiteral("Legacy.mkv"));
    QJsonArray records;
    records.append(record);
    QVERIFY(write_records(mining_history_path(root), records));

    const std::vector<MiningHistoryItem> items = load(root);
    QCOMPARE(items.size(), std::size_t(1));
    const MiningHistoryItem& stored = items.at(0);
    QCOMPARE(stored.video_title, std::string("Legacy.mkv"));
    QCOMPARE(stored.subtitle_source_name, std::string("Legacy.mkv"));
    QVERIFY(!stored.video_path.has_value());
    QVERIFY(!stored.remote_video_identity.has_value());
    QVERIFY(!stored.subtitle_format.has_value());
}

void MiningHistoryTest::an_items_object_from_an_earlier_build_still_reads() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    QJsonArray records;
    records.append(record_object(QStringLiteral("aaaaaaaa-0000-4000-8000-000000000000"),
                                 reference_seconds(1760000000.0)));
    QVERIFY(write_items_object(mining_history_path(root), records));

    const std::vector<MiningHistoryItem> items = load(root);
    QCOMPARE(items.size(), std::size_t(1));
    QCOMPARE(items.at(0).id, std::string("aaaaaaaa-0000-4000-8000-000000000000"));
    QCOMPARE(items.at(0).created_at, reference_seconds(1760000000.0));

    // A write replaces the wrapper with the bare array the store emits.
    QVERIFY(save(root, items));
    QVERIFY(read_document(mining_history_path(root)).isArray());
}

void MiningHistoryTest::remote_identity_accepts_the_legacy_provider_member() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    QJsonArray records;
    QJsonObject record = record_object(QStringLiteral("aaaaaaaa-0000-4000-8000-000000000000"), 0.0);
    record.insert(QStringLiteral("remoteVideoIdentity"),
                  identity_object(QStringLiteral("provider"), QStringLiteral("bilibili"),
                                  QStringLiteral("https://www.bilibili.com/video/BV1")));
    records.append(record);

    // providerID wins when both members are present.
    QJsonObject both = record_object(QStringLiteral("bbbbbbbb-0000-4000-8000-000000000000"), 1.0);
    QJsonObject identity = identity_object(QStringLiteral("providerID"), QStringLiteral("plex"),
                                           QStringLiteral("https://example.com/video"));
    identity.insert(QStringLiteral("provider"), QStringLiteral("youtube"));
    both.insert(QStringLiteral("remoteVideoIdentity"), identity);
    records.append(both);

    QVERIFY(write_records(mining_history_path(root), records));

    const std::vector<MiningHistoryItem> items = load(root);
    QCOMPARE(items.size(), std::size_t(2));
    const std::vector<std::string> expected = {"bilibili", "plex"};
    QCOMPARE(provider_ids_of(items), expected);
}

void MiningHistoryTest::a_ytdlp_provider_on_a_youtube_url_becomes_youtube() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    QJsonArray records;
    const QString id = QStringLiteral("aaaaaaaa-0000-4000-8000-000000000000");

    QJsonObject watch = record_object(id, 0.0);
    watch.insert(QStringLiteral("remoteVideoIdentity"),
                 identity_object(QStringLiteral("providerID"), QStringLiteral("ytdlp"),
                                 QStringLiteral("https://www.youtube.com/watch?v=dQw4w9WgXcQ")));
    records.append(watch);

    QJsonObject short_link = record_object(QStringLiteral("bbbbbbbb-0000-4000-8000-000000000000"), 1.0);
    short_link.insert(QStringLiteral("remoteVideoIdentity"),
                      identity_object(QStringLiteral("providerID"), QStringLiteral("ytdlp"),
                                      QStringLiteral("https://youtu.be/dQw4w9WgXcQ")));
    records.append(short_link);

    QJsonObject other_host = record_object(QStringLiteral("cccccccc-0000-4000-8000-000000000000"), 2.0);
    other_host.insert(QStringLiteral("remoteVideoIdentity"),
                      identity_object(QStringLiteral("providerID"), QStringLiteral("ytdlp"),
                                      QStringLiteral("https://example.com/watch?v=dQw4w9WgXcQ")));
    records.append(other_host);

    // The canonical URL is preferred over the original one.
    QJsonObject canonical = record_object(QStringLiteral("dddddddd-0000-4000-8000-000000000000"), 3.0);
    QJsonObject canonical_identity = identity_object(
        QStringLiteral("providerID"), QStringLiteral("ytdlp"),
        QStringLiteral("https://example.com/watch?v=dQw4w9WgXcQ"));
    canonical_identity.insert(QStringLiteral("canonicalURL"),
                              QStringLiteral("https://www.youtube.com/watch?v=dQw4w9WgXcQ"));
    canonical.insert(QStringLiteral("remoteVideoIdentity"), canonical_identity);
    records.append(canonical);

    QVERIFY(write_records(mining_history_path(root), records));

    const std::vector<MiningHistoryItem> items = load(root);
    QCOMPARE(items.size(), std::size_t(4));
    const std::vector<std::string> expected = {"youtube", "youtube", "ytdlp", "youtube"};
    QCOMPARE(provider_ids_of(items), expected);
}

void MiningHistoryTest::add_orders_by_created_at_then_id() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    const MiningHistoryItem later =
        base_item("bbbbbbbb-0000-4000-8000-000000000000", reference_seconds(1760000100.0));
    const MiningHistoryItem earlier =
        base_item("aaaaaaaa-0000-4000-8000-000000000000", reference_seconds(1760000000.0));
    const MiningHistoryItem same_moment =
        base_item("cccccccc-0000-4000-8000-000000000000", reference_seconds(1760000000.0));

    QVERIFY(add(root, later, 25));
    QVERIFY(add(root, same_moment, 25));
    QVERIFY(add(root, earlier, 25));

    const std::vector<MiningHistoryItem> items = load(root);
    QCOMPARE(items.size(), std::size_t(3));
    const std::vector<std::string> expected = {
        earlier.id,
        same_moment.id,
        later.id,
    };
    QCOMPARE(ids_of(items), expected);
}

void MiningHistoryTest::add_prunes_the_oldest_records() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    const MiningHistoryItem oldest =
        base_item("aaaaaaaa-0000-4000-8000-000000000000", reference_seconds(1760000000.0));
    const MiningHistoryItem middle =
        base_item("bbbbbbbb-0000-4000-8000-000000000000", reference_seconds(1760000100.0));
    const MiningHistoryItem newest =
        base_item("cccccccc-0000-4000-8000-000000000000", reference_seconds(1760000200.0));

    QVERIFY(add(root, middle, 2));
    QVERIFY(add(root, oldest, 2));
    QVERIFY(add(root, newest, 2));

    const std::vector<MiningHistoryItem> items = load(root);
    QCOMPARE(items.size(), std::size_t(2));
    const std::vector<std::string> expected = {middle.id, newest.id};
    QCOMPARE(ids_of(items), expected);
}

void MiningHistoryTest::add_with_zero_limit_stores_an_empty_list() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    QVERIFY(add(root, full_item(), 25));
    QCOMPARE(load(root).size(), std::size_t(1));

    QVERIFY(add(root, full_item(), 0));
    QVERIFY(load(root).empty());

    const QJsonDocument document = read_document(mining_history_path(root));
    QVERIFY(document.isArray());
    QVERIFY(document.array().isEmpty());
}

void MiningHistoryTest::remove_deletes_only_the_given_id() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    const MiningHistoryItem first =
        base_item("aaaaaaaa-0000-4000-8000-000000000000", reference_seconds(1760000000.0));
    const MiningHistoryItem second =
        base_item("bbbbbbbb-0000-4000-8000-000000000000", reference_seconds(1760000100.0));
    const MiningHistoryItem third =
        base_item("cccccccc-0000-4000-8000-000000000000", reference_seconds(1760000200.0));
    QVERIFY(save(root, {first, second, third}));

    QVERIFY(remove(root, second.id));

    const std::vector<MiningHistoryItem> items = load(root);
    QCOMPARE(items.size(), std::size_t(2));
    const std::vector<std::string> expected = {first.id, third.id};
    QCOMPARE(ids_of(items), expected);
}

void MiningHistoryTest::remove_of_an_absent_id_leaves_the_file_alone() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    const MiningHistoryItem item =
        base_item("aaaaaaaa-0000-4000-8000-000000000000", reference_seconds(1760000000.0));
    QVERIFY(save(root, {item}));
    const QByteArray before = read_bytes(mining_history_path(root));
    QVERIFY(!before.isEmpty());

    const bool removed = remove(root, "dddddddd-0000-4000-8000-000000000000");
    QVERIFY(!removed);
    QCOMPARE(read_bytes(mining_history_path(root)), before);
}

void MiningHistoryTest::clear_stores_an_empty_list() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    QVERIFY(save(root, {full_item()}));
    QVERIFY(clear(root));
    QVERIFY(load(root).empty());

    const QJsonDocument document = read_document(mining_history_path(root));
    QVERIFY(document.isArray());
    QVERIFY(document.array().isEmpty());
}

void MiningHistoryTest::a_missing_file_reads_as_an_empty_list() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);

    QVERIFY(load(root).empty());
    QVERIFY(!std::filesystem::exists(mining_history_path(root)));
}

void MiningHistoryTest::a_damaged_or_foreign_document_reads_as_an_empty_list() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root = test_directory(directory);
    const std::filesystem::path path = mining_history_path(root);

    const QByteArray damaged = QByteArrayLiteral("[{\"id\": ");
    QVERIFY(write_bytes(path, damaged));
    QVERIFY(load(root).empty());
    QCOMPARE(read_bytes(path), damaged);

    // A damaged file is not replaced by a write either; recovery needs the file
    // to be removed first.
    QVERIFY(!add(root, full_item(), 25));
    QCOMPARE(read_bytes(path), damaged);
    std::filesystem::remove(path);
    QVERIFY(add(root, full_item(), 25));
    QCOMPARE(load(root).size(), std::size_t(1));

    // An object without an "items" array is not the stored shape.
    QJsonObject foreign;
    foreign.insert(QStringLiteral("records"), QJsonArray());
    QVERIFY(write_bytes(path, QJsonDocument(foreign).toJson(QJsonDocument::Indented)));
    QVERIFY(load(root).empty());

    // One member of the wrong type rejects the document, as decoding does.
    QJsonObject record = record_object(QStringLiteral("11111111-2222-4333-8444-555555555555"), 0.0);
    record.insert(QStringLiteral("createdAt"), QStringLiteral("yesterday"));
    QJsonArray records;
    records.append(record);
    QVERIFY(write_records(path, records));
    QVERIFY(load(root).empty());
}

QTEST_APPLESS_MAIN(MiningHistoryTest)
#include "tst_mining_history.moc"
