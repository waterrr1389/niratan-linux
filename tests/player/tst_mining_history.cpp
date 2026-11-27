// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/player/mining_history.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTest>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using niratan::player::add;
using niratan::player::clear;
using niratan::player::load;
using niratan::player::mining_history_path;
using niratan::player::MiningHistoryItem;
using niratan::player::MiningHistoryMediaKind;
using niratan::player::MiningHistoryNavigationStatus;
using niratan::player::remove;
using niratan::player::resolve_mining_history_navigation;
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

// Writes a file and returns its standardized absolute path, the form the
// resolver reports.
QString create_file(const std::filesystem::path& path) {
    QDir().mkpath(QString::fromStdString(path.parent_path().string()));
    QFile file(QString::fromStdString(path.string()));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QByteArrayLiteral("x"));
        file.close();
    }
    return QFileInfo(QString::fromStdString(path.string())).canonicalFilePath();
}

MiningHistoryItem resolver_item(const std::filesystem::path& video,
                                const std::filesystem::path& subtitle) {
    MiningHistoryItem item = base_item("resolver", 0.0);
    item.video_file_name = video.filename().string();
    item.video_title = item.video_file_name;
    item.video_path = video.string();
    item.subtitle_source_name = subtitle.filename().string();
    item.subtitle_source_path = subtitle.string();
    item.subtitle_format = "srt";
    item.cue_start = 12.5;
    item.cue_end = 14.0;
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
    void named_history_date_thresholds();
    void resolver_reports_a_ready_local_destination();
    void resolver_reports_missing_video_and_subtitle();
    void resolver_returns_a_remote_destination();
    void resolver_pathless_record_uses_the_current_video();
};

// VideoMiningHistorySidebar.historyRow uses a named relative date.
void MiningHistoryTest::named_history_date_thresholds() {
    const QLocale locale(QLocale::English, QLocale::UnitedStates);
    QLocale::setDefault(locale);
    const QDateTime now(QDate(2026, 11, 21), QTime(12, 0), QTimeZone::UTC);
    const double created_at_now = reference_seconds(now.toSecsSinceEpoch());
    const auto label = [created_at_now, &now](double age) {
        return niratan::player::mining_history_date_label(created_at_now - age, now);
    };
    QCOMPARE(label(-1), QStringLiteral("Just now"));
    QCOMPARE(label(59.999), QStringLiteral("Just now"));
    QCOMPARE(label(60), QStringLiteral("1 min ago"));
    QCOMPARE(label(3599.999), QStringLiteral("59 min ago"));
    QCOMPARE(label(3600), QStringLiteral("1 hour ago"));
    QCOMPARE(label(86399.999), QStringLiteral("23 hours ago"));
    QCOMPARE(label(86400), QStringLiteral("Yesterday"));
    QCOMPARE(label(129600), QStringLiteral("Yesterday"));
    QCOMPARE(label(129600.001), locale.toString(QDate(2026, 11, 19), QLocale::ShortFormat));
}

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

// VideoMiningHistoryNavigation.swift: an existing video and external subtitle
// resolve to local media, the standardized paths and the delayed seek time.
void MiningHistoryTest::resolver_reports_a_ready_local_destination() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root(directory.path().toStdString());
    const QString video = create_file(root / "Episode 1.mkv");
    const QString subtitle = create_file(root / "Episode 1.ja.srt");
    QVERIFY(!video.isEmpty());
    QVERIFY(!subtitle.isEmpty());

    const MiningHistoryItem item = resolver_item(video.toStdString(), subtitle.toStdString());
    const auto resolution = resolve_mining_history_navigation(item, QString(), 0.5);
    QVERIFY(resolution.status == MiningHistoryNavigationStatus::ready);
    QVERIFY(resolution.destination.media.kind == MiningHistoryMediaKind::local_file);
    QCOMPARE(resolution.destination.media.path, video);
    QCOMPARE(resolution.destination.subtitle_path.value_or(QString()), subtitle);
    QVERIFY(!resolution.destination.embedded_subtitle_track_id.has_value());
    QCOMPARE(resolution.destination.seek_time, 13.0);

    // The reported paths are absolute even when the record stores relative
    // ones, as URL.standardizedFileURL is.
    MiningHistoryItem relative = item;
    relative.video_path = std::filesystem::relative(std::filesystem::path(item.video_path.value()),
                                                    std::filesystem::current_path())
                              .string();
    const auto relative_resolution = resolve_mining_history_navigation(relative, QString(), 0.0);
    QVERIFY(relative_resolution.status == MiningHistoryNavigationStatus::ready);
    QCOMPARE(relative_resolution.destination.media.path, video);

    // A record with no subtitle source is ready without one.
    MiningHistoryItem silent = item;
    silent.subtitle_source_path.reset();
    silent.subtitle_format.reset();
    const auto silent_resolution = resolve_mining_history_navigation(silent, QString(), 0.0);
    QVERIFY(silent_resolution.status == MiningHistoryNavigationStatus::ready);
    QVERIFY(!silent_resolution.destination.subtitle_path.has_value());
    QCOMPARE(silent_resolution.destination.seek_time, 12.5);

    // The embedded track travels with a ready destination and has no file,
    // even when a subtitle path is stored but missing.
    MiningHistoryItem embedded = item;
    embedded.subtitle_source_path = (root / "Missing.srt").string();
    embedded.subtitle_format = "embedded";
    embedded.embedded_subtitle_track_id = 7;
    const auto embedded_resolution = resolve_mining_history_navigation(embedded, QString(), 0.0);
    QVERIFY(embedded_resolution.status == MiningHistoryNavigationStatus::ready);
    QVERIFY(!embedded_resolution.destination.subtitle_path.has_value());
    QCOMPARE(embedded_resolution.destination.embedded_subtitle_track_id.value_or(-1), 7);
}

// VideoMiningHistoryNavigationResolver.resolve reports each missing source
// before any ready destination, in its guard order.
void MiningHistoryTest::resolver_reports_missing_video_and_subtitle() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root(directory.path().toStdString());
    const QString video = create_file(root / "Episode 1.mkv");
    const QString subtitle = create_file(root / "Episode 1.ja.srt");
    const MiningHistoryItem item = resolver_item(video.toStdString(), subtitle.toStdString());

    MiningHistoryItem missing_video = item;
    missing_video.video_path = (root / "Missing.mkv").string();
    QVERIFY(resolve_mining_history_navigation(missing_video, QString(), 0.0).status ==
            MiningHistoryNavigationStatus::missing_video);

    MiningHistoryItem missing_subtitle = item;
    missing_subtitle.subtitle_source_path = (root / "Missing.srt").string();
    QVERIFY(resolve_mining_history_navigation(missing_subtitle, QString(), 0.0).status ==
            MiningHistoryNavigationStatus::missing_subtitle);

    // A named format with no stored file cannot be restored either.
    MiningHistoryItem format_only = item;
    format_only.subtitle_source_path.reset();
    format_only.subtitle_format = "srt";
    QVERIFY(resolve_mining_history_navigation(format_only, QString(), 0.0).status ==
            MiningHistoryNavigationStatus::missing_subtitle);

    // The video is checked before the subtitle.
    MiningHistoryItem both_missing = missing_subtitle;
    both_missing.video_path = (root / "Missing.mkv").string();
    QVERIFY(resolve_mining_history_navigation(both_missing, QString(), 0.0).status ==
            MiningHistoryNavigationStatus::missing_video);
}

// A remote identity resolves to remote media without filesystem checks and
// without a subtitle file.
void MiningHistoryTest::resolver_returns_a_remote_destination() {
    MiningHistoryItem item = base_item("remote", 0.0);
    item.video_path.reset();
    item.subtitle_source_path.reset();
    item.subtitle_format.reset();
    item.cue_start = 12.5;

    MiningHistoryItem::RemoteVideoIdentity identity;
    identity.provider_id = "youtube";
    identity.remote_id = "dQw4w9WgXcQ";
    identity.original_url = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";
    identity.canonical_url = identity.original_url;
    identity.title = "Remote Episode";
    identity.duration = 42.5;
    item.remote_video_identity = identity;

    const auto resolution = resolve_mining_history_navigation(item, QString(), 0.5);
    QVERIFY(resolution.status == MiningHistoryNavigationStatus::ready);
    QVERIFY(resolution.destination.media.kind == MiningHistoryMediaKind::remote);
    QVERIFY(resolution.destination.media.remote == identity);
    QVERIFY(!resolution.destination.subtitle_path.has_value());
    QVERIFY(!resolution.destination.embedded_subtitle_track_id.has_value());
    QCOMPARE(resolution.destination.seek_time, 13.0);

    // Remote media wins over stored local sources, which are not checked.
    MiningHistoryItem with_paths = item;
    with_paths.video_path = "/does/not/exist.mkv";
    with_paths.subtitle_source_path = "/does/not/exist.srt";
    with_paths.subtitle_format = "srt";
    const auto with_paths_resolution =
        resolve_mining_history_navigation(with_paths, QString(), 0.0);
    QVERIFY(with_paths_resolution.status == MiningHistoryNavigationStatus::ready);
    QVERIFY(with_paths_resolution.destination.media.kind == MiningHistoryMediaKind::remote);
    QVERIFY(!with_paths_resolution.destination.subtitle_path.has_value());
}

// A pathless older record jumps inside a currently open video with the same
// file name; any other current video is the legacy source.
void MiningHistoryTest::resolver_pathless_record_uses_the_current_video() {
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    const QString current =
        create_file(std::filesystem::path(first.path().toStdString()) / "Episode 1.mkv");
    const QString same_name =
        create_file(std::filesystem::path(second.path().toStdString()) / "Episode 1.mkv");
    const QString other =
        create_file(std::filesystem::path(second.path().toStdString()) / "Other.mkv");

    MiningHistoryItem item = base_item("legacy", 0.0);
    item.video_path.reset();
    item.subtitle_source_path.reset();
    item.subtitle_format.reset();
    item.cue_start = 12.5;

    const auto same = resolve_mining_history_navigation(item, current, -20.0);
    QVERIFY(same.status == MiningHistoryNavigationStatus::ready);
    QCOMPARE(same.destination.media.path, current);
    QCOMPARE(same.destination.seek_time, 0.0);

    // Only the file name is compared; the current file can live elsewhere.
    const auto elsewhere = resolve_mining_history_navigation(item, same_name, 0.0);
    QVERIFY(elsewhere.status == MiningHistoryNavigationStatus::ready);
    QCOMPARE(elsewhere.destination.media.path, same_name);

    // The current video is not required to exist; only its file name is
    // compared, and its standardized path is reported.
    const QString absent_current =
        QDir::cleanPath(QFileInfo(QStringLiteral("%1/Missing/Episode 1.mkv").arg(second.path()))
                            .absoluteFilePath());
    QVERIFY(!QFileInfo::exists(absent_current));
    const auto absent = resolve_mining_history_navigation(item, absent_current, 0.0);
    QVERIFY(absent.status == MiningHistoryNavigationStatus::ready);
    QCOMPARE(absent.destination.media.path, absent_current);

    QVERIFY(resolve_mining_history_navigation(item, other, 0.0).status ==
            MiningHistoryNavigationStatus::legacy_source_unavailable);
    QVERIFY(resolve_mining_history_navigation(item, QString(), 0.0).status ==
            MiningHistoryNavigationStatus::legacy_source_unavailable);
}

QTEST_APPLESS_MAIN(MiningHistoryTest)
#include "tst_mining_history.moc"
