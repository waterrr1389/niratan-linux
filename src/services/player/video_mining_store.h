// SPDX-License-Identifier: GPL-3.0-or-later
//
// Coordination of direct-media files that more than one card may want, so a
// repeated mine waits for the first capture instead of generating the same
// file again. Follows Features/Video/VideoMiningMediaStore.swift
// (directMediaURL, claimDirectMediaGeneration,
// waitForDirectMediaGeneration, finishDirectMediaGeneration, replaceMediaItem)
// and Core/AnkiManager.swift (writeDirectMedia reusing an existing
// destination).
//
// path_for is the media directory and the file name joined; the name is not
// sanitized here. claim owns a destination only while its file is absent and
// nobody else owns it. wait_for reports true inline when the file exists,
// waits when another caller owns the destination and receives that caller's
// finish result, and reports false inline when nobody owns it and the file is
// absent. finish reports succeeded && the file exists to every waiter and
// releases ownership, so a failed generation can be claimed again.
//
// replace moves the temporary file when the destination is absent and, when
// the destination already exists, discards the temporary file and keeps the
// destination, reporting true in both cases. A missing temporary file reports
// false and leaves the destination untouched.
//
// One thread: wait callbacks run inline from wait_for or finish, and may call
// back into the store. The store must outlive pending waits.
//
// Recorded difference: ownership and waiter keys are the destination strings
// as given, where Swift standardizes the file URL first. The producer builds
// one path per media name and passes that same path to claim, wait_for and
// finish, so the two agree for every input this port produces.

#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::player {

class DirectMediaStore {
   public:
    explicit DirectMediaStore(std::string media_directory);

    std::string path_for(std::string_view filename) const;

    bool claim(const std::string& destination);

    void wait_for(const std::string& destination, std::function<void(bool)> callback);

    void finish(const std::string& destination, bool succeeded);

    bool replace(const std::string& temp, const std::string& destination);

   private:
    std::string directory_;
    std::set<std::string> in_flight_;
    std::map<std::string, std::vector<std::function<void(bool)>>> waiters_;
};

}  // namespace niratan::player
