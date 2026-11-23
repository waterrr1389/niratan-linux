// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video library view model's thumbnail surface. Follows
// Features/Video/VideoLibraryView.swift (`VideoThumbnailImageView.body`,
// `thumbnailTaskID`, `VideoThumbnailPlaceholderView`) and
// VideoThumbnailScheduler as described in thumbnail_scheduler.h: publish one
// `file:` URL per item path, keep published URLs across suspension, remember
// refused requests and retry them after the last suspension clears.

#include "viewmodels/video/video_library_view_model.h"

#include "services/video/thumbnail_scheduler.h"
#include "services/video/thumbnail_store.h"

#include <QUrl>

namespace niratan::viewmodels {

void VideoLibraryViewModel::setThumbnailsSuspended(bool suspended) {
    if (thumbnails_suspended_ == suspended) {
        return;
    }
    thumbnails_suspended_ = suspended;
    if (suspended) {
        thumbnails_->suspend(video::ThumbnailSuspendReason::playback);
    } else {
        thumbnails_->resume(video::ThumbnailSuspendReason::playback);
        resume_suspended_thumbnails();
    }
    emit thumbnailsSuspendedChanged();
}

void VideoLibraryViewModel::requestThumbnail(const QString& path) {
    if (item_for_path(path) == nullptr) {
        return;
    }
    request_thumbnail_now(path);
}

const video::LibraryItem* VideoLibraryViewModel::item_for_path(const QString& path) const {
    for (const auto& item : store_.catalog().items) {
        if (item.path == path) {
            return &item;
        }
    }
    return nullptr;
}

void VideoLibraryViewModel::request_thumbnail_now(const QString& path) {
    const auto* item = item_for_path(path);
    if (item == nullptr) {
        return;
    }
    const video::ThumbnailRequest request = video::ThumbnailRequest::for_item(*item);
    const QString key = video::thumbnail_cache_key(request);

    auto entry = published_thumbnails_.find(path);
    if (entry != published_thumbnails_.end() && entry->cache_key == key) {
        // Published already, or the same request is still pending.
        return;
    }
    if (entry != published_thumbnails_.end()) {
        if (const auto pending = pending_thumbnail_requests_.constFind(path);
            pending != pending_thumbnail_requests_.cend()) {
            thumbnails_->cancel_request(*pending);
        }
        pending_thumbnail_requests_.remove(path);
        if (const auto current_entry = published_thumbnails_.find(path);
            current_entry != published_thumbnails_.end()) {
            published_thumbnails_.erase(current_entry);
        }
        suspended_thumbnail_paths_.remove(path);
        if (thumbnail_urls_.remove(path) > 0) {
            emit thumbnailUrlsChanged();
        }
    }

    const auto completion = [this, path, key](auto url) {
        const auto* current = item_for_path(path);
        if (current == nullptr) {
            return;
        }
        if (video::thumbnail_cache_key(video::ThumbnailRequest::for_item(*current)) != key) {
            return;
        }
        const auto existing = published_thumbnails_.constFind(path);
        if (existing != published_thumbnails_.cend() && existing->cache_key != key) {
            return;
        }
        if (url.has_value()) {
            pending_thumbnail_requests_.remove(path);
            const QString text = QString::fromStdString(url->string());
            published_thumbnails_.insert_or_assign(path, PublishedThumbnail{key, text});
            publish_thumbnail(path, QUrl::fromLocalFile(text).toString());
            return;
        }
        if (existing != published_thumbnails_.cend()) {
            published_thumbnails_.erase(existing);
        }
        pending_thumbnail_requests_.remove(path);
        if (thumbnails_ && thumbnails_->is_suspended()) {
            suspended_thumbnail_paths_.insert(path);
        }
    };

    const quint64 id =
        thumbnails_->request(request, video::ThumbnailRequestMode::generate_if_missing, completion);
    if (id != 0) {
        pending_thumbnail_requests_.insert(path, id);
        published_thumbnails_.insert(path, PublishedThumbnail{key, {}});
    }
}

void VideoLibraryViewModel::publish_thumbnail(const QString& path, const QString& url) {
    if (thumbnail_urls_.value(path).toString() == url) {
        return;
    }
    thumbnail_urls_.insert(path, url);
    emit thumbnailUrlsChanged();
}

void VideoLibraryViewModel::resume_suspended_thumbnails() {
    const QStringList paths(suspended_thumbnail_paths_.begin(), suspended_thumbnail_paths_.end());
    suspended_thumbnail_paths_.clear();
    for (const auto& path : paths) {
        request_thumbnail_now(path);
    }
}

}  // namespace niratan::viewmodels
