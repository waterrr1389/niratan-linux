// SPDX-License-Identifier: GPL-3.0-or-later
//
// Per-book statistics storage, following Core/StatisticsStorage.swift
// (StatisticsStorage and its private SessionsFile) and the rules it calls in
// Models/Statistics.swift (ReadingSessionLog.legacySessions, dailyStatistics
// and importingDaily, StatisticsEditor.deduplicated, StatisticsClock).
//
// Files
// -----
// Both files live directly in the book root, the folder that also holds
// metadata.json:
//   statistics_sessions.json  the source of truth
//   statistics.json           the TTU-compatible daily rows, always derived
//
// statistics_sessions.json:
//   { "sessions": { "<id>": { "modified": <ms>, "value": <session or null> } },
//     "exportedDaily": { "<dateKey>": <ms> } }
// A session is { "startedAt": <ms>, "endedAt": <ms>, "charactersRead": <int>,
// "readingTime": <seconds> }. A null value is a tombstone: it keeps the id and
// its modification time and contributes no session. exportedDaily records the
// lastStatisticModified of each daily row this module last wrote, so a row
// written by something else is recognisable.
//
// statistics.json is a JSON array of Statistics objects with the Swift keys
// (title, dateKey, charactersRead, readingTime, minReadingSpeed,
// altMinReadingSpeed, lastReadingSpeed, maxReadingSpeed,
// lastStatisticModified). It is rewritten from the sessions, never merged
// into, whenever this module saves.
//
// Both files are written indented through storage::write_json_file, which
// replaces the target atomically and refuses to replace bytes it cannot parse
// as JSON.
//
// Decoding follows synthesized Codable: every key shown above is required,
// extra keys are ignored, and a document of the wrong shape is rejected as a
// whole. Integer keys require a number without a fractional part inside the
// field's range. A file that is absent, unreadable or rejected is treated as
// absent, as Swift's decode does; the read paths never throw and never repair
// the bytes on disk. has_statistics reports the file's presence, not its
// validity.
//
// Time
// ----
// Instants are milliseconds since the Unix epoch, as in the statistics model.
// Day boundaries are calendar days of `zone`, so callers pass the zone that
// means local. current_time_milliseconds() is the only clock read here, and
// import_daily_statistics takes its stamp as `now_ms` so a caller or a test
// can pin it. Merge imports stamp from the imported row instead and ignore
// `now_ms`.
//
// Reconciliation
// --------------
// load_statistics builds the records and the daily rows in memory and never
// writes; needs_save tells the caller whether save_statistics is required.
//   1. With a usable sessions file, its records are the base. Every
//      deduplicated row of statistics.json whose exportedDaily entry differs
//      from its lastStatisticModified is then merged in with the Merge rules,
//      and needs_save becomes true when there is at least one such row.
//   2. Without a usable sessions file, the rows of statistics.json are
//      converted by legacy_sessions() and needs_save is true whenever that
//      file holds at least one row, even when no row converts.
// The daily rows are then daily_statistics() of the resulting records: one
// row per day with activity, sorted by date key.
//
// The key hashed into a legacy session id is the book root's file name with
// trailing separators ignored, matching Swift's root.lastPathComponent.
//
// Title
// -----
// Derived rows carry one title: the first non-empty title among the stored
// daily rows in file order, else metadata.json's renamedTitle, else its title,
// else "". This is StatisticsStorage.title(root:daily:). save_statistics
// instead writes the title its caller supplies, because the title lives on
// BookMetadata and the caller already holds it.
//
// Import
// ------
// import_daily_statistics follows ReadingSessionLog.importingDaily. Rows are
// deduplicated and visited in ascending date-key order; the day a row covers
// is taken out of the untouched days as it is visited.
//   Merge: a row is skipped when the existing day's last modification is not
//   older than the row's lastStatisticModified. A row without activity is a
//   deletion: it tombstones the existing day, and is ignored when the day has
//   no sessions.
//   Replace: every affected record and every untouched existing day's
//   sessions are stamped with now_ms, so days the import does not cover stop
//   being visible.
// The written daily file uses the first supplied row's title, or the resolved
// title when no rows are supplied. Nothing is written when the records do not
// change, so a pending legacy migration stays unwritten until a caller saves.

#pragma once

#include <QTimeZone>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "models/statistics/statistics.h"

namespace niratan::statistics {

// "statistics_sessions.json" and "statistics.json".
std::string_view statistics_sessions_file_name();
std::string_view statistics_daily_file_name();

// StatisticsClock.milliseconds(.now).
std::int64_t current_time_milliseconds();

struct StatisticsReadResult {
    ReadingSessionRecords records;
    std::vector<Statistics> daily;
    // True when statistics.json holds a row that is not reconciled into
    // records: a legacy migration is pending, or external daily rows were
    // merged in.
    bool needs_save = false;

    bool operator==(const StatisticsReadResult&) const = default;
};

// Reads both files, reconciles them as described above and returns the
// records with the daily rows derived from them. Never writes.
StatisticsReadResult load_statistics(const std::filesystem::path& book_root, int reset_minutes,
                                     const QTimeZone& zone = QTimeZone(QTimeZone::LocalTime));

// Read-only daily view: derives the rows from a usable sessions file without
// importing external daily rows or writing. When the sessions file is absent
// or rejected, the visible rows of statistics.json are returned as stored
// (deduplicated, rows without activity dropped, sorted by date key). An empty
// result means there is nothing to show; it does not distinguish a missing
// file from an empty one.
std::vector<Statistics> read_daily_statistics(
    const std::filesystem::path& book_root, int reset_minutes,
    const QTimeZone& zone = QTimeZone(QTimeZone::LocalTime));

// Dashboard read: absent or empty data is an empty vector; an existing
// book with no decodable statistics file returns nullopt.
std::optional<std::vector<Statistics>> read_dashboard_statistics(
    const std::filesystem::path& book_root, int reset_minutes, const QTimeZone& zone);

// Writes both files from `records`: the sessions with exportedDaily set to
// each derived row's lastStatisticModified (the greater one on a duplicate
// date key), then the derived rows sorted by date key. Returns false, with a
// reason in `error` when given, if a write fails; the sessions file is
// written first, so a daily failure leaves the new sessions on disk.
bool save_statistics(const std::filesystem::path& book_root,
                     const ReadingSessionRecords& records, std::string_view title,
                     int reset_minutes, const QTimeZone& zone = QTimeZone(QTimeZone::LocalTime),
                     std::string* error = nullptr);

// Applies daily rows to the records load_statistics would return and saves the
// result when it changed. `now_ms` stamps Replace imports only. Returns false
// only when a needed write fails.
bool import_daily_statistics(const std::filesystem::path& book_root,
                             const std::vector<Statistics>& statistics, StatisticsSyncMode mode,
                             int reset_minutes,
                             const QTimeZone& zone = QTimeZone(QTimeZone::LocalTime),
                             std::int64_t now_ms = current_time_milliseconds(),
                             std::string* error = nullptr);

// True when either file exists at the book root, whatever its contents and
// even when it is a directory, as FileManager.fileExists reports.
bool has_statistics(const std::filesystem::path& book_root);

}  // namespace niratan::statistics
