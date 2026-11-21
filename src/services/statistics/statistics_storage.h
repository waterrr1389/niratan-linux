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
// Both files are written indented through QSaveFile: the sessions file first,
// then the daily file, each replacing its target wholesale even when the
// target's current bytes are not valid JSON. The shared
// storage::write_json_file keeps its refusal to replace a damaged target for
// its other callers. A daily write failure leaves the new sessions file on
// disk.
//
// Decoding follows synthesized Codable: every key shown above is required,
// extra keys are ignored, and a document of the wrong shape is rejected as a
// whole. Integer keys require a number without a fractional part; the speed
// fields are int, so a value outside that range rejects the file, while
// charactersRead and lastStatisticModified are read as signed 64-bit integers,
// as Swift's Int is. The integer keys are read through QJsonValue::toInteger(),
// which keeps a parsed integer literal exact instead of rounding it through
// double. A file that is absent, unreadable or rejected is treated as absent,
// as Swift's decode does; the read paths never throw. A read that only derives
// rows never repairs the bytes, while a pending reconcile write during load
// does replace them. has_statistics reports the file's presence, not its
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
// load_statistics builds the records and the daily rows in memory.
//   1. With a usable sessions file, its records are the base. Every
//      deduplicated row of statistics.json whose exportedDaily entry differs
//      from its lastStatisticModified is then merged in with the Merge rules.
//   2. Without a usable sessions file, the rows of statistics.json are
//      converted by legacy_sessions().
// When either rule leaves work to persist, load_statistics writes both files
// before it returns, and a write failure is swallowed: the reconciled records
// are returned either way. needs_save reports that a reconcile was pending; it
// needs no action from the caller.
// The daily rows are then daily_statistics() of the resulting records: one
// row per day with activity, sorted by date key.
//
// The key hashed into a legacy session id is the book root's file name with
// trailing separators ignored, matching Swift's root.lastPathComponent.
//
// Title
// -----
// Both files carry one title: the first non-empty title among the stored
// daily rows in file order, else metadata.json's renamedTitle, else its title,
// else "". This is StatisticsStorage.title(root:daily:). save_statistics
// ignores the title its caller supplies; the argument stays in the signature
// because callers already hold the book's display title.
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
// title when no rows are supplied. load_statistics runs first, so a pending
// reconcile is persisted even when the imported rows change nothing; the
// import itself writes only when the records change.

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
    // True when statistics.json held work a load had to reconcile: a legacy
    // migration, or external daily rows that were merged in. load_statistics
    // has already written it; the flag is informational.
    bool needs_save = false;

    bool operator==(const StatisticsReadResult&) const = default;
};

// Reads both files, reconciles them as described above and returns the
// records with the daily rows derived from them. A pending reconcile is
// written before returning; a failed write is swallowed.
StatisticsReadResult load_statistics(const std::filesystem::path& book_root, int reset_minutes,
                                     const QTimeZone& zone = QTimeZone(QTimeZone::LocalTime));

// Read-only daily view: applies the same reconcile in memory, so an external
// daily row is merged and legacy rows are converted, and never writes. When
// the sessions file is absent or rejected and statistics.json is undecodable,
// the result is empty.
std::vector<Statistics> read_daily_statistics(
    const std::filesystem::path& book_root, int reset_minutes,
    const QTimeZone& zone = QTimeZone(QTimeZone::LocalTime));

// Dashboard read: a book without either file is an empty vector so a caller
// that reads every book, as this port's dashboard does, does not mark it
// corrupt; StatisticsStorage.hasStatistics is the gate Swift's caller applies
// before the read, where dailyStatistics is nil. A book whose reconciled
// records are empty and whose statistics.json did not decode returns nullopt,
// as Swift's nil does.
std::optional<std::vector<Statistics>> read_dashboard_statistics(
    const std::filesystem::path& book_root, int reset_minutes, const QTimeZone& zone);

// Writes both files from `records`: the sessions with exportedDaily set to
// each derived row's lastStatisticModified (the greater one on a duplicate
// date key), then the derived rows sorted by date key, all titled by the
// resolved title. `title` is ignored. Returns false, with a reason in `error`
// when given, if a write fails; the sessions file is written first, so a daily
// failure leaves the new sessions on disk.
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
