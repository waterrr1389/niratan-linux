// SPDX-License-Identifier: GPL-3.0-or-later

#include "ttu_progress.h"

#include <charconv>
#include <map>

namespace niratan::sync {
namespace {

constexpr auto reference_epoch = 978307200.0;

std::optional<std::int64_t> timestamp(std::string_view name, std::string_view prefix,
                                      std::size_t minimum_parts) {
    if (!name.starts_with(prefix)) {
        return std::nullopt;
    }
    std::size_t count = 0;
    std::string_view field;
    while (!name.empty()) {
        const auto separator = name.find('_');
        const auto part = name.substr(0, separator);
        if (!part.empty() && ++count == 4) {
            field = part;
        }
        if (separator == name.npos) {
            break;
        }
        name.remove_prefix(separator + 1);
    }
    if (count < minimum_parts) {
        return std::nullopt;
    }
    if (field.starts_with('+')) {
        field.remove_prefix(1);
        if (field.empty() || field.starts_with('-')) {
            return std::nullopt;
        }
    }
    std::int64_t value = 0;
    const auto end = field.data() + field.size();
    const auto parsed = std::from_chars(field.data(), end, value);
    return parsed.ec == std::errc{} && parsed.ptr == end ? std::optional{value} : std::nullopt;
}

}  // namespace

std::optional<std::int64_t> parse_progress_timestamp(std::string_view file_name) {
    return timestamp(file_name, "progress_", 5);
}

std::optional<std::int64_t> parse_audio_book_timestamp(std::string_view file_name) {
    return timestamp(file_name, "audioBook_", 4);
}

SyncDirection determine_direction(const std::optional<books::Bookmark>& local,
                                  const std::optional<DriveFile>& progress_file) {
    const auto local_date = local ? local->last_modified : std::nullopt;
    const auto remote_date =
        progress_file ? parse_progress_timestamp(progress_file->name) : std::nullopt;
    if (!local_date) {
        return remote_date ? SyncDirection::import_from_ttu : SyncDirection::synced;
    }
    if (!remote_date) {
        return SyncDirection::export_to_ttu;
    }
    const auto local_seconds = *local_date + reference_epoch;
    const auto remote_seconds = *remote_date / 1000.0;
    if (local_seconds > remote_seconds) {
        return SyncDirection::export_to_ttu;
    }
    if (remote_seconds > local_seconds) {
        return SyncDirection::import_from_ttu;
    }
    return SyncDirection::synced;
}

bool should_import_audio_book(const std::optional<DriveFile>& remote_file,
                              std::optional<std::int64_t> local_mtime_ms) {
    const auto remote = remote_file ? parse_audio_book_timestamp(remote_file->name) : std::nullopt;
    return remote && (!local_mtime_ms || *remote > *local_mtime_ms);
}

std::vector<statistics::Statistics>
merge_statistics(const std::vector<statistics::Statistics>& local,
                 const std::vector<statistics::Statistics>& external,
                 statistics::StatisticsSyncMode mode) {
    if (mode == statistics::StatisticsSyncMode::replace) {
        return external;
    }
    std::map<std::string, statistics::Statistics> days;
    for (const auto& row : local) {
        days[row.date_key] = row;
    }
    for (const auto& row : external) {
        const auto [day, inserted] = days.try_emplace(row.date_key, row);
        if (!inserted && row.last_statistic_modified > day->second.last_statistic_modified) {
            day->second = row;
        }
    }
    std::vector<statistics::Statistics> result;
    result.reserve(days.size());
    for (auto& [key, row] : days) {
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace niratan::sync
