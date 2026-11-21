// SPDX-License-Identifier: GPL-3.0-or-later

#include "statistics_storage.h"
#include "statistics_storage_codec.h"

#include <QUuid>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace niratan::statistics {
namespace {

using detail::SessionsFile;

std::string new_session_id();

// ReadingSessionLog.importingDaily. Rows are deduplicated and visited in
// ascending date-key order; a visited row takes its day out of the untouched
// days that Replace tombstones at the end.
ReadingSessionRecords importing_daily(const std::vector<Statistics>& statistics,
                                      const ReadingSessionRecords& records, std::string_view key,
                                      StatisticsSyncMode mode, int reset_minutes,
                                      const QTimeZone& zone, std::int64_t now_ms) {
    ReadingSessionRecords updated = records;

    std::map<std::string, ReadingSessionDay, std::less<>> days;
    for (ReadingSessionDay& day : session_days(records, reset_minutes, zone)) {
        days.emplace(day.date_key, std::move(day));
    }

    std::vector<Statistics> imported_rows = deduplicated_statistics(statistics);
    std::ranges::sort(imported_rows, {}, &Statistics::date_key);

    for (const Statistics& imported : imported_rows) {
        std::optional<ReadingSessionDay> previous;
        std::vector<std::string> previous_ids;
        const auto day_position = days.find(imported.date_key);
        if (day_position != days.end()) {
            previous = std::move(day_position->second);
            days.erase(day_position);
            previous_ids.reserve(previous->sessions.size());
            for (const ReadingSessionEntry& entry : previous->sessions) {
                previous_ids.push_back(entry.id);
            }
        }

        const std::int64_t modified = imported.last_statistic_modified;
        if (mode == StatisticsSyncMode::merge && previous.has_value() &&
            previous->last_modified() >= modified) {
            continue;
        }

        const bool has_activity = imported.characters_read > 0 || imported.reading_time > 0.0;
        if (!has_activity && previous_ids.empty()) {
            continue;
        }

        const std::optional<ReadingSession> value =
            has_activity ? legacy_session(imported, reset_minutes, zone) : std::nullopt;

        // An unchanged Replace day keeps its existing stamps.
        if (mode == StatisticsSyncMode::replace && previous_ids.size() == 1 && value.has_value() &&
            previous->sessions.front().session == *value) {
            continue;
        }

        std::string id;
        if (!previous_ids.empty()) {
            id = *std::ranges::min_element(previous_ids);
        } else {
            id = legacy_session_id(key, imported.date_key);
        }
        const bool id_existed = std::ranges::find(previous_ids, id) != previous_ids.end();
        if (!id_existed && updated.contains(id)) {
            id = new_session_id();
        }

        const std::int64_t timestamp = mode == StatisticsSyncMode::replace ? now_ms : modified;
        for (const std::string& previous_id : previous_ids) {
            if (previous_id != id || !value.has_value()) {
                updated[previous_id] = TimestampedSession{timestamp, std::nullopt};
            }
        }
        if (value.has_value()) {
            updated[id] = TimestampedSession{timestamp, value};
        }
    }

    if (mode == StatisticsSyncMode::replace) {
        for (const auto& day : days) {
            for (const ReadingSessionEntry& entry : day.second.sessions) {
                updated[entry.id] = TimestampedSession{now_ms, std::nullopt};
            }
        }
    }

    return updated;
}

// Swift's root.lastPathComponent: the final component with trailing separators
// ignored.
std::string book_key(const std::filesystem::path& root) {
    std::filesystem::path path = root;
    while (!path.empty()) {
        const std::filesystem::path name = path.filename();
        if (!name.empty()) {
            return name.string();
        }
        const std::filesystem::path parent = path.parent_path();
        if (parent == path) {
            break;
        }
        path = parent;
    }
    return {};
}

std::string new_session_id() {
    // Swift generates a random UUID here; the case matches UUID.uuidString.
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

struct ReconcileResult {
    ReadingSessionRecords records;
    bool needs_save = false;
};

// StatisticsStorage.reconciled without the write. A decodable sessions file is
// the base; every deduplicated daily row whose exportedDaily entry differs is
// merged in. Without one, a non-empty daily document is converted.
ReconcileResult reconcile(const std::filesystem::path& root,
                          const std::optional<std::vector<Statistics>>& daily,
                          const std::optional<SessionsFile>& sessions_file, int reset_minutes,
                          const QTimeZone& zone) {
    if (!sessions_file.has_value()) {
        if (!daily.has_value() || daily->empty()) {
            return {};
        }
        return {legacy_sessions(*daily, book_key(root), reset_minutes, zone), true};
    }

    std::vector<Statistics> external;
    for (Statistics& row : deduplicated_statistics(daily.value_or(std::vector<Statistics>{}))) {
        const auto stamp = sessions_file->exported_daily.find(row.date_key);
        if (stamp == sessions_file->exported_daily.end() ||
            stamp->second != row.last_statistic_modified) {
            external.push_back(std::move(row));
        }
    }

    if (external.empty()) {
        return {sessions_file->records, false};
    }
    return {importing_daily(external, sessions_file->records, book_key(root),
                            StatisticsSyncMode::merge, reset_minutes, zone, 0),
            true};
}

}  // namespace

std::string_view statistics_sessions_file_name() {
    return detail::sessions_file_name;
}

std::string_view statistics_daily_file_name() {
    return detail::daily_file_name;
}

std::int64_t current_time_milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

StatisticsReadResult load_statistics(const std::filesystem::path& book_root, int reset_minutes,
                                     const QTimeZone& zone) {
    const std::optional<std::vector<Statistics>> daily_rows = detail::load_daily(book_root);
    const std::optional<SessionsFile> sessions_file = detail::load_sessions_file(book_root);
    const std::string title = detail::resolve_title(book_root, daily_rows);

    const ReconcileResult reconciled =
        reconcile(book_root, daily_rows, sessions_file, reset_minutes, zone);

    StatisticsReadResult result;
    result.records = reconciled.records;
    result.needs_save = reconciled.needs_save;
    if (result.needs_save) {
        // StatisticsStorage.load swallows a failed reconcile write and returns
        // the records it built either way.
        detail::write_files(book_root, result.records, title, reset_minutes, zone, nullptr);
    }
    result.daily = daily_statistics(result.records, title, reset_minutes, zone);
    return result;
}

std::vector<Statistics> read_daily_statistics(const std::filesystem::path& book_root,
                                              int reset_minutes, const QTimeZone& zone) {
    return read_dashboard_statistics(book_root, reset_minutes, zone)
        .value_or(std::vector<Statistics>{});
}

std::optional<std::vector<Statistics>>
read_dashboard_statistics(const std::filesystem::path& book_root, int reset_minutes,
                          const QTimeZone& zone) {
    const std::optional<std::vector<Statistics>> daily_rows = detail::load_daily(book_root);
    const std::optional<SessionsFile> sessions_file = detail::load_sessions_file(book_root);
    const ReconcileResult reconciled =
        reconcile(book_root, daily_rows, sessions_file, reset_minutes, zone);

    // StatisticsStorage.dailyStatistics: nil when there is no record and no
    // daily document. A book with neither file is an empty view here, because
    // this port's dashboard reads every book without Swift's hasStatistics
    // gate.
    if (reconciled.records.empty() && !daily_rows.has_value()) {
        if (!has_statistics(book_root)) {
            return std::vector<Statistics>{};
        }
        return std::nullopt;
    }
    return daily_statistics(reconciled.records, detail::resolve_title(book_root, daily_rows),
                            reset_minutes, zone);
}

bool save_statistics(const std::filesystem::path& book_root, const ReadingSessionRecords& records,
                     std::string_view, int reset_minutes, const QTimeZone& zone,
                     std::string* error) {
    // StatisticsStorage.save ignores any title the caller holds and writes the
    // resolved one.
    const std::string title = detail::resolve_title(book_root, detail::load_daily(book_root));
    return detail::write_files(book_root, records, title, reset_minutes, zone, error);
}

bool import_daily_statistics(const std::filesystem::path& book_root,
                             const std::vector<Statistics>& statistics, StatisticsSyncMode mode,
                             int reset_minutes, const QTimeZone& zone, std::int64_t now_ms,
                             std::string* error) {
    const StatisticsReadResult current = load_statistics(book_root, reset_minutes, zone);
    const ReadingSessionRecords updated = importing_daily(
        statistics, current.records, book_key(book_root), mode, reset_minutes, zone, now_ms);
    if (updated == current.records) {
        return true;
    }

    // StatisticsStorage.importDaily: the first supplied row's title, else the
    // resolved title.
    const std::string title = statistics.empty()
        ? detail::resolve_title(book_root, detail::load_daily(book_root))
        : statistics.front().title;
    return detail::write_files(book_root, updated, title, reset_minutes, zone, error);
}

bool has_statistics(const std::filesystem::path& book_root) {
    std::error_code error;
    if (std::filesystem::exists(book_root / std::filesystem::path(detail::sessions_file_name),
                                error)) {
        return true;
    }
    error.clear();
    return std::filesystem::exists(book_root / std::filesystem::path(detail::daily_file_name),
                                   error);
}

}  // namespace niratan::statistics
