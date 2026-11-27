// SPDX-License-Identifier: GPL-3.0-or-later
// SyncManager.swift: parseProgressTimestamp, parseAudioBookTimestamp,
// determineSyncDirection, shouldImportAudioBook and mergeStatistics.
// Timestamps come from the millisecond filename field, not Drive metadata.
// An absent local or remote progress date loses to a present one; equal dates
// are synced. Statistics replace returns the incoming rows as given; merge
// uses dateKey and keeps the local row on equal or older incoming timestamps.

#pragma once

#include "drive_files.h"
#include "models/books/book_position.h"
#include "models/statistics/statistics.h"

namespace niratan::sync {

struct TtuResult {
    enum class Kind { synced, imported, exported, skipped };
    Kind kind = Kind::skipped;
    std::string title;
    int character_count = 0;
    bool operator==(const TtuResult&) const = default;
};

std::optional<std::int64_t> parse_progress_timestamp(std::string_view file_name);
std::optional<std::int64_t> parse_audio_book_timestamp(std::string_view file_name);
SyncDirection determine_direction(const std::optional<books::Bookmark>& local,
                                  const std::optional<DriveFile>& progress_file);
bool should_import_audio_book(const std::optional<DriveFile>& remote_file,
                              std::optional<std::int64_t> local_mtime_ms);
std::vector<statistics::Statistics>
merge_statistics(const std::vector<statistics::Statistics>& local,
                 const std::vector<statistics::Statistics>& external,
                 statistics::StatisticsSyncMode mode);

}  // namespace niratan::sync
