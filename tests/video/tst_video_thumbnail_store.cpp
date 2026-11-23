// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the video thumbnail cache, scheduler and ffmpeg generator,
// following Features/Video/VideoThumbnailStore.swift (`cacheKey`, `cacheURL`,
// `cachedThumbnailURL`, `generateThumbnailURL`, `invalidateThumbnail`,
// `maximumDimension`, `VideoThumbnailScheduler` and its suspend/resume and
// cancellation paths), Features/Video/Playback/HSMpvClient.mm
// (`HSMpvRenderThumbnailPNGData`), script/test_video_thumbnail_store.swift,
// and Decisions D1, D2, D4, D6 and D8 of
// notes/specs/services-video-thumbnail-store.md.
//
// Every generator is injected: no test starts a real ffmpeg, and the ffmpeg
// case runs a script that records its arguments and writes stdout.

#include "services/storage/app_paths.h"
#include "services/video/thumbnail_scheduler.h"
#include "services/video/thumbnail_store.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QWaitCondition>

#include "../viewmodels/data_root.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>

using namespace niratan::video;

namespace {

QString qs(const std::filesystem::path& path) {
    return QString::fromStdString(path.string());
}

std::filesystem::path fs(const QString& path) {
    return std::filesystem::path(path.toStdString());
}

bool write_file(const QString& path, const QByteArray& bytes) {
    if (path.isEmpty()) {
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(bytes) == bytes.size();
}

QByteArray read_file(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

ThumbnailRequest request_for(const QString& path, qint64 file_size = 1024,
                             std::optional<qint64> modified_msecs = std::nullopt) {
    ThumbnailRequest request;
    request.path = path;
    request.file_size = file_size;
    if (modified_msecs) {
        request.modified_at = QDateTime::fromMSecsSinceEpoch(*modified_msecs);
    }
    return request;
}

std::filesystem::path cache_file(const std::filesystem::path& directory,
                                 const ThumbnailRequest& request) {
    return directory / (thumbnail_cache_key(request).toStdString() + ".png");
}

// A generator that can block inside generate() so a test can inspect the
// queue, and that honours the cancellation flag.
class FakeGenerator : public ThumbnailGenerator {
   public:
    QByteArray generate(const QString& path, int maximum_dimension,
                        const ThumbnailCancelled& is_cancelled) override {
        {
            QMutexLocker locker(&mutex_);
            ++active_;
            max_active_ = std::max(max_active_, active_);
            starts_.append(path);
            maximum_dimensions_.append(maximum_dimension);
            worker_thread_ = QThread::currentThread();
            condition_.wakeAll();
        }
        bool cancelled = false;
        {
            QMutexLocker locker(&mutex_);
            while (blocking_ && !released_) {
                if (is_cancelled && is_cancelled()) {
                    cancelled = true;
                    break;
                }
                condition_.wait(locker.mutex(), 10);
            }
            if (is_cancelled && is_cancelled()) {
                cancelled = true;
            }
            --active_;
            condition_.wakeAll();
        }
        if (cancelled) {
            return {};
        }
        {
            QMutexLocker locker(&mutex_);
            if (fail_paths_.contains(path)) {
                return {};
            }
            return payloads_.value(path, QByteArrayLiteral("PNG-DATA"));
        }
    }

    void set_blocking(bool blocking) {
        QMutexLocker locker(&mutex_);
        blocking_ = blocking;
    }

    void release() {
        QMutexLocker locker(&mutex_);
        released_ = true;
        condition_.wakeAll();
    }

    void fail_for(const QString& path) {
        QMutexLocker locker(&mutex_);
        fail_paths_.insert(path);
    }

    bool wait_for_starts(int count, int timeout_ms = 5000) {
        QMutexLocker locker(&mutex_);
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (starts_.size() < count) {
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            condition_.wait(locker.mutex(), 10);
        }
        return true;
    }

    int start_count() {
        QMutexLocker locker(&mutex_);
        return static_cast<int>(starts_.size());
    }

    QStringList starts() {
        QMutexLocker locker(&mutex_);
        return starts_;
    }

    QList<int> maximum_dimensions() {
        QMutexLocker locker(&mutex_);
        return maximum_dimensions_;
    }

    int max_active() {
        QMutexLocker locker(&mutex_);
        return max_active_;
    }

    QThread* worker_thread() {
        QMutexLocker locker(&mutex_);
        return worker_thread_;
    }

   private:
    QMutex mutex_;
    QWaitCondition condition_;
    bool blocking_ = false;
    bool released_ = false;
    int active_ = 0;
    int max_active_ = 0;
    QStringList starts_;
    QList<int> maximum_dimensions_;
    QThread* worker_thread_ = nullptr;
    QSet<QString> fail_paths_;
    QHash<QString, QByteArray> payloads_;
};

// Waiters are collected from callbacks on the scheduler's thread.
struct Collector {
    QVector<QString> tags;
    QVector<std::optional<std::filesystem::path>> values;

    ThumbnailScheduler::Completion sink(const QString& tag) {
        return [this, tag](std::optional<std::filesystem::path> value) {
            tags.append(tag);
            values.append(std::move(value));
        };
    }

    int count(const QString& tag) const {
        int total = 0;
        for (const auto& value : tags) {
            if (value == tag) {
                ++total;
            }
        }
        return total;
    }

    std::optional<std::filesystem::path> value_for(const QString& tag) const {
        for (qsizetype index = 0; index < tags.size(); ++index) {
            if (tags.at(index) == tag) {
                return values.at(index);
            }
        }
        return std::nullopt;
    }

    bool has(const QString& tag) const { return count(tag) > 0; }

    bool is_empty(const QString& tag) const {
        for (qsizetype index = 0; index < tags.size(); ++index) {
            if (tags.at(index) == tag) {
                return !values.at(index).has_value();
            }
        }
        return false;
    }
};

QString fake_ffmpeg(const QTemporaryDir& dir, const QByteArray& body) {
    const QString path = dir.filePath(QStringLiteral("ffmpeg"));
    write_file(path, "#!/bin/sh\n" + body);
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

QStringList recorded_lines(const QString& path) {
    const QByteArray bytes = read_file(path);
    return QString::fromUtf8(bytes).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

}  // namespace

class VideoThumbnailStoreTest : public QObject {
    Q_OBJECT

   private slots:
    void cacheKeyUsesPathSizeAndModified() {
        QCOMPARE(thumbnail_cache_key(request_for(QStringLiteral("/videos/clip.mkv"))),
                 QStringLiteral("bb4a00d9225b5d2e"));
        QCOMPARE(thumbnail_cache_key(
                     request_for(QStringLiteral("/videos/clip.mkv"), 1024, 1700000000000)),
                 QStringLiteral("cfb4fe6455d7cf42"));
        QCOMPARE(thumbnail_cache_key(
                     request_for(QStringLiteral("/videos/clip.mkv"), 1025, 1700000000000)),
                 QStringLiteral("a4447ce68e06ce5b"));
        QCOMPARE(thumbnail_cache_key(
                     request_for(QStringLiteral("/videos/日本語.mkv"), 2048, 1700000000500)),
                 QStringLiteral("db5925faa8f6c7d6"));
        QCOMPARE(thumbnail_cache_key(
                     request_for(QStringLiteral("/videos/clip.mkv"), 1024, 1699999999500)),
                 QStringLiteral("33d8a3947164ba6a"));
        QCOMPARE(thumbnail_cache_key(request_for(QStringLiteral("/videos/clip.mkv"), 1024, -500)),
                 QStringLiteral("cab308e72f08b93c"));
        QCOMPARE(thumbnail_cache_key(
                     request_for(QStringLiteral("/videos/clip.mkv"), 1024, -1700000000000)),
                 QStringLiteral("e002793b6d0e8799"));
    }

    void defaultDirectoryAndCacheUrl() {
        niratan::test::TempDataRoot root;
        QVERIFY(root.isValid());
        const auto expected =
            niratan::storage::cache_root() / std::filesystem::path("VideoThumbnails");
        QCOMPARE(qs(default_thumbnail_cache_directory()), qs(expected));
        QCOMPARE(qs(default_thumbnail_cache_directory().filename()),
                 QStringLiteral("VideoThumbnails"));

        ThumbnailStore store(expected);
        const auto request = request_for(QStringLiteral("/videos/clip.mkv"));
        QCOMPARE(qs(store.cache_url(request)), qs(cache_file(expected, request)));

        const bool home_set = qEnvironmentVariableIsSet("HOME");
        const bool cache_set = qEnvironmentVariableIsSet("XDG_CACHE_HOME");
        const QByteArray home = qgetenv("HOME");
        const QByteArray cache = qgetenv("XDG_CACHE_HOME");
        qunsetenv("HOME");
        qunsetenv("XDG_CACHE_HOME");
        QVERIFY(default_thumbnail_cache_directory().empty());
        if (home_set) {
            qputenv("HOME", home);
        }
        if (cache_set) {
            qputenv("XDG_CACHE_HOME", cache);
        }
    }

    void maximumDimensionIsThreeHundredEightyFour() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ThumbnailStore store(fs(dir.path()));
        QCOMPARE(store.maximum_dimension(), 384);
        QCOMPARE(kThumbnailMaximumDimension, 384);
    }

    void cachedUrlOnlyReportsExistingFiles() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto cache_directory = fs(dir.path()) / "cache";
        ThumbnailStore store(cache_directory);
        const auto request = request_for(QStringLiteral("/videos/clip.mkv"));
        QVERIFY(!store.cached_url(request).has_value());
        QVERIFY(!std::filesystem::exists(cache_directory));

        QVERIFY(write_file(qs(store.cache_url(request)), "cached"));
        const auto cached = store.cached_url(request);
        QVERIFY(cached.has_value());
        QCOMPARE(qs(*cached), qs(store.cache_url(request)));
    }

    void generateUrlHandlesCacheHitMissingSourceAndFailure() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto generator = std::make_shared<FakeGenerator>();
        const auto cache_directory = fs(dir.path()) / "cache";
        ThumbnailStore store(cache_directory, generator);

        const QString source = dir.filePath(QStringLiteral("clip.mkv"));
        QVERIFY(write_file(source, "video"));
        const auto request = request_for(source);
        const auto generated = store.generate_url(request);
        QVERIFY(generated.has_value());
        QCOMPARE(qs(*generated), qs(store.cache_url(request)));
        QCOMPARE(read_file(qs(*generated)), QByteArrayLiteral("PNG-DATA"));
        QCOMPARE(generator->start_count(), 1);
        QCOMPARE(generator->starts(), QStringList({source}));
        QCOMPARE(QDir(qs(cache_directory)).entryList(QDir::Files | QDir::NoDotAndDotDot),
                 QStringList({qs(store.cache_url(request).filename())}));
        QCOMPARE(generator->maximum_dimensions().value(0), 384);

        QVERIFY(write_file(qs(store.cache_url(request)), "old"));
        const auto hit = store.generate_url(request);
        QVERIFY(hit.has_value());
        QCOMPARE(read_file(qs(*hit)), QByteArrayLiteral("old"));
        QCOMPARE(generator->start_count(), 1);

        const auto missing = store.generate_url(request_for(dir.filePath("absent.mkv")));
        QVERIFY(!missing.has_value());
        QCOMPARE(generator->start_count(), 1);

        const QString failed = dir.filePath(QStringLiteral("failed.mkv"));
        QVERIFY(write_file(failed, "video"));
        generator->fail_for(failed);
        const auto failed_request = request_for(failed);
        QVERIFY(!store.generate_url(failed_request).has_value());
        QVERIFY(!std::filesystem::exists(store.cache_url(failed_request)));
        QCOMPARE(QDir(qs(cache_directory)).entryList(QDir::Files | QDir::NoDotAndDotDot),
                 QStringList({qs(store.cache_url(request).filename())}));
        QCOMPARE(generator->start_count(), 2);

        const QString blocked = dir.filePath(QStringLiteral("blocked"));
        QVERIFY(write_file(blocked, "not a directory"));
        ThumbnailStore failing_store(fs(blocked), generator);
        const QString unwritable = dir.filePath(QStringLiteral("unwritable.mkv"));
        QVERIFY(write_file(unwritable, "video"));
        QVERIFY(!failing_store.generate_url(request_for(unwritable)).has_value());
        QCOMPARE(generator->start_count(), 3);
    }

    void invalidateRemovesTheCacheFile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ThumbnailStore store(fs(dir.path()));
        const auto request = request_for(QStringLiteral("/videos/clip.mkv"));
        QVERIFY(write_file(qs(store.cache_url(request)), "png"));
        store.invalidate(request);
        QVERIFY(!std::filesystem::exists(store.cache_url(request)));
        store.invalidate(request);
    }

    void schedulerServesCacheWhileSuspendedAndInCacheOnlyMode() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto generator = std::make_shared<FakeGenerator>();
        auto store = std::make_shared<ThumbnailStore>(fs(dir.path()), generator);
        ThumbnailScheduler scheduler(store);
        const auto request = request_for(QStringLiteral("/videos/clip.mkv"));
        QVERIFY(write_file(qs(store->cache_url(request)), "png"));

        scheduler.suspend(ThumbnailSuspendReason::playback);
        Collector collector;
        scheduler.request(request, ThumbnailRequestMode::cache_only, collector.sink("cache"));
        scheduler.request(request, ThumbnailRequestMode::generate_if_missing,
                          collector.sink("generate"));
        QTRY_COMPARE(collector.count("cache"), 1);
        QCOMPARE(collector.count("generate"), 1);
        QCOMPARE(qs(*collector.value_for("cache")), qs(store->cache_url(request)));
        QCOMPARE(qs(*collector.value_for("generate")), qs(store->cache_url(request)));
        QCOMPARE(generator->start_count(), 0);
        scheduler.resume(ThumbnailSuspendReason::playback);
    }

    void cacheOnlyNeverEnqueues() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto generator = std::make_shared<FakeGenerator>();
        auto store = std::make_shared<ThumbnailStore>(fs(dir.path()), generator);
        generator->set_blocking(true);
        ThumbnailScheduler scheduler(store);
        const QString first = dir.filePath(QStringLiteral("first.mkv"));
        const QString second = dir.filePath(QStringLiteral("second.mkv"));
        QVERIFY(write_file(first, "video"));
        QVERIFY(write_file(second, "video"));

        Collector collector;
        scheduler.request(request_for(first), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("first"));
        QVERIFY(generator->wait_for_starts(1));
        const quint64 same_key = scheduler.request(
            request_for(first), ThumbnailRequestMode::cache_only, collector.sink("same"));
        QTRY_COMPARE(collector.count("same"), 1);
        QVERIFY(collector.is_empty("same"));
        QCOMPARE(same_key, quint64(0));
        QCOMPARE(generator->start_count(), 1);
        scheduler.request(request_for(second), ThumbnailRequestMode::cache_only,
                          collector.sink("second"));
        QTRY_COMPARE(collector.count("second"), 1);
        QVERIFY(collector.is_empty("second"));
        QCOMPARE(generator->start_count(), 1);

        generator->release();
        QTRY_COMPARE(collector.count("first"), 1);
        QVERIFY(collector.value_for("first").has_value());
        QCOMPARE(generator->start_count(), 1);
    }

    void runsOneJobAtATimeAndSharesDuplicateRequests() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto generator = std::make_shared<FakeGenerator>();
        auto store = std::make_shared<ThumbnailStore>(fs(dir.path()), generator);
        generator->set_blocking(true);
        ThumbnailScheduler scheduler(store);
        const QString first = dir.filePath(QStringLiteral("first.mkv"));
        const QString second = dir.filePath(QStringLiteral("second.mkv"));
        QVERIFY(write_file(first, "video"));
        QVERIFY(write_file(second, "video"));

        Collector collector;
        scheduler.request(request_for(first), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("first"));
        QVERIFY(generator->wait_for_starts(1));
        scheduler.request(request_for(second), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("second"));
        scheduler.request(request_for(first), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("duplicate"));
        QTest::qWait(50);
        QCOMPARE(generator->start_count(), 1);
        QCOMPARE(generator->max_active(), 1);

        generator->release();
        QTRY_COMPARE(collector.count("first"), 1);
        QTRY_COMPARE(collector.count("duplicate"), 1);
        QTRY_COMPARE(collector.count("second"), 1);
        QCOMPARE(qs(*collector.value_for("first")), qs(store->cache_url(request_for(first))));
        QVERIFY(collector.value_for("duplicate").has_value());
        QCOMPARE(qs(*collector.value_for("first")), qs(*collector.value_for("duplicate")));
        QCOMPARE(generator->start_count(), 2);
        QCOMPARE(generator->starts(), QStringList({first, second}));
        QCOMPARE(generator->max_active(), 1);
        QVERIFY(generator->worker_thread() != QThread::currentThread());
    }

    void suspensionCountsFollowTheReason() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto store = std::make_shared<ThumbnailStore>(fs(dir.path()));
        ThumbnailScheduler scheduler(store);
        QVERIFY(!scheduler.is_suspended());

        scheduler.suspend(ThumbnailSuspendReason::playback);
        scheduler.suspend(ThumbnailSuspendReason::playback);
        QVERIFY(scheduler.is_suspended());
        scheduler.resume(ThumbnailSuspendReason::playback);
        QVERIFY(!scheduler.is_suspended());
        scheduler.resume(ThumbnailSuspendReason::playback);
        QVERIFY(!scheduler.is_suspended());

        scheduler.suspend(ThumbnailSuspendReason::lookup);
        scheduler.suspend(ThumbnailSuspendReason::lookup);
        QVERIFY(scheduler.is_suspended());
        scheduler.resume(ThumbnailSuspendReason::lookup);
        QVERIFY(scheduler.is_suspended());
        scheduler.resume(ThumbnailSuspendReason::lookup);
        QVERIFY(!scheduler.is_suspended());

        scheduler.suspend(ThumbnailSuspendReason::mining);
        scheduler.resume(ThumbnailSuspendReason::mining);
        QVERIFY(!scheduler.is_suspended());
        scheduler.resume(ThumbnailSuspendReason::mining);
    }

    void suspendCancelsRunningAndPendingWork() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto generator = std::make_shared<FakeGenerator>();
        auto store = std::make_shared<ThumbnailStore>(fs(dir.path()), generator);
        generator->set_blocking(true);
        ThumbnailScheduler scheduler(store);
        const QString first = dir.filePath(QStringLiteral("first.mkv"));
        const QString second = dir.filePath(QStringLiteral("second.mkv"));
        const QString third = dir.filePath(QStringLiteral("third.mkv"));
        QVERIFY(write_file(first, "video"));
        QVERIFY(write_file(second, "video"));
        QVERIFY(write_file(third, "video"));

        Collector collector;
        scheduler.request(request_for(first), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("first"));
        QVERIFY(generator->wait_for_starts(1));
        scheduler.request(request_for(second), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("second"));
        scheduler.suspend(ThumbnailSuspendReason::playback);
        QTRY_COMPARE(collector.count("first"), 1);
        QTRY_COMPARE(collector.count("second"), 1);
        QVERIFY(collector.is_empty("first"));
        QVERIFY(collector.is_empty("second"));
        QVERIFY(!std::filesystem::exists(store->cache_url(request_for(first))));
        QVERIFY(!std::filesystem::exists(store->cache_url(request_for(second))));

        scheduler.request(request_for(third), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("refused"));
        QTRY_COMPARE(collector.count("refused"), 1);
        QVERIFY(collector.is_empty("refused"));
        QCOMPARE(generator->start_count(), 1);

        scheduler.resume(ThumbnailSuspendReason::playback);
        generator->release();
        scheduler.request(request_for(third), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("resumed"));
        QTRY_COMPARE(collector.count("resumed"), 1);
        QCOMPARE(generator->start_count(), 2);
        QVERIFY(collector.value_for("resumed").has_value());
    }

    void cancelPendingKeepsTheRunningJob() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto generator = std::make_shared<FakeGenerator>();
        auto store = std::make_shared<ThumbnailStore>(fs(dir.path()), generator);
        generator->set_blocking(true);
        ThumbnailScheduler scheduler(store);
        const QString first = dir.filePath(QStringLiteral("first.mkv"));
        const QString second = dir.filePath(QStringLiteral("second.mkv"));
        const QString third = dir.filePath(QStringLiteral("third.mkv"));
        QVERIFY(write_file(first, "video"));
        QVERIFY(write_file(second, "video"));
        QVERIFY(write_file(third, "video"));

        Collector collector;
        scheduler.request(request_for(first), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("first"));
        QVERIFY(generator->wait_for_starts(1));
        scheduler.request(request_for(second), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("second"));
        scheduler.request(request_for(third), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("third"));
        scheduler.cancel_pending();
        QTRY_COMPARE(collector.count("second"), 1);
        QTRY_COMPARE(collector.count("third"), 1);
        QVERIFY(collector.is_empty("second"));
        QVERIFY(collector.is_empty("third"));
        QCOMPARE(collector.count("first"), 0);

        generator->release();
        QTRY_COMPARE(collector.count("first"), 1);
        QVERIFY(collector.value_for("first").has_value());
        QCOMPARE(generator->start_count(), 1);
        QVERIFY(!std::filesystem::exists(store->cache_url(request_for(second))));
    }

    void cancellingOneWaiterKeepsTheSharedJob() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto generator = std::make_shared<FakeGenerator>();
        auto store = std::make_shared<ThumbnailStore>(fs(dir.path()), generator);
        generator->set_blocking(true);
        ThumbnailScheduler scheduler(store);
        const QString first = dir.filePath(QStringLiteral("first.mkv"));
        const QString second = dir.filePath(QStringLiteral("second.mkv"));
        QVERIFY(write_file(first, "video"));
        QVERIFY(write_file(second, "video"));

        Collector collector;
        scheduler.request(request_for(first), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("first"));
        QVERIFY(generator->wait_for_starts(1));
        const quint64 kept = scheduler.request(
            request_for(second), ThumbnailRequestMode::generate_if_missing, collector.sink("kept"));
        const quint64 cancelled =
            scheduler.request(request_for(second), ThumbnailRequestMode::generate_if_missing,
                              collector.sink("cancelled"));
        QVERIFY(kept != 0);
        QVERIFY(cancelled != 0);
        QVERIFY(kept != cancelled);
        scheduler.cancel_request(cancelled);
        QTRY_COMPARE(collector.count("cancelled"), 1);
        QVERIFY(collector.is_empty("cancelled"));

        generator->release();
        QTRY_COMPARE(collector.count("first"), 1);
        QTRY_COMPARE(collector.count("kept"), 1);
        QVERIFY(collector.value_for("kept").has_value());
        QCOMPARE(qs(*collector.value_for("kept")), qs(store->cache_url(request_for(second))));
        QCOMPARE(generator->start_count(), 2);
    }

    void cancellingTheLastWaiterDropsTheJob() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto generator = std::make_shared<FakeGenerator>();
        auto store = std::make_shared<ThumbnailStore>(fs(dir.path()), generator);
        generator->set_blocking(true);
        ThumbnailScheduler scheduler(store);
        const QString first = dir.filePath(QStringLiteral("first.mkv"));
        const QString second = dir.filePath(QStringLiteral("second.mkv"));
        QVERIFY(write_file(first, "video"));
        QVERIFY(write_file(second, "video"));

        Collector collector;
        scheduler.request(request_for(first), ThumbnailRequestMode::generate_if_missing,
                          collector.sink("first"));
        QVERIFY(generator->wait_for_starts(1));
        const quint64 id =
            scheduler.request(request_for(second), ThumbnailRequestMode::generate_if_missing,
                              collector.sink("second"));
        QVERIFY(id != 0);
        scheduler.cancel_request(id);
        QTRY_COMPARE(collector.count("second"), 1);
        QVERIFY(collector.is_empty("second"));

        generator->release();
        QTRY_COMPARE(collector.count("first"), 1);
        QCOMPARE(generator->start_count(), 1);
        QVERIFY(!std::filesystem::exists(store->cache_url(request_for(second))));
    }

    void cancellingTheRunningWaiterLeavesTheGeneration() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto generator = std::make_shared<FakeGenerator>();
        auto store = std::make_shared<ThumbnailStore>(fs(dir.path()), generator);
        generator->set_blocking(true);
        ThumbnailScheduler scheduler(store);
        const QString first = dir.filePath(QStringLiteral("first.mkv"));
        QVERIFY(write_file(first, "video"));

        Collector collector;
        const quint64 running = scheduler.request(
            request_for(first), ThumbnailRequestMode::generate_if_missing, collector.sink("first"));
        QVERIFY(running != 0);
        QVERIFY(generator->wait_for_starts(1));
        scheduler.cancel_request(running);
        QTest::qWait(50);
        QCOMPARE(collector.count("first"), 0);

        generator->release();
        QTRY_COMPARE(collector.count("first"), 1);
        QVERIFY(collector.value_for("first").has_value());
        QCOMPARE(generator->start_count(), 1);
    }

    void ffmpegArgumentsMatchTheSwiftOptions() {
        const QStringList expected{
            QStringLiteral("-nostdin"),
            QStringLiteral("-y"),
            QStringLiteral("-ss"),
            QStringLiteral("5"),
            QStringLiteral("-i"),
            QStringLiteral("/videos/a b.mkv"),
            QStringLiteral("-frames:v"),
            QStringLiteral("1"),
            QStringLiteral("-vf"),
            QStringLiteral(
                "scale='min(384,iw)':'min(384,ih)':force_original_aspect_ratio=decrease"),
            QStringLiteral("-f"),
            QStringLiteral("image2pipe"),
            QStringLiteral("-c:v"),
            QStringLiteral("png"),
            QStringLiteral("-")};
        QCOMPARE(thumbnail_generator_arguments(QStringLiteral("/videos/a b.mkv"), 384, 5),
                 expected);

        QStringList fallback = expected;
        fallback[3] = QStringLiteral("0");
        QCOMPARE(thumbnail_generator_arguments(QStringLiteral("/videos/a b.mkv"), 384, 0),
                 fallback);

        QStringList narrower = expected;
        narrower[9] = QStringLiteral(
            "scale='min(256,iw)':'min(256,ih)':force_original_aspect_ratio=decrease");
        QCOMPARE(thumbnail_generator_arguments(QStringLiteral("/videos/a b.mkv"), 256, 5),
                 narrower);
    }

    void ffmpegReadsPngFromStdoutAndFallsBackToTheFirstFrame() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString args_file = dir.filePath(QStringLiteral("args.txt"));
        const QString ss_file = dir.filePath(QStringLiteral("ss.txt"));
        qputenv("ARGS_FILE", args_file.toUtf8());
        qputenv("SS_FILE", ss_file.toUtf8());
        const QString executable =
            fake_ffmpeg(dir,
                        "printf '%s\\n' \"$@\" >> \"$ARGS_FILE\"\n"
                        "ss=''\n"
                        "prev=''\n"
                        "for a in \"$@\"; do\n"
                        "  if [ \"$prev\" = '-ss' ]; then ss=\"$a\"; fi\n"
                        "  prev=\"$a\"\n"
                        "done\n"
                        "printf '%s\\n' \"$ss\" >> \"$SS_FILE\"\n"
                        "if [ \"$ss\" = '5' ]; then\n"
                        "  [ \"${FIVE:-0}\" = 1 ] && printf '%s' \"$FIVE_BYTES\"\n"
                        "else\n"
                        "  [ \"${ZERO:-1}\" = 1 ] && printf '%s' \"$ZERO_BYTES\"\n"
                        "fi\n"
                        "exit ${FAKE_EXIT:-0}\n");
        FfmpegThumbnailGenerator generator(executable);
        const QString source = dir.filePath(QStringLiteral("clip.mkv"));
        QVERIFY(write_file(source, "video"));

        qputenv("FIVE_BYTES", "FIVE");
        qputenv("ZERO_BYTES", "ZERO");
        qputenv("FIVE", "1");
        qputenv("ZERO", "1");
        QCOMPARE(generator.generate(source, 384, {}), QByteArrayLiteral("FIVE"));
        QCOMPARE(recorded_lines(ss_file), QStringList({QStringLiteral("5")}));

        qputenv("FIVE", "0");
        QFile::remove(ss_file);
        QCOMPARE(generator.generate(source, 384, {}), QByteArrayLiteral("ZERO"));
        QCOMPARE(recorded_lines(ss_file), QStringList({QStringLiteral("5"), QStringLiteral("0")}));

        qputenv("ZERO", "0");
        QCOMPARE(generator.generate(source, 384, {}), QByteArray{});
        qputenv("FAKE_EXIT", "1");
        QCOMPARE(generator.generate(source, 384, {}), QByteArray{});

        const QStringList arguments = recorded_lines(args_file);
        QVERIFY(!arguments.isEmpty());
        QVERIFY(arguments.contains(QStringLiteral("-nostdin")));
        QVERIFY(arguments.contains(QStringLiteral("image2pipe")));
        QVERIFY(arguments.contains(QStringLiteral(
            "scale='min(384,iw)':'min(384,ih)':force_original_aspect_ratio=decrease")));
    }

    void ffmpegDefaultsToTheExecutableOnPath() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString executable = dir.filePath(QStringLiteral("ffmpeg"));
        QVERIFY(write_file(executable, "#!/bin/sh\nprintf '%s' 'PATH-PNG'\n"));
        QFile::setPermissions(executable, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        const QByteArray saved_path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ":" + saved_path);
        const QString source = dir.filePath(QStringLiteral("clip.mkv"));
        QVERIFY(write_file(source, "video"));
        FfmpegThumbnailGenerator generator;
        const QByteArray result = generator.generate(source, 384, {});
        qputenv("PATH", saved_path);
        QCOMPARE(result, QByteArrayLiteral("PATH-PNG"));
    }

    void ffmpegCancellationKillsTheProcess() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString marker = dir.filePath(QStringLiteral("started"));
        qputenv("MARKER", marker.toUtf8());
        const QString executable = fake_ffmpeg(dir,
                                               "printf started > \"$MARKER\"\n"
                                               "sleep 30\n"
                                               "printf '%s' 'LATE'\n");
        FfmpegThumbnailGenerator generator(executable);
        const QString source = dir.filePath(QStringLiteral("clip.mkv"));
        QVERIFY(write_file(source, "video"));

        std::atomic<bool> cancelled{false};
        std::optional<QByteArray> result;
        const auto started = std::chrono::steady_clock::now();
        std::thread worker([&] {
            result = generator.generate(source, 384, [&cancelled] { return cancelled.load(); });
        });
        bool marker_seen = false;
        for (int attempt = 0; attempt < 500; ++attempt) {
            if (QFileInfo::exists(marker)) {
                marker_seen = true;
                break;
            }
            QTest::qWait(10);
        }
        cancelled = true;
        worker.join();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - started)
                                 .count();
        QVERIFY(marker_seen);
        QVERIFY(result.has_value());
        QVERIFY(result->isEmpty());
        QVERIFY(elapsed < 5000);
    }
};

QTEST_GUILESS_MAIN(VideoThumbnailStoreTest)
#include "tst_video_thumbnail_store.moc"
