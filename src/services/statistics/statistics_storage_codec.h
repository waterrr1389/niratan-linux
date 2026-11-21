// SPDX-License-Identifier: GPL-3.0-or-later
//
// The JSON layer of services/statistics/statistics_storage, following
// Core/StatisticsStorage.swift's SessionsFile, loadDaily, decode, encode and
// title(root:daily:).
//
// Private to the module: only statistics_storage.cpp includes it. Both files
// are written with QSaveFile, each replacing its target wholesale, even when
// the target's current bytes are not valid JSON; a decode failure rejects the
// whole document.

#pragma once

#include <QTimeZone>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/statistics/statistics.h"

namespace niratan::statistics::detail {

inline constexpr std::string_view sessions_file_name = "statistics_sessions.json";
inline constexpr std::string_view daily_file_name = "statistics.json";
inline constexpr std::string_view metadata_file_name = "metadata.json";

// StatisticsStorage.SessionsFile: the source of truth plus the stamps of the
// daily rows this module last wrote.
struct SessionsFile {
    ReadingSessionRecords records;
    std::map<std::string, std::int64_t, std::less<>> exported_daily;
};

// Both loaders return nullopt for an absent, unreadable or rejected document.
std::optional<std::vector<Statistics>> load_daily(const std::filesystem::path& root);
std::optional<SessionsFile> load_sessions_file(const std::filesystem::path& root);

// StatisticsStorage.title(root:daily:): the first non-empty stored row title,
// else metadata.json's renamedTitle, else its title, else "".
std::string resolve_title(const std::filesystem::path& root,
                          const std::optional<std::vector<Statistics>>& daily);

// StatisticsStorage.write: derives the rows with
// ReadingSessionLog.dailyStatistics, builds exportedDaily from each row's
// lastStatisticModified (the greater one on a duplicate date key), writes the
// sessions file and then the daily file. A daily failure leaves the new
// sessions file on disk.
bool write_files(const std::filesystem::path& root, const ReadingSessionRecords& records,
                 std::string_view title, int reset_minutes, const QTimeZone& zone,
                 std::string* error);

}  // namespace niratan::statistics::detail
