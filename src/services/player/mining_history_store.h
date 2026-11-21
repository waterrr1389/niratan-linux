// SPDX-License-Identifier: GPL-3.0-or-later
//
// Owned handle for the video mining history, wrapping the free functions in
// mining_history.h with a data root so viewmodels need not hold a path.
// Follows Features/Video/VideoMiningHistoryStore.swift
// (VideoMiningHistoryStore.init) in owning the storage location. Snapshot
// loading reads the configured limit before pruning and never writes.
// An empty root returns an empty snapshot and rejects every mutation.
// Item validation and mutation results follow mining_history.h.

#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "services/player/mining_history.h"

namespace niratan::player {

class MiningHistoryStore {
   public:
    static MiningHistoryStore at_data_root();
    explicit MiningHistoryStore(std::filesystem::path app_directory);

    struct Snapshot {
        std::vector<MiningHistoryItem> items;
        int limit = 0;

        bool operator==(const Snapshot&) const = default;
    };
    Snapshot load_snapshot() const;

    bool add(const MiningHistoryItem& item, int limit) const;
    bool remove(std::string_view id) const;
    bool clear() const;

    bool empty() const { return app_directory_.empty(); }

   private:
    std::filesystem::path app_directory_;
};

}  // namespace niratan::player
