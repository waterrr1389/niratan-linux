// SPDX-License-Identifier: GPL-3.0-or-later
//
// The single-concurrency poster scheduler. Follows
// Features/Video/VideoThumbnailStore.swift (`VideoThumbnailScheduler`) as
// stated in thumbnail_scheduler.h.

#include "services/video/thumbnail_scheduler.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QRunnable>
#include <QThreadPool>

#include <atomic>
#include <memory>
#include <utility>

namespace niratan::video {
namespace {

class GenerationTask : public QRunnable {
   public:
    explicit GenerationTask(std::function<void()> body) : body_(std::move(body)) {}

    void run() override {
        if (body_) {
            body_();
        }
    }

   private:
    std::function<void()> body_;
};

}  // namespace

struct ThumbnailScheduler::State {
    struct Waiter {
        quint64 id = 0;
        Completion completion;
    };

    struct PendingJob {
        ThumbnailRequest request;
        QString key;
        QList<Waiter> waiters;
    };

    State(ThumbnailScheduler* owner, std::shared_ptr<ThumbnailStore> thumbnail_store)
        : owner(owner), store(std::move(thumbnail_store)) {
        pool.setMaxThreadCount(maximum_concurrent_jobs());
    }

    bool is_suspended() const { return playback_count > 0 || lookup_count > 0 || mining_count > 0; }

    int& count_for(ThumbnailSuspendReason reason) {
        switch (reason) {
            case ThumbnailSuspendReason::playback:
                return playback_count;
            case ThumbnailSuspendReason::lookup:
                return lookup_count;
            case ThumbnailSuspendReason::mining:
                return mining_count;
        }
        return playback_count;
    }

    void cancel_pending() {
        const auto order = pending_order;
        const auto jobs = pending_jobs;
        pending_order.clear();
        pending_jobs.clear();
        for (const auto& key : order) {
            const auto job = jobs.constFind(key);
            if (job == jobs.cend()) {
                continue;
            }
            for (const auto& waiter : job->waiters) {
                resolve(waiter, std::nullopt);
            }
        }
    }

    void start_next() {
        if (running_id != 0 || is_suspended() || pending_order.isEmpty()) {
            return;
        }
        const QString key = pending_order.takeFirst();
        auto job = pending_jobs.take(key);
        if (job.waiters.isEmpty()) {
            start_next();
            return;
        }
        running_id = next_job_id++;
        running_key = key;
        running_waiters = std::move(job.waiters);
        running_cancelled = std::make_shared<std::atomic_bool>(false);
        const quint64 id = running_id;
        const auto request = job.request;
        const auto cancelled = running_cancelled;
        auto* state = this;
        pool.start(new GenerationTask([state, id, request, cancelled]() {
            const auto url =
                state->store->generate_url(request, [cancelled]() { return cancelled->load(); });
            QMetaObject::invokeMethod(
                state->owner, [state, id, url]() { state->finish(id, url); }, Qt::QueuedConnection);
        }));
    }

    void finish(quint64 id, std::optional<std::filesystem::path> url) {
        if (id != running_id) {
            return;
        }
        running_id = 0;
        running_key.clear();
        running_cancelled.reset();
        const auto waiters = running_waiters;
        running_waiters.clear();
        for (const auto& waiter : waiters) {
            resolve(waiter, url);
        }
        start_next();
    }

    void resolve(const Waiter& waiter, std::optional<std::filesystem::path> url) {
        if (waiter.completion) {
            waiter.completion(std::move(url));
        }
    }

    ThumbnailScheduler* owner = nullptr;
    std::shared_ptr<ThumbnailStore> store;
    QThreadPool pool;

    int playback_count = 0;
    int lookup_count = 0;
    int mining_count = 0;

    QList<QString> pending_order;
    QHash<QString, PendingJob> pending_jobs;

    quint64 running_id = 0;
    quint64 next_job_id = 1;
    quint64 next_request_id = 1;
    QString running_key;
    QList<Waiter> running_waiters;
    std::shared_ptr<std::atomic_bool> running_cancelled;
};

ThumbnailScheduler::ThumbnailScheduler(std::shared_ptr<ThumbnailStore> store, QObject* parent)
    : QObject(parent), state_(std::make_unique<State>(this, std::move(store))) {}

ThumbnailScheduler::ThumbnailScheduler(std::filesystem::path cache_directory,
                                       std::shared_ptr<ThumbnailGenerator> generator,
                                       QObject* parent)
    : ThumbnailScheduler(
          std::make_shared<ThumbnailStore>(std::move(cache_directory), std::move(generator)),
          parent) {}

ThumbnailScheduler::~ThumbnailScheduler() {
    // Do not resolve the queued waiters here: their callbacks belong to the
    // owner, whose members may already be gone. They are simply dropped.
    state_->pending_order.clear();
    state_->pending_jobs.clear();
    state_->running_waiters.clear();
    if (state_->running_cancelled) {
        state_->running_cancelled->store(true);
    }
    state_->pool.waitForDone();
}

quint64 ThumbnailScheduler::request(const ThumbnailRequest& request, ThumbnailRequestMode mode,
                                    Completion completion) {
    if (const auto cached = state_->store->cached_url(request)) {
        if (completion) {
            completion(cached);
        }
        return 0;
    }
    if (mode == ThumbnailRequestMode::cache_only) {
        if (completion) {
            completion(std::nullopt);
        }
        return 0;
    }
    if (state_->is_suspended()) {
        if (completion) {
            completion(std::nullopt);
        }
        return 0;
    }

    const QString key = thumbnail_cache_key(request);
    const quint64 id = state_->next_request_id++;
    State::Waiter waiter{id, std::move(completion)};
    auto existing = state_->pending_jobs.find(key);
    if (existing == state_->pending_jobs.end()) {
        State::PendingJob job;
        job.request = request;
        job.key = key;
        job.waiters.append(waiter);
        state_->pending_jobs.insert(key, job);
        state_->pending_order.append(key);
    } else {
        existing->waiters.append(waiter);
    }
    state_->start_next();
    return id;
}

void ThumbnailScheduler::cancel_request(quint64 request_id) {
    for (auto job = state_->pending_jobs.begin(); job != state_->pending_jobs.end(); ++job) {
        for (qsizetype index = 0; index < job->waiters.size(); ++index) {
            if (job->waiters.at(index).id != request_id) {
                continue;
            }
            const auto waiter = job->waiters.takeAt(index);
            if (job->waiters.isEmpty()) {
                const QString key = job.key();
                state_->pending_jobs.erase(job);
                state_->pending_order.removeAll(key);
            }
            state_->resolve(waiter, std::nullopt);
            return;
        }
    }
}

void ThumbnailScheduler::cancel_pending() {
    state_->cancel_pending();
}

void ThumbnailScheduler::suspend(ThumbnailSuspendReason reason) {
    int& count = state_->count_for(reason);
    if (reason == ThumbnailSuspendReason::playback) {
        count = 1;
    } else {
        count += 1;
    }
    if (state_->running_cancelled) {
        state_->running_cancelled->store(true);
    }
    state_->cancel_pending();
}

void ThumbnailScheduler::resume(ThumbnailSuspendReason reason) {
    int& count = state_->count_for(reason);
    if (count <= 0) {
        return;
    }
    count -= 1;
    state_->start_next();
}

bool ThumbnailScheduler::is_suspended() const {
    return state_->is_suspended();
}

}  // namespace niratan::video
