// SPDX-License-Identifier: GPL-3.0-or-later
//
// The local video library catalog and folder scanning, following
// Features/Video/VideoLibraryStore.swift, VideoMediaTypes.swift and the
// matching cases of Niratan's script/test_video_library_store.swift
// (testScanFiltersSortsAndDedupesMediaFiles,
// testSuccessfulRescanRemovesStaleItems,
// testFailedScanPreservesExistingItemsAndRecordsError,
// testItemMetadataCollectionsSubtitleBindingAndMissingCleanup,
// testRemovingCollectionKeepsVideoItemsAndFiles,
// testSmartCollectionsPersistAndSurviveMissingCleanup,
// testCatalogRoundTripAndSourceRemoval, testLegacyCatalogDecodesWithEmptyV3Metadata,
// testLegacyManualCollectionsDecodeWithDefaultKind).
//
// Remote items, bookmarks and media servers are out of scope; the document's
// remoteItems member is written empty and ignored on read.

#include "services/storage/json_file.h"
#include "services/video/library_catalog.h"
#include "services/video/media_types.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTest>

#include <filesystem>

using namespace niratan::video;

namespace {

std::filesystem::path fs_path(const QString& path) {
    return std::filesystem::path(path.toStdString());
}

QString make_directory(const QString& path) {
    QDir().mkpath(path);
    return path;
}

QString touch_file(const QString& path, qint64 size = 1, qint64 modified_seconds = 0) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(QByteArray(static_cast<qsizetype>(size), '1'));
    file.flush();
    if (modified_seconds > 0) {
        file.setFileTime(QDateTime::fromSecsSinceEpoch(modified_seconds, QTimeZone::UTC),
                         QFileDevice::FileModificationTime);
    }
    file.close();
    return QFileInfo(path).absoluteFilePath();
}

bool write_bytes(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(bytes) == bytes.size();
}

QByteArray read_bytes(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

// Seconds between 1970-01-01 and Foundation's reference date.
constexpr double kReferenceOffset = 978'307'200;

double reference_seconds(qint64 epoch_seconds) {
    return double(epoch_seconds) - kReferenceOffset;
}

}  // namespace

class LibraryCatalogTest : public QObject {
    Q_OBJECT
   private slots:
    void mediaExtensions() {
        QCOMPARE(
            supported_extensions(),
            QStringList({QStringLiteral("mkv"),  QStringLiteral("webm"), QStringLiteral("avi"),
                         QStringLiteral("m4v"),  QStringLiteral("mp4"),  QStringLiteral("mov"),
                         QStringLiteral("qt"),   QStringLiteral("mpg"),  QStringLiteral("mpeg"),
                         QStringLiteral("ts"),   QStringLiteral("m2ts"), QStringLiteral("mts"),
                         QStringLiteral("3gp"),  QStringLiteral("ogv"),  QStringLiteral("wmv"),
                         QStringLiteral("asf"),  QStringLiteral("flv"),  QStringLiteral("m4b"),
                         QStringLiteral("m4a"),  QStringLiteral("mp3"),  QStringLiteral("flac"),
                         QStringLiteral("opus"), QStringLiteral("ogg"),  QStringLiteral("oga"),
                         QStringLiteral("weba"), QStringLiteral("wav"),  QStringLiteral("aac"),
                         QStringLiteral("aiff"), QStringLiteral("aif"),  QStringLiteral("ape"),
                         QStringLiteral("wv")}));
        QVERIFY(is_media_file(QStringLiteral("/videos/Episode.MKV")));
        QVERIFY(is_media_file(QStringLiteral("/videos/episode.mp4")));
        QVERIFY(is_media_file(QStringLiteral("/videos/audiobook.m4b")));
        QVERIFY(!is_media_file(QStringLiteral("/videos/notes.txt")));
        QVERIFY(!is_media_file(QStringLiteral("/videos/E01.srt")));
        QVERIFY(!is_media_file(QStringLiteral("/videos/.mp4")));
        QVERIFY(!is_media_file(QStringLiteral("/videos/Episode")));
    }

    void scanFiltersSortsAndDedupesMediaFiles() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("source"));
        make_directory(root + QStringLiteral("/Season 1"));
        make_directory(root + QStringLiteral("/.hidden"));
        const QString episode2 =
            touch_file(root + QStringLiteral("/Season 1/Episode 2.mkv"), 20, 20);
        const QString episode1 = touch_file(root + QStringLiteral("/Episode 1.mp4"), 10, 10);
        touch_file(root + QStringLiteral("/Episode 10.MKV"), 5, 5);
        touch_file(root + QStringLiteral("/notes.txt"));
        touch_file(root + QStringLiteral("/Episode 1.srt"));
        touch_file(root + QStringLiteral("/.hidden/Hidden Episode.mp4"));
        touch_file(root + QStringLiteral("/.hidden.mp4"));

        LibraryStore store(fs_path(dir.filePath("library.json")));
        const auto source = store.add_source(fs_path(root));
        QCOMPARE(store.catalog().sources.size(), 1);
        QCOMPARE(source.name, QStringLiteral("source"));
        QCOMPARE(source.path, QFileInfo(root).absoluteFilePath());
        QVERIFY(!source.last_scanned_at.has_value());
        QVERIFY(!source.last_error.has_value());

        const auto now = QDateTime::fromSecsSinceEpoch(500, QTimeZone::UTC);
        QString error;
        QVERIFY2(store.scan_source(source.id, &error, now), qPrintable(error));
        QStringList titles;
        QList<qint64> sizes;
        for (const auto& item : store.catalog().items) {
            titles.append(item.title);
            sizes.append(item.file_size);
            QCOMPARE(item.source_id, source.id);
            QVERIFY(!item.path.contains(QStringLiteral(".hidden")));
        }
        QCOMPARE(titles,
                 QStringList({QStringLiteral("Episode 1"), QStringLiteral("Episode 2"),
                              QStringLiteral("Episode 10")}));
        QCOMPARE(sizes, (QList<qint64>{10, 20, 5}));
        QVERIFY(store.catalog().items[0].path == episode1);
        QVERIFY(store.catalog().items[1].path == episode2);
        QCOMPARE(store.catalog().items[0].parent_folder, QStringLiteral("source"));
        QCOMPARE(store.catalog().items[1].parent_folder, QStringLiteral("Season 1"));
        QVERIFY(store.catalog().items[0].modified_at.has_value());
        QCOMPARE(store.catalog().items[0].modified_at->toSecsSinceEpoch(), qint64(10));
        QCOMPARE(store.catalog().items[0].last_seen_at.toSecsSinceEpoch(), qint64(500));
        QCOMPARE(store.catalog().sources[0].last_scanned_at->toSecsSinceEpoch(), qint64(500));

        const auto duplicate = store.add_source(fs_path(root));
        QCOMPARE(duplicate.id, source.id);
        QCOMPARE(store.catalog().sources.size(), 1);

        const QString other = make_directory(dir.filePath("Alpha"));
        const auto alpha = store.add_source(fs_path(other));
        QCOMPARE(store.catalog().sources.size(), 2);
        QCOMPARE(store.catalog().sources[0].name, QStringLiteral("Alpha"));
        QCOMPARE(store.catalog().sources[1].name, QStringLiteral("source"));
        QCOMPARE(alpha.path, QFileInfo(other).absoluteFilePath());
    }

    void successfulRescanRemovesStaleItems() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("stale"));
        const QString episode = touch_file(root + QStringLiteral("/Episode.mkv"));

        LibraryStore store(fs_path(dir.filePath("library.json")));
        const auto source = store.add_source(fs_path(root));
        QVERIFY(store.scan_source(source.id));
        QCOMPARE(store.catalog().items.size(), 1);

        QVERIFY(QFile::remove(episode));
        QVERIFY(store.scan_source(source.id));
        QVERIFY(store.catalog().items.isEmpty());
    }

    void failedScanPreservesItemsAndRecordsError() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("failure"));
        touch_file(root + QStringLiteral("/Episode.mkv"));

        LibraryStore store(fs_path(dir.filePath("library.json")));
        const auto source = store.add_source(fs_path(root));
        QVERIFY(store.scan_source(source.id));
        QCOMPARE(store.catalog().items.size(), 1);

        QString error;
        QVERIFY(!store.scan_source(QUuid::createUuid(), &error));
        QCOMPARE(error, QStringLiteral("Video folder not found."));

        QVERIFY(QDir(root).removeRecursively());
        error.clear();
        QVERIFY(!store.scan_source(source.id, &error));
        QCOMPARE(error, QStringLiteral("Video folder is no longer available. %1").arg(source.path));
        QCOMPARE(store.catalog().items.size(), 1);
        QCOMPARE(store.catalog().sources.first().last_error.value_or(QString()), error);

        LibraryStore reloaded(fs_path(dir.filePath("library.json")));
        QCOMPARE(reloaded.catalog().items.size(), 1);
        QCOMPARE(reloaded.catalog().sources.first().last_error.value_or(QString()), error);

        const QString later = make_directory(dir.filePath("Z Later"));
        const QString later_episode = touch_file(later + QStringLiteral("/New.mkv"));
        const auto later_source = store.add_source(fs_path(later));
        QVERIFY(!store.scan_all_sources(&error));
        QCOMPARE(error, QStringLiteral("Video folder is no longer available. %1").arg(source.path));
        std::optional<LibrarySource> later_entry;
        for (const auto& entry : store.catalog().sources) {
            if (entry.id == later_source.id) {
                later_entry = entry;
            }
        }
        QVERIFY(later_entry.has_value());
        QVERIFY(later_entry->last_scanned_at.has_value());
        QVERIFY(!later_entry->last_error.has_value());
        QStringList paths;
        for (const auto& item : store.catalog().items) {
            paths.append(item.path);
        }
        QVERIFY(paths.contains(QFileInfo(later_episode).absoluteFilePath()));
    }

    void metadataCollectionsSubtitleAndMissingCleanup() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("metadata"));
        const QString show = make_directory(root + QStringLiteral("/Show A"));
        const QString episode = touch_file(show + QStringLiteral("/Episode 01.mkv"));
        const QString subtitle = touch_file(show + QStringLiteral("/Episode 01.ja.srt"));

        const auto catalog_path = fs_path(dir.filePath("library.json"));
        LibraryStore store(catalog_path);
        const auto source = store.add_source(fs_path(root));
        QVERIFY(store.scan_source(source.id));
        QCOMPARE(store.catalog().items.size(), 1);
        const QString path = store.catalog().items[0].path;

        store.set_display_title(path, QStringLiteral("Custom Episode"));
        store.set_favorite(path, true);
        store.set_tags(path, {QStringLiteral("Anime"), QStringLiteral("Listening")});
        store.bind_subtitle(path, QFileInfo(subtitle).absoluteFilePath());
        const auto collection = store.create_collection(QStringLiteral("Weekend"),
                                                        {path, path, QStringLiteral("/gone.mkv")});

        QCOMPARE(collection.name, QStringLiteral("Weekend"));
        QCOMPARE(collection.kind, CollectionKind::manual);
        QCOMPARE(collection.item_paths, QStringList({path}));
        QCOMPARE(store.metadata_for_path(path).collection_ids, (QList<QUuid>{collection.id}));

        LibraryStore reloaded(catalog_path);
        const auto metadata = reloaded.metadata_for_path(path);
        QCOMPARE(metadata.display_title.value_or(QString()), QStringLiteral("Custom Episode"));
        QVERIFY(metadata.is_favorite);
        QCOMPARE(metadata.tags,
                 QStringList({QStringLiteral("Anime"), QStringLiteral("Listening")}));
        QCOMPARE(metadata.bound_subtitle_path.value_or(QString()),
                 QFileInfo(subtitle).absoluteFilePath());
        QCOMPARE(reloaded.catalog().collections.size(), 1);
        QCOMPARE(reloaded.catalog().collections.first().name, QStringLiteral("Weekend"));
        QCOMPARE(reloaded.catalog().collections.first().item_paths, QStringList({path}));

        reloaded.set_tags(path,
                          {QStringLiteral("Caf\u00e9"), QStringLiteral("cafe"),
                           QStringLiteral(" CAFE "), QStringLiteral("")});
        QCOMPARE(reloaded.metadata_for_path(path).tags, QStringList({QStringLiteral("Caf\u00e9")}));

        reloaded.set_display_title(path, QStringLiteral("   "));
        QVERIFY(!reloaded.metadata_for_path(path).display_title.has_value());
        reloaded.set_display_title(path, QStringLiteral("Custom Episode"));

        QCOMPARE(reloaded.remove_missing_items(), 0);

        QVERIFY(QFile::remove(episode));
        QCOMPARE(reloaded.remove_missing_items(source.id), 1);
        QVERIFY(reloaded.catalog().items.isEmpty());
        QVERIFY(reloaded.metadata_for_path(path).empty());
        QVERIFY(reloaded.catalog().collections.first().item_paths.isEmpty());
    }

    void removingCollectionKeepsVideoItemsAndFiles() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("remove-collection"));
        const QString episode = touch_file(root + QStringLiteral("/Episode 01.mkv"));

        LibraryStore store(fs_path(dir.filePath("library.json")));
        const auto source = store.add_source(fs_path(root));
        QVERIFY(store.scan_source(source.id));
        const QString path = store.catalog().items[0].path;

        store.set_display_title(path, QStringLiteral("Keep Me"));
        const auto collection = store.create_collection(QStringLiteral("Weekend"), {path});
        QCOMPARE(store.metadata_for_path(path).collection_ids, (QList<QUuid>{collection.id}));

        store.remove_collection(collection.id);

        QVERIFY(store.catalog().collections.isEmpty());
        QCOMPARE(store.catalog().items.size(), 1);
        QCOMPARE(store.catalog().items.first().path, path);
        QVERIFY(QFileInfo::exists(episode));
        const auto metadata = store.metadata_for_path(path);
        QCOMPARE(metadata.display_title.value_or(QString()), QStringLiteral("Keep Me"));
        QVERIFY(metadata.collection_ids.isEmpty());
    }

    void smartCollectionsPersistAndSurviveMissingCleanup() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("smart"));
        const QString episode = touch_file(root + QStringLiteral("/Frieren 01.mkv"));

        const auto catalog_path = fs_path(dir.filePath("library.json"));
        LibraryStore store(catalog_path);
        const auto source = store.add_source(fs_path(root));
        QVERIFY(store.scan_source(source.id));

        SmartRule rule;
        rule.id = QUuid::createUuid();
        rule.field = SmartRuleField::file_name;
        rule.match = SmartRuleMatch::contains;
        rule.value = QStringLiteral("  Frieren  ");
        const auto smart = store.create_smart_collection(QStringLiteral("Frieren"), {rule});
        QCOMPARE(smart.kind, CollectionKind::smart);
        QCOMPARE(smart.name, QStringLiteral("Frieren"));
        QVERIFY(smart.item_paths.isEmpty());
        QCOMPARE(smart.smart_rules.size(), 1);
        QCOMPARE(smart.smart_rules.first().value, QStringLiteral("Frieren"));
        QCOMPARE(smart.smart_rules.first().id, rule.id);

        LibraryStore reloaded(catalog_path);
        QCOMPARE(reloaded.catalog().collections.size(), 1);
        QCOMPARE(reloaded.catalog().collections.first().kind, CollectionKind::smart);
        QCOMPARE(reloaded.catalog().collections.first().smart_rules, smart.smart_rules);
        QVERIFY(reloaded.catalog().collections.first().item_paths.isEmpty());

        QVERIFY(QFile::remove(episode));
        QCOMPARE(reloaded.remove_missing_items(source.id), 1);
        QCOMPARE(reloaded.catalog().collections.first().smart_rules, smart.smart_rules);

        const auto untitled = reloaded.create_smart_collection(QStringLiteral("  "), {});
        QCOMPARE(untitled.name, QStringLiteral("Untitled Collection"));
        QCOMPARE(reloaded.catalog().collections.size(), 2);
        QCOMPARE(reloaded.catalog().collections[0].name, QStringLiteral("Frieren"));
        QCOMPARE(reloaded.catalog().collections[1].name, QStringLiteral("Untitled Collection"));
    }

    void catalogRoundTripAndSourceRemoval() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("roundtrip"));
        touch_file(root + QStringLiteral("/Movie.webm"));
        const auto catalog_path = fs_path(dir.filePath("library.json"));

        LibraryStore store(catalog_path);
        const auto source = store.add_source(fs_path(root));
        QVERIFY(store.scan_source(source.id));
        const QString path = store.catalog().items[0].path;
        store.set_display_title(path, QStringLiteral("Movie Title"));
        store.create_collection(QStringLiteral("Films"), {path});

        LibraryStore reloaded(catalog_path);
        QCOMPARE(reloaded.catalog().sources.size(), 1);
        QCOMPARE(reloaded.catalog().sources.first().path, QFileInfo(root).absoluteFilePath());
        QCOMPARE(reloaded.catalog().items.size(), 1);
        QCOMPARE(reloaded.catalog().items.first().title, QStringLiteral("Movie"));

        reloaded.remove_source(source.id);
        QVERIFY(reloaded.catalog().sources.isEmpty());
        QVERIFY(reloaded.catalog().items.isEmpty());
        QCOMPARE(reloaded.catalog().collections.size(), 1);
        QCOMPARE(reloaded.catalog().collections.first().name, QStringLiteral("Films"));
        QVERIFY(reloaded.catalog().collections.first().item_paths.isEmpty());
        QVERIFY(reloaded.metadata_for_path(path).empty());

        LibraryStore empty(catalog_path);
        QVERIFY(empty.catalog().sources.isEmpty());
        QVERIFY(empty.catalog().items.isEmpty());
        QCOMPARE(empty.catalog().collections.size(), 1);
    }

    void legacyCatalogDecodesWithDefaults() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QUuid source_id = QUuid::createUuid();
        const QUuid collection_id = QUuid::createUuid();
        const QString legacy = QStringLiteral("{\n"
                                              "  \"sources\" : [\n"
                                              "    {\n"
                                              "      \"id\" : \"%1\",\n"
                                              "      \"name\" : \"Legacy\",\n"
                                              "      \"path\" : \"/tmp/Legacy\",\n"
                                              "      \"bookmark\" : \"\"\n"
                                              "    }\n"
                                              "  ],\n"
                                              "  \"items\" : [\n"
                                              "    {\n"
                                              "      \"path\" : \"/tmp/Legacy/one.mkv\",\n"
                                              "      \"sourceID\" : \"%1\",\n"
                                              "      \"title\" : \"one\",\n"
                                              "      \"parentFolder\" : \"Legacy\",\n"
                                              "      \"fileSize\" : 5,\n"
                                              "      \"lastSeenAt\" : 0\n"
                                              "    }\n"
                                              "  ],\n"
                                              "  \"collections\" : [\n"
                                              "    {\n"
                                              "      \"id\" : \"%2\",\n"
                                              "      \"name\" : \"Weekend\",\n"
                                              "      \"itemPaths\" : [\n"
                                              "        \"/tmp/one.mkv\"\n"
                                              "      ]\n"
                                              "    }\n"
                                              "  ]\n"
                                              "}\n")
                                   .arg(source_id.toString(QUuid::WithoutBraces).toUpper(),
                                        collection_id.toString(QUuid::WithoutBraces).toUpper());
        const QString file = dir.filePath("legacy.json");
        QVERIFY(write_bytes(file, legacy.toUtf8()));

        LibraryStore store(fs_path(file));
        QCOMPARE(store.catalog().sources.size(), 1);
        QCOMPARE(store.catalog().sources.first().id, source_id);
        QVERIFY(!store.catalog().sources.first().last_scanned_at.has_value());
        QVERIFY(!store.catalog().sources.first().last_error.has_value());
        QCOMPARE(store.catalog().items.size(), 1);
        QCOMPARE(store.catalog().items.first().title, QStringLiteral("one"));
        QVERIFY(store.catalog().item_metadata_by_path.isEmpty());
        QCOMPARE(store.catalog().collections.size(), 1);
        QCOMPARE(store.catalog().collections.first().kind, CollectionKind::manual);
        QVERIFY(store.catalog().collections.first().smart_rules.isEmpty());
        QCOMPARE(store.catalog().collections.first().item_paths,
                 QStringList({QStringLiteral("/tmp/one.mkv")}));
    }

    void catalogDocumentShape() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("shape"));
        const QString episode = touch_file(root + QStringLiteral("/Episode 01.mkv"), 7, 20);
        const QString catalog_file = dir.filePath("library.json");

        LibraryStore store(fs_path(catalog_file));
        const auto source = store.add_source(fs_path(root));
        const auto now = QDateTime::fromSecsSinceEpoch(1000, QTimeZone::UTC);
        QVERIFY(store.scan_source(source.id, nullptr, now));
        const QString path = store.catalog().items[0].path;
        store.set_display_title(path, QStringLiteral("Title"));
        store.create_collection(QStringLiteral("Films"), {path});

        const auto read = niratan::storage::read_json_file(fs_path(catalog_file));
        QCOMPARE(read.status, niratan::storage::JsonReadStatus::success);
        const auto document = read.document.object();
        QStringList members = document.keys();
        members.sort();
        QCOMPARE(members,
                 QStringList({QStringLiteral("collections"), QStringLiteral("itemMetadataByPath"),
                              QStringLiteral("items"), QStringLiteral("remoteItems"),
                              QStringLiteral("sources")}));
        QVERIFY(document.value("remoteItems").isArray());
        QVERIFY(document.value("remoteItems").toArray().isEmpty());

        const auto source_object = document.value("sources").toArray().first().toObject();
        QVERIFY(!source_object.contains(QStringLiteral("bookmark")));
        QCOMPARE(source_object.value("id").toString(),
                 source.id.toString(QUuid::WithoutBraces).toUpper());
        QCOMPARE(source_object.value("path").toString(), QFileInfo(root).absoluteFilePath());
        QCOMPARE(source_object.value("lastScannedAt").toDouble(), reference_seconds(1000));
        QVERIFY(!source_object.contains(QStringLiteral("lastError")));

        const auto item = document.value("items").toArray().first().toObject();
        QCOMPARE(item.value("path").toString(), QFileInfo(episode).absoluteFilePath());
        QCOMPARE(item.value("title").toString(), QStringLiteral("Episode 01"));
        QCOMPARE(item.value("fileSize").toDouble(), 7.0);
        QCOMPARE(item.value("modifiedAt").toDouble(), reference_seconds(20));
        QCOMPARE(item.value("lastSeenAt").toDouble(), reference_seconds(1000));
        QCOMPARE(item.value("parentFolder").toString(), QStringLiteral("shape"));

        const auto metadata =
            document.value("itemMetadataByPath").toObject().value(path).toObject();
        QCOMPARE(metadata.value("displayTitle").toString(), QStringLiteral("Title"));
        QCOMPARE(metadata.value("isFavorite").toBool(), false);

        QVERIFY(write_bytes(catalog_file, QByteArrayLiteral("not a json document")));
        const QByteArray damaged = read_bytes(catalog_file);
        LibraryStore damaged_store(fs_path(catalog_file));
        QVERIFY(damaged_store.catalog().sources.isEmpty());
        QVERIFY(damaged_store.catalog().items.isEmpty());
        QCOMPARE(read_bytes(catalog_file), damaged);

        const QString other = make_directory(dir.filePath("other"));
        damaged_store.add_source(fs_path(other));
        QCOMPARE(read_bytes(catalog_file) == damaged, false);
        QVERIFY(niratan::storage::read_json_file(fs_path(catalog_file)).document.isObject());

        QTemporaryDir blocked;
        QVERIFY(blocked.isValid());
        const QString inside = make_directory(blocked.filePath("inside"));
        LibraryStore unwritable(fs_path(blocked.path()));
        const auto kept = unwritable.add_source(fs_path(inside));
        QCOMPARE(unwritable.catalog().sources.size(), 1);
        QCOMPARE(unwritable.catalog().sources.first().id, kept.id);
    }

    void rescanDedupesAcrossSourcesAndClearsError() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("root"));
        const QString show = make_directory(root + QStringLiteral("/Show"));
        const QString episode = touch_file(show + QStringLiteral("/Episode.mkv"), 5, 5);

        LibraryStore store(fs_path(dir.filePath("library.json")));
        const auto root_source = store.add_source(fs_path(root));
        QVERIFY(store.scan_source(root_source.id));
        QCOMPARE(store.catalog().items.size(), 1);

        const auto show_source = store.add_source(fs_path(show));
        QVERIFY(store.scan_source(show_source.id));
        QCOMPARE(store.catalog().items.size(), 1);
        QCOMPARE(store.catalog().items.first().source_id, show_source.id);
        QCOMPARE(store.catalog().items.first().path, QFileInfo(episode).absoluteFilePath());

        QVERIFY(QDir(show).removeRecursively());
        QString error;
        QVERIFY(!store.scan_source(show_source.id, &error));
        std::optional<LibrarySource> failed_source;
        for (const auto& entry : store.catalog().sources) {
            if (entry.id == show_source.id) {
                failed_source = entry;
            }
        }
        QVERIFY(failed_source.has_value());
        QCOMPARE(failed_source->last_error.value_or(QString()), error);

        make_directory(show);
        touch_file(show + QStringLiteral("/Episode.mkv"), 5, 5);
        QVERIFY(store.scan_source(show_source.id));
        std::optional<LibrarySource> recovered_source;
        for (const auto& entry : store.catalog().sources) {
            if (entry.id == show_source.id) {
                recovered_source = entry;
            }
        }
        QVERIFY(recovered_source.has_value());
        QVERIFY(!recovered_source->last_error.has_value());
        QCOMPARE(store.catalog().items.size(), 1);
        QCOMPARE(store.catalog().items.first().path, QFileInfo(episode).absoluteFilePath());
    }

    void clearingMetadataRemovesEntry() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("metadata-clear"));
        touch_file(root + QStringLiteral("/Episode.mkv"));

        const auto catalog_path = fs_path(dir.filePath("library.json"));
        LibraryStore store(catalog_path);
        const auto source = store.add_source(fs_path(root));
        QVERIFY(store.scan_source(source.id));
        const QString path = store.catalog().items[0].path;

        store.set_favorite(path, true);
        QVERIFY(!store.catalog().item_metadata_by_path.isEmpty());
        store.set_favorite(path, false);
        QVERIFY(store.catalog().item_metadata_by_path.isEmpty());

        LibraryStore reloaded(catalog_path);
        QVERIFY(reloaded.catalog().item_metadata_by_path.isEmpty());
        QVERIFY(niratan::storage::read_json_file(catalog_path)
                    .document.object()
                    .value(QStringLiteral("itemMetadataByPath"))
                    .toObject()
                    .isEmpty());
    }

    void updatingCollectionsRewritesKindAndMembership() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = make_directory(dir.filePath("update-collections"));
        const QString first = touch_file(root + QStringLiteral("/A.mkv"), 2, 2);
        const QString second = touch_file(root + QStringLiteral("/B.mkv"), 3, 3);

        const auto catalog_path = fs_path(dir.filePath("library.json"));
        LibraryStore store(catalog_path);
        const auto source = store.add_source(fs_path(root));
        QVERIFY(store.scan_source(source.id));
        const QString path_a = QFileInfo(first).absoluteFilePath();
        const QString path_b = QFileInfo(second).absoluteFilePath();

        SmartRule smart_rule;
        smart_rule.field = SmartRuleField::file_name;
        smart_rule.match = SmartRuleMatch::contains;
        smart_rule.value = QStringLiteral("A");
        const auto collection = store.create_smart_collection(QStringLiteral("Temp"), {smart_rule});

        store.update_collection(collection.id, QStringLiteral(" Weekday "),
                                {path_b, QStringLiteral("/gone.mkv"), path_b});
        const auto updated = store.catalog().collections.first();
        QCOMPARE(updated.name, QStringLiteral("Weekday"));
        QCOMPARE(updated.kind, CollectionKind::manual);
        QCOMPARE(updated.item_paths, QStringList({path_b}));
        QVERIFY(updated.smart_rules.isEmpty());
        QCOMPARE(store.metadata_for_path(path_b).collection_ids, (QList<QUuid>{collection.id}));
        QVERIFY(store.metadata_for_path(path_a).collection_ids.isEmpty());

        SmartRule tag_rule;
        tag_rule.field = SmartRuleField::tag;
        tag_rule.match = SmartRuleMatch::contains;
        tag_rule.value = QStringLiteral("listen");
        store.update_smart_collection(collection.id, QStringLiteral("Temp"), {tag_rule});
        const auto smart = store.catalog().collections.first();
        QCOMPARE(smart.kind, CollectionKind::smart);
        QCOMPARE(smart.name, QStringLiteral("Temp"));
        QVERIFY(smart.item_paths.isEmpty());
        QCOMPARE(smart.smart_rules.size(), 1);
        QCOMPARE(smart.smart_rules.first().value, QStringLiteral("listen"));
        QVERIFY(store.metadata_for_path(path_b).collection_ids.isEmpty());
        QVERIFY(store.catalog().item_metadata_by_path.isEmpty());

        const Collection expected = store.catalog().collections.first();
        LibraryStore reloaded(catalog_path);
        QCOMPARE(reloaded.catalog().collections.first(), expected);
    }

    void missingCleanupFiltersBySource() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString first = make_directory(dir.filePath("Source A"));
        const QString second = make_directory(dir.filePath("Source B"));
        const QString file_a = touch_file(first + QStringLiteral("/A.mkv"));
        const QString file_b = touch_file(second + QStringLiteral("/B.mkv"));

        LibraryStore store(fs_path(dir.filePath("library.json")));
        const auto source_a = store.add_source(fs_path(first));
        const auto source_b = store.add_source(fs_path(second));
        QVERIFY(store.scan_source(source_a.id));
        QVERIFY(store.scan_source(source_b.id));
        QCOMPARE(store.catalog().items.size(), 2);

        QVERIFY(QFile::remove(file_a));
        QVERIFY(QFile::remove(file_b));
        QCOMPARE(store.remove_missing_items(source_a.id), 1);
        QCOMPARE(store.catalog().items.size(), 1);
        QCOMPARE(store.catalog().items.first().source_id, source_b.id);
        QCOMPARE(store.remove_missing_items(), 1);
        QVERIFY(store.catalog().items.isEmpty());
    }
};

QTEST_APPLESS_MAIN(LibraryCatalogTest)
#include "tst_library_catalog.moc"
