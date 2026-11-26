// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_mining_store.h"

#include <filesystem>
#include <system_error>
#include <utility>

namespace niratan::player {

namespace {

bool exists(const std::string& path) {
    std::error_code error;
    return std::filesystem::exists(std::filesystem::path(path), error);
}

}  // namespace

DirectMediaStore::DirectMediaStore(std::string media_directory)
    : directory_(std::move(media_directory)) {}

std::string DirectMediaStore::path_for(std::string_view filename) const {
    const auto path =
        std::filesystem::path(directory_) / std::filesystem::path(std::string(filename));
    return path.string();
}

bool DirectMediaStore::claim(const std::string& destination) {
    if (exists(destination) || in_flight_.contains(destination)) {
        return false;
    }
    in_flight_.insert(destination);
    return true;
}

void DirectMediaStore::wait_for(const std::string& destination,
                                std::function<void(bool)> callback) {
    if (exists(destination)) {
        callback(true);
        return;
    }
    if (!in_flight_.contains(destination)) {
        callback(false);
        return;
    }
    waiters_[destination].push_back(std::move(callback));
}

void DirectMediaStore::finish(const std::string& destination, bool succeeded) {
    in_flight_.erase(destination);
    const bool ready = succeeded && exists(destination);
    const auto found = waiters_.find(destination);
    if (found == waiters_.end()) {
        return;
    }
    auto callbacks = std::move(found->second);
    waiters_.erase(found);
    for (const auto& callback : callbacks) {
        callback(ready);
    }
}

bool DirectMediaStore::replace(const std::string& temp, const std::string& destination) {
    if (exists(destination)) {
        if (!exists(temp)) {
            return false;
        }
        std::error_code error;
        std::filesystem::remove(std::filesystem::path(temp), error);
        return !error;
    }
    if (!exists(temp)) {
        return false;
    }
    const std::filesystem::path from(temp);
    const std::filesystem::path to(destination);
    std::error_code error;
    std::filesystem::rename(from, to, error);
    if (error == std::errc::cross_device_link) {
        // FileManager.moveItem copies across volumes; the temporary location
        // (often a tmpfs) and the media directory are usually different ones.
        error.clear();
        std::filesystem::copy_file(from, to, error);
        if (error == std::errc::file_exists) {
            // Another writer finished the same file first; keep it, as when
            // the destination already existed.
            error.clear();
        } else if (error) {
            std::error_code ignored;
            std::filesystem::remove(to, ignored);
        }
    }
    std::error_code ignored;
    std::filesystem::remove(from, ignored);
    return !error;
}

}  // namespace niratan::player
