// SPDX-License-Identifier: GPL-3.0-or-later
//
// Queue video poster generation with one job at a time and suspend it while
// playback, a lookup popup or mining has priority, following
// Features/Video/VideoThumbnailStore.swift: `VideoThumbnailScheduler`
// (`maximumConcurrentJobs`, `thumbnailURL(for:requestMode:)`,
// `suspend(reason:)`, `resume(reason:)`, `cancelPending`, `enqueue`,
// `cancelPendingRequest`, `startNextIfNeeded`, `finishRunningJob`).
//
// `request` checks the cache before the request mode and the suspension
// guards: a cached item resolves even while suspended and in cache-only mode.
// `cache_only` never enqueues or generates. `generate_if_missing` enqueues an
// uncached item; a request while suspended resolves with nothing and creates
// no job. Requests for the same cache key share one generation and every
// waiter gets the same result. Generation runs on the scheduler's own worker
// thread, never on the caller's.
//
// The completion callback runs on the thread that owns the scheduler. A cache
// hit or a refusal may invoke it before `request` returns; an enqueued job
// invokes it later, exactly once. The returned id cancels that one waiter;
// cancelling the last waiter of a not-yet-running job drops the job, while
// cancelling a waiter of the running job leaves the generation alone.
//
// `suspend` cancels the running generation and every pending job, and their
// waiters get nothing. A playback suspension is pinned at one count; lookup
// and mining suspensions count and clear one at a time. Once the last
// suspension clears, the next job starts only after the cancelled generation
// has stopped. A cancelled generation writes no cache file. `cancel_pending`
// resolves the queued jobs and keeps the running one.
//
// Destruction cancels and joins the worker so no callback runs after it.

#pragma once

#include <QObject>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "services/video/thumbnail_store.h"

namespace niratan::video {

class ThumbnailScheduler : public QObject {
    Q_OBJECT

   public:
    using Completion = std::function<void(std::optional<std::filesystem::path>)>;

    explicit ThumbnailScheduler(std::shared_ptr<ThumbnailStore> store, QObject* parent = nullptr);
    explicit ThumbnailScheduler(std::filesystem::path cache_directory,
                                std::shared_ptr<ThumbnailGenerator> generator = {},
                                QObject* parent = nullptr);
    ~ThumbnailScheduler() override;

    ThumbnailScheduler(const ThumbnailScheduler&) = delete;
    ThumbnailScheduler& operator=(const ThumbnailScheduler&) = delete;

    // Zero means no job was created (cache hit, refusal or cancellation).
    quint64 request(const ThumbnailRequest& request, ThumbnailRequestMode mode,
                    Completion completion);
    void cancel_request(quint64 request_id);
    void cancel_pending();

    void suspend(ThumbnailSuspendReason reason);
    void resume(ThumbnailSuspendReason reason);
    bool is_suspended() const;

    static constexpr int maximum_concurrent_jobs() { return 1; }

   private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace niratan::video
