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
    std::error_code error;
    std::filesystem::rename(std::filesystem::path(temp), std::filesystem::path(destination), error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(std::filesystem::path(temp), ignored);
        return false;
    }
    return true;
}

}  // namespace niratan::player
