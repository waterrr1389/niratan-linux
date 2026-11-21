// SPDX-License-Identifier: GPL-3.0-or-later

#include "mining_history_store.h"

#include <utility>

#include "services/player/video_settings.h"
#include "services/storage/app_paths.h"

namespace niratan::player {

MiningHistoryStore MiningHistoryStore::at_data_root() {
    return MiningHistoryStore(storage::data_root());
}

MiningHistoryStore::MiningHistoryStore(std::filesystem::path app_directory)
    : app_directory_(std::move(app_directory)) {}

MiningHistoryStore::Snapshot MiningHistoryStore::load_snapshot() const {
    if (app_directory_.empty()) {
        return {};
    }
    const int limit = player::load_video_settings(app_directory_)
                          .value("videoMiningHistoryLimit")
                          .toInt();
    return {player::pruned_items(player::load(app_directory_), limit), limit};
}

bool MiningHistoryStore::add(const MiningHistoryItem& item, int limit) const {
    if (app_directory_.empty()) {
        return false;
    }
    return player::add(app_directory_, item, limit);
}

bool MiningHistoryStore::remove(std::string_view id) const {
    if (app_directory_.empty()) {
        return false;
    }
    return player::remove(app_directory_, id);
}

bool MiningHistoryStore::clear() const {
    if (app_directory_.empty()) {
        return false;
    }
    return player::clear(app_directory_);
}

}  // namespace niratan::player
