// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video library view model's local half, following
// Features/Video/VideoLibraryViewModel.swift and these cases of Niratan's
// script/test_video_library_view_model.swift:
// testContinueWatchingShowsOnlyResumableVideos,
// testSearchMatchesTitleFolderSourceAndPath, testSortOptionsOrderAllVideos,
// testUnfinishedFilterUsesIncompletePlaybackState,
// testSmartFiltersSplitUnwatchedFinishedAndMissingVideos,
// testSourceSummariesCountItemsAndRefreshOneSource,
// testV3OrganizationMetadataSubtitleAndCollections,
// testFoldersViewUsesSourceRelativeParentFolders,
// testSmartCollectionsMatchRulesAndPreviewRows,
// testNeedsReviewExcludesManualAndSmartCollectionRows,
// testV3SelectionBatchActionsAndMissingCleanup,
// testPlaybackStateActionsUpdateRows, testFilteredEmptyStateUsesSearchCopy,
// testPlaybackHistoryRefreshAdvancesRevision,
// testClearingSelectionClosesDetailsInspector.
//
// Remote items, media servers and the YouTube link sheet are out of scope.
// The two @AppStorage values of VideoLibraryView.swift live in settings.json
// through player/video_settings.h.

#include "data_root.h"
#include "services/player/playback_history.h"
#include "services/player/video_settings.h"
#include "services/storage/app_paths.h"
#include "services/storage/json_file.h"
#include "services/video/library_catalog.h"
#include "services/video/thumbnail_store.h"
#include "viewmodels/video/video_library_view_model.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QMutex>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUrl>
#include <QTest>

#include <filesystem>

using niratan::player::PlaybackHistory;
using niratan::video::LibraryStore;
using niratan::viewmodels::VideoLibraryViewModel;

namespace {

std::filesystem::path fs_path(const QString& path) {
    return std::filesystem::path(path.toStdString());
}

QString make_directory(const QString& path) {
    QDir().mkpath(path);
    return path;
}

// The view model, its store and the playback history all resolve from the
// data root, so a test points XDG_DATA_HOME at its own temporary directory.
std::filesystem::path use_data_home(const QString& base) {
    qputenv("XDG_DATA_HOME", base.toUtf8());
    const auto root = niratan::storage::data_root();
    QDir().mkpath(QString::fromStdString(root.string()));
    return root;
}

QString touch_file(const QString& path, qint64 size, qint64 modified_seconds) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(QByteArray(static_cast<qsizetype>(size), '1'));
    file.flush();
    file.setFileTime(QDateTime::fromSecsSinceEpoch(modified_seconds, QTimeZone::UTC),
                     QFileDevice::FileModificationTime);
    file.close();
    return QFileInfo(path).absoluteFilePath();
}

struct Fixture {
    QString base;
    QString source;
    QString alpha;
    QString beta;
    QString gamma;
};

Fixture build_simple_fixture(const QString& base) {
    Fixture fixture;
    fixture.base = base;
    fixture.source = make_directory(base + QStringLiteral("/Anime Source"));
    const QString season1 = make_directory(fixture.source + QStringLiteral("/Season 1"));
    const QString season2 = make_directory(fixture.source + QStringLiteral("/Season 2"));
    fixture.alpha = touch_file(season1 + QStringLiteral("/Alpha.mp4"), 10, 10);
    fixture.beta = touch_file(season2 + QStringLiteral("/Beta.mkv"), 30, 30);
    fixture.gamma = touch_file(fixture.source + QStringLiteral("/Gamma.webm"), 20, 20);
    const auto root = use_data_home(base);

    LibraryStore store(root / "video_library.json");
    const auto source = store.add_source(fs_path(fixture.source));
    store.scan_source(source.id);

    PlaybackHistory history(root / "video_playback_history.json");
    history.save(fixture.alpha, 20, 100, QDateTime::fromSecsSinceEpoch(100, QTimeZone::UTC));
    history.save(fixture.beta, 50, 100, QDateTime::fromSecsSinceEpoch(200, QTimeZone::UTC));
    return fixture;
}

struct OrganizedFixture {
    QString base;
    QString source;
    QString episode1;
    QString episode2;
    QString movie;
    QString subtitle;
};

OrganizedFixture build_organized_fixture(const QString& base) {
    OrganizedFixture fixture;
    fixture.base = base;
    fixture.source = make_directory(base + QStringLiteral("/Library"));
    const QString season1 = make_directory(fixture.source + QStringLiteral("/Show A/Season 1"));
    const QString season2 = make_directory(fixture.source + QStringLiteral("/Show A/Season 2"));
    const QString movies = make_directory(fixture.source + QStringLiteral("/Movies"));
    fixture.episode1 = touch_file(season1 + QStringLiteral("/Episode 01.mp4"), 10, 10);
    fixture.episode2 = touch_file(season2 + QStringLiteral("/Episode 02.mkv"), 20, 20);
    fixture.subtitle = touch_file(season1 + QStringLiteral("/Episode 01.ja.srt"), 1, 11);
    fixture.movie = touch_file(movies + QStringLiteral("/Movie.webm"), 30, 30);
    const auto root = use_data_home(base);

    LibraryStore store(root / "video_library.json");
    const auto source = store.add_source(fs_path(fixture.source));
    store.scan_source(source.id);
    return fixture;
}

QVariantList rows_of(const VideoLibraryViewModel& view_model) {
    QVariantList rows;
    for (const QVariant& section : view_model.sections()) {
        rows += section.toMap().value(QStringLiteral("rows")).toList();
    }
    return rows;
}

QStringList titles(const VideoLibraryViewModel& view_model) {
    QStringList result;
    for (const QVariant& row : rows_of(view_model)) {
        result.append(row.toMap().value(QStringLiteral("displayTitle")).toString());
    }
    return result;
}

QVariantMap row_titled(const VideoLibraryViewModel& view_model, const QString& item_title) {
    for (const QVariant& row : rows_of(view_model)) {
        const auto map = row.toMap();
        if (map.value(QStringLiteral("title")).toString() == item_title) {
            return map;
        }
    }
    return {};
}

QString id_titled(const VideoLibraryViewModel& view_model, const QString& item_title) {
    return row_titled(view_model, item_title).value(QStringLiteral("id")).toString();
}

QStringList paths_of(const VideoLibraryViewModel& view_model) {
    QStringList result;
    for (const QVariant& row : rows_of(view_model)) {
        result.append(row.toMap().value(QStringLiteral("path")).toString());
    }
    return result;
}

QStringList section_ids(const VideoLibraryViewModel& view_model) {
    QStringList result;
    for (const QVariant& section : view_model.sections()) {
        result.append(section.toMap().value(QStringLiteral("id")).toString());
    }
    return result;
}

QVariantMap rule(const QString& field, const QString& match, const QString& value) {
    return {{QStringLiteral("field"), field},
            {QStringLiteral("match"), match},
            {QStringLiteral("value"), value}};
}

bool write_bytes(const QString& path, const QByteArray& bytes, qint64 modified_seconds) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(bytes);
    file.flush();
    file.setFileTime(QDateTime::fromSecsSinceEpoch(modified_seconds, QTimeZone::UTC),
                     QFileDevice::FileModificationTime);
    return file.error() == QFileDevice::NoError;
}

niratan::video::ThumbnailRequest thumbnail_request_for(const QString& path) {
    const QFileInfo info(path);
    niratan::video::ThumbnailRequest request;
    request.path = QString::fromStdString(info.filesystemAbsoluteFilePath().string());
    request.file_size = info.size();
    request.modified_at = info.lastModified().toUTC();
    return request;
}

QString thumbnail_cache_path(const QString& item_path) {
    const auto request = thumbnail_request_for(item_path);
    const auto directory = niratan::video::default_thumbnail_cache_directory();
    const auto key = niratan::video::thumbnail_cache_key(request).toStdString();
    return QString::fromStdString((directory / (key + ".png")).string());
}

// The view model tests never start ffmpeg; each generation returns PNG bytes
// that its store writes to the cache so a URL can be published.
class StubThumbnailGenerator : public niratan::video::ThumbnailGenerator {
   public:
    QByteArray generate(const QString& path, int maximum_dimension,
                        const niratan::video::ThumbnailCancelled& is_cancelled) override {
        Q_UNUSED(maximum_dimension);
        Q_UNUSED(is_cancelled);
        QMutexLocker locker(&mutex_);
        starts_.append(path);
        if (fail_paths_.contains(path)) {
            return {};
        }
        return QByteArrayLiteral("PNG-DATA");
    }

    void fail_for(const QString& path) {
        QMutexLocker locker(&mutex_);
        fail_paths_.insert(path);
    }

    int call_count() {
        QMutexLocker locker(&mutex_);
        return static_cast<int>(starts_.size());
    }

    QStringList starts() {
        QMutexLocker locker(&mutex_);
        return starts_;
    }

   private:
    QMutex mutex_;
    QStringList starts_;
    QSet<QString> fail_paths_;
};

}  // namespace

class VideoLibraryViewModelTest : public QObject {
    Q_OBJECT
   private slots:
    void defaultsAndStoredLayout() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        const QString settings_path =
            QString::fromStdString((niratan::storage::data_root() / "settings.json").string());
        QFile settings(settings_path);
        QVERIFY(settings.open(QIODevice::WriteOnly));
        settings.write(QByteArrayLiteral("{\"videoSeekInterval\": 15}\n"));
        settings.close();

        VideoLibraryViewModel view_model;
        QCOMPARE(view_model.displayMode(), QStringLiteral("continueWatching"));
        QCOMPARE(view_model.sortOption(), QStringLiteral("title"));
        QCOMPARE(view_model.layoutMode(), QStringLiteral("posters"));
        QCOMPARE(view_model.posterSize(), 160.0);
        QCOMPARE(view_model.posterWidth(), 232.0);
        QVERIFY(view_model.hasSources());
        QVERIFY(!view_model.isScanning());
        QVERIFY(view_model.errorMessage().isEmpty());
        QCOMPARE(view_model.playbackHistoryRevision(), 0);

        view_model.setLayoutMode(QStringLiteral("list"));
        QCOMPARE(view_model.layoutMode(), QStringLiteral("list"));
        view_model.setPosterSize(300);
        QCOMPARE(view_model.posterSize(), 240.0);
        QCOMPARE(view_model.posterWidth(), 348.0);

        const auto stored =
            niratan::storage::read_json_file(std::filesystem::path(settings_path.toStdString()))
                .document.object();
        QCOMPARE(stored.value(QStringLiteral("videoLibraryLayoutMode")).toString(),
                 QStringLiteral("list"));
        QCOMPARE(stored.value(QStringLiteral("videoLibraryPosterWidth")).toDouble(), 240.0);
        QCOMPARE(stored.value(QStringLiteral("videoSeekInterval")).toInt(), 15);

        VideoLibraryViewModel reloaded;
        QCOMPARE(reloaded.layoutMode(), QStringLiteral("list"));
        QCOMPARE(reloaded.posterSize(), 240.0);
        QCOMPARE(reloaded.displayMode(), QStringLiteral("continueWatching"));
        QCOMPARE(reloaded.sortOption(), QStringLiteral("title"));

        QVERIFY(QFile::remove(settings_path));
        QFile invalid(settings_path);
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        invalid.write(QByteArrayLiteral("{\"videoLibraryLayoutMode\": \"grid\"}\n"));
        invalid.close();
        VideoLibraryViewModel fallback;
        QCOMPARE(fallback.layoutMode(), QStringLiteral("posters"));
        QCOMPARE(fallback.posterSize(), 160.0);
    }

    // Opening the library with nothing in progress shows every video instead
    // of an empty Continue Watching list, so a scanned folder never looks lost.
    void loadFallsBackToAllWithoutProgress() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        QVERIFY(QFile::remove(QString::fromStdString(
            (niratan::storage::data_root() / "video_playback_history.json").string())));

        VideoLibraryViewModel view_model;
        view_model.load();
        QCOMPARE(view_model.displayMode(), QStringLiteral("all"));
        QCOMPARE(titles(view_model).size(), 3);

        PlaybackHistory history(niratan::storage::data_root() / "video_playback_history.json");
        history.save(fixture.alpha, 20, 100, QDateTime::fromSecsSinceEpoch(100, QTimeZone::UTC));
        VideoLibraryViewModel resuming;
        resuming.load();
        QCOMPARE(resuming.displayMode(), QStringLiteral("continueWatching"));
    }

    void continueWatchingShowsOnlyResumableVideos() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;

        QCOMPARE(view_model.displayMode(), QStringLiteral("continueWatching"));
        QCOMPARE(titles(view_model),
                 QStringList({QStringLiteral("Beta"), QStringLiteral("Alpha")}));
        const auto sections = view_model.sections();
        QCOMPARE(sections.size(), 1);
        QCOMPARE(sections[0].toMap().value(QStringLiteral("id")).toString(),
                 QStringLiteral("continue"));
        QCOMPARE(sections[0].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Continue Watching"));
        QCOMPARE(sections[0].toMap().value(QStringLiteral("collectionId")).toString(), QString());

        const auto beta = row_titled(view_model, QStringLiteral("Beta"));
        QVERIFY(!beta.isEmpty());
        const auto playback = beta.value(QStringLiteral("playback")).toMap();
        QCOMPARE(playback.value(QStringLiteral("position")).toDouble(), 50.0);
        QCOMPARE(playback.value(QStringLiteral("duration")).toDouble(), 100.0);
        QCOMPARE(playback.value(QStringLiteral("finished")).toBool(), false);
        QCOMPARE(playback.value(QStringLiteral("progress")).toDouble(), 0.5);
        QCOMPARE(playback.value(QStringLiteral("remaining")).toDouble(), 50.0);
        QCOMPARE(playback.value(QStringLiteral("resumable")).toBool(), true);
        QCOMPARE(playback.value(QStringLiteral("updatedAt")).toString(),
                 QDateTime::fromSecsSinceEpoch(200, QTimeZone::UTC).toString(Qt::ISODate));
        QCOMPARE(beta.value(QStringLiteral("sourceName")).toString(),
                 QStringLiteral("Anime Source"));
        QCOMPARE(beta.value(QStringLiteral("fileSize")).toLongLong(), 30LL);
    }

    void searchMatchesTitleFolderSourceAndPath() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;
        view_model.setDisplayMode(QStringLiteral("all"));

        view_model.setSearchText(QStringLiteral("beta"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Beta")}));

        view_model.setSearchText(QStringLiteral("  season 1  "));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Alpha")}));

        view_model.setSearchText(QStringLiteral("anime source"));
        QCOMPARE(titles(view_model),
                 QStringList(
                     {QStringLiteral("Alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma")}));

        view_model.setSearchText(QStringLiteral("gamma.webm"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Gamma")}));

        view_model.setSearchText(QString());
        QCOMPARE(titles(view_model).size(), 3);
    }

    void sortTiesBreakOnPath() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString source = make_directory(dir.filePath("Sort Source"));
        const QString first = touch_file(source + QStringLiteral("/A/Episode.mkv"), 1, 10);
        const QString second = touch_file(source + QStringLiteral("/B/Episode.mkv"), 1, 30);
        use_data_home(dir.path());
        LibraryStore store(niratan::storage::data_root() / "video_library.json");
        const auto library_source = store.add_source(fs_path(source));
        QVERIFY(store.scan_source(library_source.id));

        VideoLibraryViewModel view_model;
        view_model.setDisplayMode(QStringLiteral("all"));
        const QStringList sorted(
            {QFileInfo(first).absoluteFilePath(), QFileInfo(second).absoluteFilePath()});
        const QStringList by_folder(
            {QFileInfo(first).absoluteFilePath(), QFileInfo(second).absoluteFilePath()});
        const QStringList by_date(
            {QFileInfo(second).absoluteFilePath(), QFileInfo(first).absoluteFilePath()});

        view_model.setSortOption(QStringLiteral("title"));
        QCOMPARE(paths_of(view_model), sorted);
        view_model.setSortOption(QStringLiteral("folder"));
        QCOMPARE(paths_of(view_model), by_folder);
        view_model.setSortOption(QStringLiteral("recentPlayback"));
        QCOMPARE(paths_of(view_model), sorted);
        view_model.setSortOption(QStringLiteral("modifiedDate"));
        QCOMPARE(paths_of(view_model), by_date);
    }

    void emptyStateCopyPerMode() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        use_data_home(dir.path());
        VideoLibraryViewModel view_model;

        const auto check = [&view_model](const QString& mode, const QString& title,
                                         const QString& description) {
            view_model.setDisplayMode(mode);
            QCOMPARE(view_model.emptyTitle(), title);
            QCOMPARE(view_model.emptyDescription(), description);
        };
        check(QStringLiteral("continueWatching"), QStringLiteral("No Videos in Progress"),
              QStringLiteral("Partially watched videos will appear here."));
        check(QStringLiteral("favorites"), QStringLiteral("No Favorite Videos"),
              QStringLiteral("Favorite videos will appear here."));
        check(QStringLiteral("unwatched"), QStringLiteral("No Unwatched Videos"),
              QStringLiteral("Videos without playback progress will appear here."));
        check(QStringLiteral("finished"), QStringLiteral("No Finished Videos"),
              QStringLiteral("Videos marked watched will appear here."));
        check(QStringLiteral("missing"), QStringLiteral("No Missing Videos"),
              QStringLiteral("Missing videos will appear here until their source is refreshed."));
        check(QStringLiteral("recent"), QStringLiteral("No Recent Videos"),
              QStringLiteral("Played videos will appear here."));
        check(QStringLiteral("needsReview"), QStringLiteral("No Videos Need Review"),
              QStringLiteral("Videos outside manual and smart collections will appear here."));
        for (const auto* mode : {"all", "series", "folders", "collections"}) {
            check(QLatin1String(mode), QStringLiteral("No Videos"),
                  QStringLiteral("Refresh your folders or add another source."));
        }
    }

    void sortOptionsOrderAllVideos() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;
        view_model.setDisplayMode(QStringLiteral("all"));
        QCOMPARE(section_ids(view_model), QStringList({QStringLiteral("all")}));

        view_model.setSortOption(QStringLiteral("title"));
        QCOMPARE(titles(view_model),
                 QStringList(
                     {QStringLiteral("Alpha"), QStringLiteral("Beta"), QStringLiteral("Gamma")}));
        const auto alpha_organization = row_titled(view_model, QStringLiteral("Alpha"))
                                            .value(QStringLiteral("organization"))
                                            .toMap();
        QCOMPARE(alpha_organization.value(QStringLiteral("seriesName")).toString(),
                 QStringLiteral("Anime Source"));
        QCOMPARE(alpha_organization.value(QStringLiteral("seasonName")).toString(),
                 QStringLiteral("Season 1"));
        QCOMPARE(alpha_organization.value(QStringLiteral("folderPath")).toString(),
                 QStringLiteral("Season 1"));
        const auto beta_organization = row_titled(view_model, QStringLiteral("Beta"))
                                           .value(QStringLiteral("organization"))
                                           .toMap();
        QCOMPARE(beta_organization.value(QStringLiteral("seriesName")).toString(),
                 QStringLiteral("Anime Source"));
        QCOMPARE(beta_organization.value(QStringLiteral("seasonName")).toString(),
                 QStringLiteral("Season 2"));
        const auto gamma_organization = row_titled(view_model, QStringLiteral("Gamma"))
                                            .value(QStringLiteral("organization"))
                                            .toMap();
        QCOMPARE(gamma_organization.value(QStringLiteral("seriesName")).toString(),
                 QStringLiteral("Anime Source"));
        QCOMPARE(gamma_organization.value(QStringLiteral("folderPath")).toString(),
                 QStringLiteral("Anime Source"));
        QVERIFY(!gamma_organization.contains(QStringLiteral("seasonName")));

        view_model.setSortOption(QStringLiteral("modifiedDate"));
        QCOMPARE(titles(view_model),
                 QStringList(
                     {QStringLiteral("Beta"), QStringLiteral("Gamma"), QStringLiteral("Alpha")}));

        view_model.setSortOption(QStringLiteral("folder"));
        QCOMPARE(titles(view_model),
                 QStringList(
                     {QStringLiteral("Gamma"), QStringLiteral("Alpha"), QStringLiteral("Beta")}));

        view_model.setSortOption(QStringLiteral("recentPlayback"));
        QCOMPARE(titles(view_model),
                 QStringList(
                     {QStringLiteral("Beta"), QStringLiteral("Alpha"), QStringLiteral("Gamma")}));
    }

    void unfinishedFilterUsesIncompletePlaybackState() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;
        view_model.setDisplayMode(QStringLiteral("all"));
        view_model.setShowUnfinishedOnly(true);

        QCOMPARE(titles(view_model),
                 QStringList({QStringLiteral("Alpha"), QStringLiteral("Beta")}));
    }

    void smartFiltersSplitUnwatchedFinishedAndMissingVideos() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;

        view_model.setDisplayMode(QStringLiteral("unwatched"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Gamma")}));
        QCOMPARE(section_ids(view_model), QStringList({QStringLiteral("unwatched")}));

        view_model.setDisplayMode(QStringLiteral("all"));
        const QString beta_id = id_titled(view_model, QStringLiteral("Beta"));
        QVERIFY(!beta_id.isEmpty());
        view_model.markWatched(beta_id);

        view_model.setDisplayMode(QStringLiteral("finished"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Beta")}));
        QCOMPARE(section_ids(view_model), QStringList({QStringLiteral("finished")}));

        QVERIFY(QFile::remove(fixture.gamma));
        view_model.setDisplayMode(QStringLiteral("missing"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Gamma")}));
        QCOMPARE(section_ids(view_model), QStringList({QStringLiteral("missing")}));
        QCOMPARE(view_model.emptyTitle(), QStringLiteral("No Missing Videos"));
        QCOMPARE(
            view_model.emptyDescription(),
            QStringLiteral("Missing videos will appear here until their source is refreshed."));
    }

    void modeCountsIgnoreSearch() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;

        auto counts = view_model.modeCounts();
        QCOMPARE(counts.value(QStringLiteral("all")).toInt(), 3);
        QCOMPARE(counts.value(QStringLiteral("continueWatching")).toInt(), 2);
        QCOMPARE(counts.value(QStringLiteral("unwatched")).toInt(), 1);
        QCOMPARE(counts.value(QStringLiteral("finished")).toInt(), 0);
        QCOMPARE(counts.value(QStringLiteral("recent")).toInt(), 2);
        QCOMPARE(counts.value(QStringLiteral("missing")).toInt(), 0);
        QCOMPARE(counts.value(QStringLiteral("favorites")).toInt(), 0);
        QCOMPARE(counts.value(QStringLiteral("series")).toInt(), 1);
        QCOMPARE(counts.value(QStringLiteral("folders")).toInt(), 3);
        QCOMPARE(counts.value(QStringLiteral("collections")).toInt(), 0);
        QVERIFY(!counts.contains(QStringLiteral("needsReview")));

        view_model.setSearchText(QStringLiteral("beta"));
        counts = view_model.modeCounts();
        QCOMPARE(counts.value(QStringLiteral("all")).toInt(), 3);
    }

    void sourceSummariesCountItemsAndRefreshOneSource() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;

        auto summaries = view_model.sourceSummaries();
        QCOMPARE(summaries.size(), 1);
        auto summary = summaries[0].toMap();
        QCOMPARE(summary.value(QStringLiteral("name")).toString(), QStringLiteral("Anime Source"));
        QCOMPARE(summary.value(QStringLiteral("itemCount")).toInt(), 3);
        QCOMPARE(summary.value(QStringLiteral("inProgressCount")).toInt(), 2);
        QCOMPARE(summary.value(QStringLiteral("missingCount")).toInt(), 0);

        QVERIFY(QFile::remove(fixture.gamma));
        summary = view_model.sourceSummaries()[0].toMap();
        QCOMPARE(summary.value(QStringLiteral("missingCount")).toInt(), 1);

        view_model.refreshSource(summary.value(QStringLiteral("id")).toString());
        summary = view_model.sourceSummaries()[0].toMap();
        QCOMPARE(summary.value(QStringLiteral("itemCount")).toInt(), 2);
        QCOMPARE(summary.value(QStringLiteral("missingCount")).toInt(), 0);
    }

    void organizationMetadataSubtitleAndCollections() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_organized_fixture(dir.path());
        VideoLibraryViewModel view_model;

        view_model.setDisplayMode(QStringLiteral("series"));
        QCOMPARE(view_model.sections().size(), 2);
        QCOMPARE(view_model.sections()[0].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Movies"));
        QCOMPARE(view_model.sections()[1].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Show A"));
        QCOMPARE(view_model.sections()[1].toMap().value(QStringLiteral("id")).toString(),
                 QStringLiteral("series-Show A"));
        QStringList seasons;
        for (const QVariant& row :
             view_model.sections()[1].toMap().value(QStringLiteral("rows")).toList()) {
            seasons.append(row.toMap()
                               .value(QStringLiteral("organization"))
                               .toMap()
                               .value(QStringLiteral("seasonName"))
                               .toString());
        }
        QCOMPARE(seasons, QStringList({QStringLiteral("Season 1"), QStringLiteral("Season 2")}));

        view_model.setDisplayMode(QStringLiteral("all"));
        const auto episode1 = row_titled(view_model, QStringLiteral("Episode 01"));
        QVERIFY(!episode1.isEmpty());
        const QString episode1_id = episode1.value(QStringLiteral("id")).toString();
        const auto movie = row_titled(view_model, QStringLiteral("Movie"));
        QVERIFY(!movie.isEmpty());
        QCOMPARE(movie.value(QStringLiteral("organization"))
                     .toMap()
                     .value(QStringLiteral("seriesName"))
                     .toString(),
                 QStringLiteral("Movies"));
        QVERIFY(!movie.value(QStringLiteral("organization"))
                     .toMap()
                     .contains(QStringLiteral("seasonName")));

        view_model.setDisplayTitle(episode1_id, QStringLiteral("Pilot"));
        view_model.setFavorite(episode1_id, true);
        view_model.setTags(
            episode1_id,
            {QStringLiteral("Anime"), QStringLiteral("Listening"), QStringLiteral("Anime")});
        view_model.bindSubtitle(episode1_id, fixture.subtitle);
        const auto collection =
            view_model.createCollection(QStringLiteral("Weekend"), {episode1_id});
        QVERIFY(!collection.isEmpty());
        QCOMPARE(collection.value(QStringLiteral("name")).toString(), QStringLiteral("Weekend"));

        view_model.setDisplayMode(QStringLiteral("favorites"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Pilot")}));
        QCOMPARE(section_ids(view_model), QStringList({QStringLiteral("favorites")}));

        view_model.setDisplayMode(QStringLiteral("all"));
        view_model.setSearchText(QStringLiteral("episode 01.ja"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Pilot")}));
        view_model.setSearchText(QStringLiteral("show a / season 2"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Episode 02")}));
        view_model.setSearchText(QStringLiteral("listening"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Pilot")}));
        view_model.setSearchText(QString());

        const auto pilot = row_titled(view_model, QStringLiteral("Episode 01"));
        QVERIFY(!pilot.isEmpty());
        QCOMPARE(pilot.value(QStringLiteral("displayTitle")).toString(), QStringLiteral("Pilot"));
        const auto metadata = pilot.value(QStringLiteral("metadata")).toMap();
        QCOMPARE(metadata.value(QStringLiteral("tags")).toStringList(),
                 QStringList({QStringLiteral("Anime"), QStringLiteral("Listening")}));
        QCOMPARE(metadata.value(QStringLiteral("isFavorite")).toBool(), true);
        QCOMPARE(metadata.value(QStringLiteral("boundSubtitlePath")).toString(),
                 QFileInfo(fixture.subtitle).absoluteFilePath());
        QCOMPARE(pilot.value(QStringLiteral("subtitleCandidatePath")).toString(),
                 QFileInfo(fixture.subtitle).absoluteFilePath());
        QCOMPARE(view_model.subtitleUrlForOpening(episode1_id).toLocalFile(),
                 QFileInfo(fixture.subtitle).absoluteFilePath());

        view_model.bindSubtitle(episode1_id, QString());
        const auto unbound = row_titled(view_model, QStringLiteral("Episode 01"));
        QVERIFY(unbound.value(QStringLiteral("subtitleCandidatePath")).toString().isEmpty());
        QVERIFY(view_model.subtitleUrlForOpening(episode1_id).isEmpty());
        view_model.bindSubtitle(episode1_id, fixture.subtitle);

        view_model.setDisplayMode(QStringLiteral("collections"));
        QCOMPARE(view_model.sections().size(), 1);
        QCOMPARE(view_model.sections()[0].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Weekend"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Pilot")}));
        const QString collection_id = collection.value(QStringLiteral("id")).toString();
        QCOMPARE(view_model.sections()[0].toMap().value(QStringLiteral("collectionId")).toString(),
                 collection_id);
        QVERIFY(collection_id == collection_id.toUpper());
        QCOMPARE(view_model.sections()[0].toMap().value(QStringLiteral("id")).toString(),
                 QStringLiteral("collection-") + collection_id);

        view_model.setCollectionMembership(episode1_id, collection_id, false);
        QCOMPARE(view_model.sections().size(), 1);
        QCOMPARE(view_model.sections()[0].toMap().value(QStringLiteral("rows")).toList().size(), 0);
        view_model.setCollectionMembership(episode1_id, collection_id, true);
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Pilot")}));

        const QString episode_path = episode1_id;
        view_model.removeCollection(collection_id);
        QVERIFY(view_model.sections().isEmpty());
        view_model.setDisplayMode(QStringLiteral("all"));
        QVERIFY(!row_titled(view_model, QStringLiteral("Episode 01")).isEmpty());
        QVERIFY(row_titled(view_model, QStringLiteral("Episode 01"))
                    .value(QStringLiteral("metadata"))
                    .toMap()
                    .value(QStringLiteral("collectionIDs"))
                    .toStringList()
                    .isEmpty());
        QVERIFY(QFileInfo::exists(episode_path));
    }

    void foldersViewUsesSourceRelativeParentFolders() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_organized_fixture(dir.path());
        VideoLibraryViewModel view_model;

        view_model.setDisplayMode(QStringLiteral("folders"));
        const auto sections = view_model.sections();
        QCOMPARE(sections.size(), 3);
        QCOMPARE(sections[0].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Movies"));
        QCOMPARE(sections[1].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Show A / Season 1"));
        QCOMPARE(sections[2].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Show A / Season 2"));
        QCOMPARE(sections[0].toMap().value(QStringLiteral("id")).toString(),
                 QStringLiteral("folder-Movies"));
        QCOMPARE(sections[1].toMap().value(QStringLiteral("rows")).toList().size(), 1);
        QCOMPARE(sections[1]
                     .toMap()
                     .value(QStringLiteral("rows"))
                     .toList()[0]
                     .toMap()
                     .value(QStringLiteral("title"))
                     .toString(),
                 QStringLiteral("Episode 01"));
    }

    void smartRuleFieldsAndMatches() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_organized_fixture(dir.path());
        VideoLibraryViewModel view_model;
        view_model.setDisplayMode(QStringLiteral("all"));
        const QString episode1_id = id_titled(view_model, QStringLiteral("Episode 01"));
        QVERIFY(!episode1_id.isEmpty());
        view_model.setTags(episode1_id, {QStringLiteral("Listening")});
        view_model.bindSubtitle(episode1_id, fixture.subtitle);

        PlaybackHistory history(niratan::storage::data_root() / "video_playback_history.json");
        QVERIFY(history.save(fixture.episode1, 20, 100));
        QVERIFY(history.mark_watched(fixture.episode2, std::nullopt));
        view_model.refreshPlaybackHistory();

        const auto preview = [&view_model](const QVariantList& rules) {
            QStringList result;
            for (const QVariant& row : view_model.smartCollectionPreviewRows(rules, 8)) {
                result.append(row.toMap().value(QStringLiteral("title")).toString());
            }
            return result;
        };
        const auto one = [](const QString& field, const QString& match, const QString& value) {
            return QVariantList{rule(field, match, value)};
        };

        QCOMPARE(preview(one(QStringLiteral("fileName"), QStringLiteral("contains"),
                             QStringLiteral("episode 0"))),
                 QStringList({QStringLiteral("Episode 01"), QStringLiteral("Episode 02")}));
        QCOMPARE(preview(one(QStringLiteral("parentFolder"), QStringLiteral("equals"),
                             QStringLiteral("Season 1"))),
                 QStringList({QStringLiteral("Episode 01")}));
        QCOMPARE(preview(one(QStringLiteral("path"), QStringLiteral("contains"),
                             QStringLiteral("Show A"))),
                 QStringList({QStringLiteral("Episode 01"), QStringLiteral("Episode 02")}));
        QCOMPARE(preview(one(QStringLiteral("tag"), QStringLiteral("equals"),
                             QStringLiteral("LISTENING"))),
                 QStringList({QStringLiteral("Episode 01")}));
        QCOMPARE(
            preview(one(QStringLiteral("hasBoundSubtitle"), QStringLiteral("isTrue"), QString())),
            QStringList({QStringLiteral("Episode 01")}));
        QCOMPARE(preview(one(QStringLiteral("playbackState"), QStringLiteral("contains"),
                             QStringLiteral("finished"))),
                 QStringList({QStringLiteral("Episode 02")}));
        QCOMPARE(preview(one(QStringLiteral("playbackState"), QStringLiteral("contains"),
                             QStringLiteral("inProgress"))),
                 QStringList({QStringLiteral("Episode 01")}));
        QVERIFY(preview(one(QStringLiteral("fileName"), QStringLiteral("isTrue"), QString()))
                    .isEmpty());
        QVERIFY(
            preview(one(QStringLiteral("path"), QStringLiteral("contains"), QStringLiteral("   ")))
                .isEmpty());
        QVERIFY(preview({rule(QStringLiteral("tag"), QStringLiteral("contains"),
                              QStringLiteral("listen")),
                         rule(QStringLiteral("path"), QStringLiteral("contains"),
                              QStringLiteral("Season 2"))})
                    .isEmpty());
    }

    void smartCollectionsMatchRulesAndPreviewRows() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_organized_fixture(dir.path());
        VideoLibraryViewModel view_model;

        view_model.setDisplayMode(QStringLiteral("all"));
        const QString episode1_id = id_titled(view_model, QStringLiteral("Episode 01"));
        QVERIFY(!episode1_id.isEmpty());
        view_model.setTags(episode1_id, {QStringLiteral("Listening")});
        view_model.bindSubtitle(episode1_id, fixture.subtitle);

        const auto preview = view_model.smartCollectionPreviewRows(
            {rule(QStringLiteral("path"), QStringLiteral("contains"), QStringLiteral("Show A"))},
            1);
        QCOMPARE(preview.size(), 1);
        QCOMPARE(preview[0].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Episode 01"));

        const auto tagged = view_model.createSmartCollection(
            QStringLiteral("Tagged"),
            {rule(QStringLiteral("tag"), QStringLiteral("contains"), QStringLiteral("listen"))});
        QCOMPARE(tagged.value(QStringLiteral("kind")).toString(), QStringLiteral("smart"));
        QVERIFY(tagged.value(QStringLiteral("itemPaths")).toStringList().isEmpty());
        view_model.createSmartCollection(
            QStringLiteral("Subtitled"),
            {rule(QStringLiteral("hasBoundSubtitle"), QStringLiteral("isTrue"), QString())});
        view_model.createSmartCollection(QStringLiteral("Empty"), {});

        view_model.setDisplayMode(QStringLiteral("collections"));
        const auto sections = view_model.sections();
        QCOMPARE(sections.size(), 3);
        QCOMPARE(sections[0].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Empty"));
        QCOMPARE(sections[1].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Subtitled"));
        QCOMPARE(sections[2].toMap().value(QStringLiteral("title")).toString(),
                 QStringLiteral("Tagged"));
        QCOMPARE(sections[0].toMap().value(QStringLiteral("rows")).toList().size(), 0);
        QStringList matched;
        for (int index = 1; index < sections.size(); ++index) {
            for (const QVariant& row :
                 sections[index].toMap().value(QStringLiteral("rows")).toList()) {
                matched.append(row.toMap().value(QStringLiteral("title")).toString());
            }
        }
        QCOMPARE(matched,
                 QStringList({QStringLiteral("Episode 01"), QStringLiteral("Episode 01")}));
    }

    void needsReviewExcludesManualAndSmartCollectionRows() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_organized_fixture(dir.path());
        VideoLibraryViewModel view_model;

        view_model.setDisplayMode(QStringLiteral("all"));
        const QString movie_id = id_titled(view_model, QStringLiteral("Movie"));
        QVERIFY(!movie_id.isEmpty());
        const auto manual = view_model.createCollection(QStringLiteral("Movies"), {movie_id});
        view_model.createSmartCollection(
            QStringLiteral("Show A"),
            {rule(QStringLiteral("path"), QStringLiteral("contains"), QStringLiteral("Show A"))});

        view_model.setDisplayMode(QStringLiteral("needsReview"));
        QVERIFY(titles(view_model).isEmpty());

        view_model.removeCollection(manual.value(QStringLiteral("id")).toString());
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Movie")}));
    }

    void selectionBatchActionsAndMissingCleanup() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_organized_fixture(dir.path());
        VideoLibraryViewModel view_model;

        QCOMPARE(view_model.selectedItemID(), QString());
        view_model.setDisplayMode(QStringLiteral("all"));
        const QString episode1_id = id_titled(view_model, QStringLiteral("Episode 01"));
        QVERIFY(!episode1_id.isEmpty());
        view_model.select(episode1_id);
        QCOMPARE(view_model.selectedItemID(), episode1_id);
        QCOMPARE(view_model.selectedItemIDs(), QStringList({episode1_id}));

        const QString episode2_id = id_titled(view_model, QStringLiteral("Episode 02"));
        QVERIFY(!episode2_id.isEmpty());
        view_model.setSelectedItemIDs({episode1_id, episode2_id});
        QCOMPARE(view_model.selectedItemIDs(), QStringList({episode1_id, episode2_id}));
        QCOMPARE(view_model.selectedItemID(), episode1_id);

        view_model.markSelectedWatched();
        view_model.setDisplayMode(QStringLiteral("finished"));
        QStringList finished = titles(view_model);
        finished.sort();
        QCOMPARE(finished,
                 QStringList({QStringLiteral("Episode 01"), QStringLiteral("Episode 02")}));

        view_model.clearSelectedProgress();
        QVERIFY(titles(view_model).isEmpty());

        QVERIFY(QFile::remove(fixture.movie));
        view_model.setDisplayMode(QStringLiteral("missing"));
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Movie")}));
        const auto movie_id = id_titled(view_model, QStringLiteral("Movie"));
        view_model.select(movie_id);
        QCOMPARE(view_model.removeMissingItems(QString()), 1);
        QVERIFY(titles(view_model).isEmpty());
        QVERIFY(view_model.selectedItemIDs().isEmpty());
        QVERIFY(view_model.selectedItemID().isEmpty());
    }

    void playbackStateActionsUpdateRows() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;

        const QString beta_id = id_titled(view_model, QStringLiteral("Beta"));
        QVERIFY(!beta_id.isEmpty());
        view_model.markWatched(beta_id);
        QCOMPARE(titles(view_model), QStringList({QStringLiteral("Alpha")}));

        view_model.setDisplayMode(QStringLiteral("all"));
        const QString gamma_id = id_titled(view_model, QStringLiteral("Gamma"));
        QVERIFY(!gamma_id.isEmpty());
        view_model.markWatched(gamma_id);

        view_model.setDisplayMode(QStringLiteral("recent"));
        QCOMPARE(section_ids(view_model), QStringList({QStringLiteral("recent")}));
        const auto gamma = row_titled(view_model, QStringLiteral("Gamma"));
        QVERIFY(!gamma.isEmpty());
        QCOMPARE(gamma.value(QStringLiteral("playback"))
                     .toMap()
                     .value(QStringLiteral("finished"))
                     .toBool(),
                 true);
        QCOMPARE(gamma.value(QStringLiteral("playback"))
                     .toMap()
                     .value(QStringLiteral("position"))
                     .toDouble(),
                 0.0);
        QVERIFY(
            !gamma.value(QStringLiteral("playback")).toMap().contains(QStringLiteral("duration")));

        const auto watched = row_titled(view_model, QStringLiteral("Beta"));
        QVERIFY(!watched.isEmpty());
        QCOMPARE(watched.value(QStringLiteral("playback"))
                     .toMap()
                     .value(QStringLiteral("finished"))
                     .toBool(),
                 true);
        QCOMPARE(watched.value(QStringLiteral("playback"))
                     .toMap()
                     .value(QStringLiteral("progress"))
                     .toDouble(),
                 1.0);
        QVERIFY(!watched.value(QStringLiteral("playback"))
                     .toMap()
                     .contains(QStringLiteral("remaining")));

        view_model.clearProgress(beta_id);
        QCOMPARE(titles(view_model),
                 QStringList({QStringLiteral("Gamma"), QStringLiteral("Alpha")}));

        view_model.setDisplayMode(QStringLiteral("continueWatching"));
        const QString alpha_id = id_titled(view_model, QStringLiteral("Alpha"));
        QVERIFY(!alpha_id.isEmpty());
        QCOMPARE(view_model.openFromBeginningUrl(alpha_id), QUrl::fromLocalFile(fixture.alpha));
        QVERIFY(titles(view_model).isEmpty());

        view_model.setDisplayMode(QStringLiteral("all"));
        QCOMPARE(view_model.openUrl(beta_id), QUrl::fromLocalFile(fixture.beta));
        QVERIFY(view_model.errorMessage().isEmpty());
    }

    void filteredEmptyStateUsesSearchCopy() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;
        view_model.setDisplayMode(QStringLiteral("all"));

        QCOMPARE(view_model.emptyTitle(), QStringLiteral("No Videos"));
        QCOMPARE(view_model.emptyDescription(),
                 QStringLiteral("Refresh your folders or add another source."));

        view_model.setSearchText(QStringLiteral("missing title"));
        QCOMPARE(view_model.emptyTitle(), QStringLiteral("No Matching Videos"));
        QCOMPARE(view_model.emptyDescription(),
                 QStringLiteral("Try a different search or filter."));

        view_model.setSearchText(QString());
        view_model.setShowUnfinishedOnly(true);
        view_model.setDisplayMode(QStringLiteral("missing"));
        QCOMPARE(view_model.emptyTitle(), QStringLiteral("No Matching Videos"));
    }

    void playbackHistoryRefreshAdvancesRevision() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;
        QCOMPARE(view_model.playbackHistoryRevision(), 0);

        view_model.refreshPlaybackHistory();
        QCOMPARE(view_model.playbackHistoryRevision(), 0);

        PlaybackHistory history(niratan::storage::data_root() / "video_playback_history.json");
        QVERIFY(history.save(fixture.gamma, 5, 100,
                             QDateTime::fromSecsSinceEpoch(300, QTimeZone::UTC)));
        view_model.refreshPlaybackHistory();
        QCOMPARE(view_model.playbackHistoryRevision(), 1);
        QCOMPARE(titles(view_model),
                 QStringList(
                     {QStringLiteral("Gamma"), QStringLiteral("Beta"), QStringLiteral("Alpha")}));

        view_model.refreshPlaybackHistory();
        QCOMPARE(view_model.playbackHistoryRevision(), 1);
    }

    void clearingSelectionClosesDetailsInspector() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;

        view_model.setDisplayMode(QStringLiteral("all"));
        const QString alpha_id = id_titled(view_model, QStringLiteral("Alpha"));
        QVERIFY(!alpha_id.isEmpty());
        view_model.select(alpha_id);
        QCOMPARE(view_model.selectedItemID(), alpha_id);
        QCOMPARE(view_model.selectedItemIDs(), QStringList({alpha_id}));

        view_model.clearSelection();
        QVERIFY(view_model.selectedItemID().isEmpty());
        QVERIFY(view_model.selectedItemIDs().isEmpty());
    }

    void subtitleUrlOnlyWhenFileExists() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_organized_fixture(dir.path());
        VideoLibraryViewModel view_model;

        view_model.setDisplayMode(QStringLiteral("all"));
        const QString episode1_id = id_titled(view_model, QStringLiteral("Episode 01"));
        QVERIFY(!episode1_id.isEmpty());
        view_model.bindSubtitle(episode1_id, fixture.subtitle);
        QCOMPARE(view_model.subtitleUrlForOpening(episode1_id).toLocalFile(),
                 QFileInfo(fixture.subtitle).absoluteFilePath());

        QVERIFY(QFile::remove(fixture.subtitle));
        QVERIFY(view_model.subtitleUrlForOpening(episode1_id).isEmpty());
        QCOMPARE(row_titled(view_model, QStringLiteral("Episode 01"))
                     .value(QStringLiteral("subtitleCandidatePath"))
                     .toString(),
                 QFileInfo(fixture.subtitle).absoluteFilePath());
    }

    void playbackFromLibraryUsesTheCatalogHistoryKey() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto root = use_data_home(dir.path());
        const QString source_path = make_directory(dir.filePath(QStringLiteral("Library")));
        const QString clip =
            touch_file(source_path + QStringLiteral("/日本語 #1 %20?.mkv"), 10, 10);
        const QString subtitle =
            touch_file(source_path + QStringLiteral("/日本語 #1 %20?.srt"), 10, 10);
        QVERIFY(!clip.isEmpty());
        QVERIFY(!subtitle.isEmpty());
        LibraryStore store(root / "video_library.json");
        const auto source = store.add_source(fs_path(source_path));
        store.scan_source(source.id);
        store.bind_subtitle(clip, subtitle);

        VideoLibraryViewModel library;
        library.load();
        QCOMPARE(library.displayMode(), QStringLiteral("all"));

        const QUrl opened = library.openUrl(clip);
        QVERIFY2(opened.isLocalFile(), "the player cannot save resume state for a non-local URL");
        QCOMPARE(opened.toLocalFile(), clip);
        QCOMPARE(library.subtitleUrlForOpening(clip), QUrl::fromLocalFile(subtitle));

        PlaybackHistory history = PlaybackHistory::at_data_root();
        QVERIFY(history.save(QFileInfo(opened.toLocalFile()).absoluteFilePath(), 50, 100));
        library.refreshPlaybackHistory();
        QCOMPARE(library.displayMode(), QStringLiteral("all"));
        library.setDisplayMode(QStringLiteral("continueWatching"));
        QCOMPARE(id_titled(library, QStringLiteral("日本語 #1 %20?")), clip);

        VideoLibraryViewModel reopened;
        reopened.load();
        QCOMPARE(reopened.displayMode(), QStringLiteral("continueWatching"));
        QCOMPARE(reopened.modeCounts().value(QStringLiteral("continueWatching")).toInt(), 1);
    }

    void openUrlReportsMissingFile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        VideoLibraryViewModel view_model;
        view_model.setDisplayMode(QStringLiteral("all"));

        const QString beta_id = id_titled(view_model, QStringLiteral("Beta"));
        QVERIFY(!beta_id.isEmpty());
        QVERIFY(QFile::remove(fixture.beta));
        QVERIFY(view_model.openUrl(beta_id).isEmpty());
        QCOMPARE(view_model.errorMessage(),
                 QStringLiteral("The saved video file is no longer available."));
        QVERIFY(view_model.openFromBeginningUrl(beta_id).isEmpty());
        QCOMPARE(view_model.playbackHistoryRevision(), 0);
        QVERIFY(!titles(view_model).contains(QStringLiteral("Beta")));
        QCOMPARE(titles(view_model).size(), 2);
    }

    void addFoldersAndRefreshAll() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        const QString second = make_directory(dir.filePath("Second Source"));
        touch_file(second + QStringLiteral("/Drama/One.mp4"), 4, 40);

        VideoLibraryViewModel view_model;
        view_model.setDisplayMode(QStringLiteral("all"));
        view_model.addFolders({second});
        QCOMPARE(view_model.sourceSummaries().size(), 2);
        QCOMPARE(titles(view_model).size(), 4);
        QCOMPARE(view_model.sourceSummaries()[0].toMap().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Anime Source"));

        const QString missing = dir.filePath("Gone");
        view_model.addFolders({missing});
        QCOMPARE(view_model.errorMessage(),
                 QStringLiteral("Video folder is no longer available. %1")
                     .arg(QFileInfo(missing).absoluteFilePath()));
        QVERIFY(!view_model.isScanning());

        view_model.refreshAllSources();
        QCOMPARE(view_model.errorMessage(),
                 QStringLiteral("Video folder is no longer available. %1")
                     .arg(QFileInfo(missing).absoluteFilePath()));
    }

    void emptyLibraryStates() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        use_data_home(dir.path());
        VideoLibraryViewModel empty;
        QVERIFY(!empty.hasSources());
        QVERIFY(empty.sections().isEmpty());
        QCOMPARE(empty.emptyTitle(), QStringLiteral("No Videos in Progress"));
        empty.refreshAllSources();
        QVERIFY(empty.errorMessage().isEmpty());
        const auto counts = empty.modeCounts();
        QCOMPARE(counts.size(), 3);
        QCOMPARE(counts.value(QStringLiteral("series")).toInt(), 0);
        QCOMPARE(counts.value(QStringLiteral("folders")).toInt(), 0);
        QCOMPARE(counts.value(QStringLiteral("collections")).toInt(), 0);
        QVERIFY(!counts.contains(QStringLiteral("continueWatching")));
        QCOMPARE(empty.layoutMode(), QStringLiteral("posters"));
        QCOMPARE(empty.posterSize(), 160.0);
        QCOMPARE(empty.posterWidth(), 232.0);
    }

    void unknownSourceFallsBackToParentFolder() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("Orphan/Movie.mkv");
        touch_file(path, 3, 5);
        const QString json =
            QStringLiteral("{\"sources\": [], \"items\": [{\"path\": \"%1\", "
                           "\"sourceID\": \"%2\", \"title\": \"Movie\", \"parentFolder\": "
                           "\"Orphan\", \"fileSize\": 3, \"lastSeenAt\": 0}], "
                           "\"itemMetadataByPath\": {}, \"collections\": []}")
                .arg(QFileInfo(path).absoluteFilePath(),
                     QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper());
        const auto root = use_data_home(dir.path());
        QFile file(QString::fromStdString((root / "video_library.json").string()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(json.toUtf8());
        file.close();

        VideoLibraryViewModel view_model;
        view_model.setDisplayMode(QStringLiteral("all"));
        const auto row = row_titled(view_model, QStringLiteral("Movie"));
        QVERIFY(!row.isEmpty());
        QCOMPARE(row.value(QStringLiteral("sourceName")).toString(), QStringLiteral("Orphan"));
        QCOMPARE(row.value(QStringLiteral("organization"))
                     .toMap()
                     .value(QStringLiteral("seriesName"))
                     .toString(),
                 QStringLiteral("Orphan"));
        QCOMPARE(row.value(QStringLiteral("organization"))
                     .toMap()
                     .value(QStringLiteral("folderPath"))
                     .toString(),
                 QStringLiteral("Orphan"));
    }

    void collectionsKeepChosenSortOrder() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_organized_fixture(dir.path());
        VideoLibraryViewModel view_model;

        view_model.setDisplayMode(QStringLiteral("all"));
        const QString episode2_id = id_titled(view_model, QStringLiteral("Episode 02"));
        const QString episode1_id = id_titled(view_model, QStringLiteral("Episode 01"));
        QVERIFY(!episode2_id.isEmpty());
        QVERIFY(!episode1_id.isEmpty());
        view_model.createCollection(QStringLiteral("Show"), {episode2_id, episode1_id});

        view_model.setDisplayMode(QStringLiteral("collections"));
        QCOMPARE(titles(view_model),
                 QStringList({QStringLiteral("Episode 01"), QStringLiteral("Episode 02")}));

        view_model.setSortOption(QStringLiteral("modifiedDate"));
        QCOMPARE(titles(view_model),
                 QStringList({QStringLiteral("Episode 02"), QStringLiteral("Episode 01")}));
    }

    void thumbnailMapStartsEmptyAndIgnoresUnknownPaths() {
        niratan::test::TempDataRoot root;
        QVERIFY(root.isValid());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        build_simple_fixture(dir.path());
        auto generator = std::make_shared<StubThumbnailGenerator>();
        VideoLibraryViewModel view_model(generator);

        QVERIFY(view_model.thumbnailUrls().isEmpty());
        QVERIFY(!view_model.thumbnailsSuspended());
        view_model.requestThumbnail(QStringLiteral("/videos/absent.mkv"));
        QTest::qWait(50);
        QVERIFY(view_model.thumbnailUrls().isEmpty());
        QCOMPARE(generator->call_count(), 0);
    }

    void requestThumbnailPublishesTheGeneratedCacheUrl() {
        niratan::test::TempDataRoot root;
        QVERIFY(root.isValid());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        auto generator = std::make_shared<StubThumbnailGenerator>();
        VideoLibraryViewModel view_model(generator);
        QSignalSpy changed(&view_model, &VideoLibraryViewModel::thumbnailUrlsChanged);

        view_model.requestThumbnail(fixture.alpha);
        QTRY_VERIFY_WITH_TIMEOUT(view_model.thumbnailUrls().contains(fixture.alpha), 5000);
        const QString url = view_model.thumbnailUrls().value(fixture.alpha).toString();
        QCOMPARE(url, QUrl::fromLocalFile(thumbnail_cache_path(fixture.alpha)).toString());
        QVERIFY(QFileInfo::exists(thumbnail_cache_path(fixture.alpha)));
        QCOMPARE(generator->call_count(), 1);
        QCOMPARE(generator->starts(), QStringList({fixture.alpha}));
        QVERIFY(changed.count() >= 1);
    }

    void repeatedThumbnailRequestsAreNoOps() {
        niratan::test::TempDataRoot root;
        QVERIFY(root.isValid());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        auto generator = std::make_shared<StubThumbnailGenerator>();
        VideoLibraryViewModel view_model(generator);

        view_model.requestThumbnail(fixture.alpha);
        view_model.requestThumbnail(fixture.alpha);
        QTRY_VERIFY_WITH_TIMEOUT(view_model.thumbnailUrls().contains(fixture.alpha), 5000);
        const QString url = view_model.thumbnailUrls().value(fixture.alpha).toString();
        view_model.requestThumbnail(fixture.alpha);
        QTest::qWait(100);
        QCOMPARE(generator->call_count(), 1);
        QCOMPARE(view_model.thumbnailUrls().value(fixture.alpha).toString(), url);
    }

    void cachedThumbnailIsPublishedWhileSuspended() {
        niratan::test::TempDataRoot root;
        QVERIFY(root.isValid());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        QVERIFY(write_bytes(thumbnail_cache_path(fixture.alpha), "cached", 30));
        auto generator = std::make_shared<StubThumbnailGenerator>();
        VideoLibraryViewModel view_model(generator);

        view_model.setThumbnailsSuspended(true);
        QVERIFY(view_model.thumbnailsSuspended());
        view_model.requestThumbnail(fixture.alpha);
        QTRY_VERIFY_WITH_TIMEOUT(view_model.thumbnailUrls().contains(fixture.alpha), 5000);
        QCOMPARE(view_model.thumbnailUrls().value(fixture.alpha).toString(),
                 QUrl::fromLocalFile(thumbnail_cache_path(fixture.alpha)).toString());
        QCOMPARE(generator->call_count(), 0);
        view_model.setThumbnailsSuspended(false);
    }

    void suspendedRequestIsRetriedAfterResume() {
        niratan::test::TempDataRoot root;
        QVERIFY(root.isValid());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        auto generator = std::make_shared<StubThumbnailGenerator>();
        VideoLibraryViewModel view_model(generator);

        view_model.setThumbnailsSuspended(true);
        view_model.requestThumbnail(fixture.alpha);
        view_model.requestThumbnail(fixture.alpha);
        view_model.requestThumbnail(fixture.beta);
        QTest::qWait(100);
        QVERIFY(view_model.thumbnailUrls().isEmpty());
        QCOMPARE(generator->call_count(), 0);

        view_model.setThumbnailsSuspended(false);
        QTRY_VERIFY_WITH_TIMEOUT(view_model.thumbnailUrls().contains(fixture.alpha), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(view_model.thumbnailUrls().contains(fixture.beta), 5000);
        QCOMPARE(generator->starts().size(), 2);
        QVERIFY(generator->starts().contains(fixture.alpha));
        QVERIFY(generator->starts().contains(fixture.beta));
    }

    void suspendingKeepsPublishedThumbnails() {
        niratan::test::TempDataRoot root;
        QVERIFY(root.isValid());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        auto generator = std::make_shared<StubThumbnailGenerator>();
        VideoLibraryViewModel view_model(generator);

        view_model.requestThumbnail(fixture.alpha);
        QTRY_VERIFY_WITH_TIMEOUT(view_model.thumbnailUrls().contains(fixture.alpha), 5000);
        const QString url = view_model.thumbnailUrls().value(fixture.alpha).toString();
        view_model.setThumbnailsSuspended(true);
        QCOMPARE(view_model.thumbnailUrls().value(fixture.alpha).toString(), url);
        view_model.setThumbnailsSuspended(false);
        QCOMPARE(view_model.thumbnailUrls().value(fixture.alpha).toString(), url);
        QCOMPARE(generator->call_count(), 1);
    }

    void failedGenerationKeepsThePlaceholder() {
        niratan::test::TempDataRoot root;
        QVERIFY(root.isValid());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        auto generator = std::make_shared<StubThumbnailGenerator>();
        generator->fail_for(fixture.alpha);
        VideoLibraryViewModel view_model(generator);

        view_model.requestThumbnail(fixture.alpha);
        QTRY_COMPARE_WITH_TIMEOUT(generator->call_count(), 1, 5000);
        QTest::qWait(100);
        QVERIFY(!view_model.thumbnailUrls().contains(fixture.alpha));
        QVERIFY(!QFileInfo::exists(thumbnail_cache_path(fixture.alpha)));

        view_model.requestThumbnail(fixture.beta);
        QTRY_VERIFY_WITH_TIMEOUT(view_model.thumbnailUrls().contains(fixture.beta), 5000);
        QVERIFY(!view_model.thumbnailUrls().contains(fixture.alpha));
    }

    void changedItemUsesANewCacheKey() {
        niratan::test::TempDataRoot root;
        QVERIFY(root.isValid());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto fixture = build_simple_fixture(dir.path());
        auto generator = std::make_shared<StubThumbnailGenerator>();
        VideoLibraryViewModel view_model(generator);

        view_model.requestThumbnail(fixture.alpha);
        QTRY_VERIFY_WITH_TIMEOUT(view_model.thumbnailUrls().contains(fixture.alpha), 5000);
        const QString first_url = view_model.thumbnailUrls().value(fixture.alpha).toString();
        const QString first_cache = thumbnail_cache_path(fixture.alpha);

        QVERIFY(write_bytes(fixture.alpha, QByteArray(50, '2'), 2000));
        view_model.requestThumbnail(fixture.alpha);
        QTest::qWait(100);
        QCOMPARE(generator->call_count(), 1);
        QCOMPARE(view_model.thumbnailUrls().value(fixture.alpha).toString(), first_url);

        view_model.refreshAllSources();
        const QString second_cache = thumbnail_cache_path(fixture.alpha);
        QVERIFY(first_cache != second_cache);
        view_model.requestThumbnail(fixture.alpha);
        QTRY_COMPARE_WITH_TIMEOUT(view_model.thumbnailUrls().value(fixture.alpha).toString(),
                                  QUrl::fromLocalFile(second_cache).toString(), 5000);
        QCOMPARE(generator->call_count(), 2);
    }
};

QTEST_GUILESS_MAIN(VideoLibraryViewModelTest)
#include "tst_video_library_view_model.moc"
