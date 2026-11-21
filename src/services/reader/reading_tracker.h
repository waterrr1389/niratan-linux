// SPDX-License-Identifier: GPL-3.0-or-later
//
// One book's live reading statistics, following NativeMac/NativeReaderView.swift
// (NativeReaderModel: isStatisticsContextActive, loadStatistics,
// applyStatisticsHistory, regroupStatisticsHistory, startTracking,
// stopTracking, startTrackingOnPageTurnIfNeeded, updateStats,
// resetTrackingBaseline, flushStats, saveStats, reconcileStatisticsFocus,
// updateStatisticsResetTime, sessionStatistics, todaysStatistics,
// allTimeStatistics, and the On autostart in loadBook), Models/Statistics.swift
// (ReadingSession, ReadingTotal, ReadingSessionLog) and
// Features/Reader/ReaderStatisticsPersistencePolicy.swift.
//
// Scope
// -----
// The tracker owns the live session (one mutable ReadingSession and its record
// id), the book's persisted records and the totals derived from them. It never
// reads the clock, opens a file or starts a timer: every instant and character
// position is an argument, and persistence goes through the StatisticsStore the
// caller supplies. The view drives it from its own timer, page-turn callbacks
// and window or overlay state.
//
// Settings and lifecycle
// ----------------------
// configure() stores enableStatistics, statisticsAutostartMode and the reset
// minute (normalized to 0..1439). It loads the persisted records on the first
// enabled configure, so the read-only totals are available while autostart is
// Off or still waiting for a page turn. A changed reset minute re-groups the
// loaded history at once, including while statistics is disabled; the live
// session and its tracking baseline are left to the next timestamped call,
// because configure() has no instant.
//
// start() begins tracking only when statistics is enabled and the autostart
// mode is On. Page Turn starts on the first on_manual_navigation(); Off never
// starts. Starting while already tracking does nothing. The live session is
// kept when it already has activity (a stop/start on the same day continues
// it); otherwise it starts at the given instant. Tracking starts paused when
// the context is inactive. A position reported before tracking starts or while
// paused is retained without being credited; start() and a resume anchor the
// character baseline there, as startTracking and reconcileStatisticsFocus call
// resetTrackingBaseline, which reads the current character. When no position
// has been reported, the first update_position() after start or resume
// establishes the baseline and credits the elapsed time without characters.
//
// set_context_active() receives the result of the model's predicate
//     statistics_sheet_active || (reader_window_active && !reader_content_covered)
// so the statistics sheet keeps time running and a covered reader window
// pauses it. Becoming active resets the instant baseline and resumes; becoming
// inactive flushes and pauses. The three inputs stay in the view.
//
// Credit
// ------
// update_position() credits the time since the last baseline and
// max(characters - last_count, -session.characters_read) characters, in that
// form, then moves the baseline to the given position. A non-positive time
// delta credits nothing and leaves the baseline alone. A forward jump is
// credited in full; a backward jump can only take the session down to zero.
// While paused, not tracking or disabled, nothing is credited, but the
// reported position is retained as the baseline for the next start or resume.
// The caller
// must not report a position while a synced bookmark is being restored, as
// Swift's updateStats does not apply a position during that restore. flush()
// credits the time since the last baseline with no characters (the last known
// position is unchanged) and refuses while paused; it never persists by
// itself.
//
// Day boundary
// ------------
// Before crediting, a live session with activity whose reporting-day key
// differs from the reporting day of now is saved, its record id is replaced, a
// new session starts at now and the tracking baseline is reset, so the
// interval that spans the boundary is not credited. The reporting day is
// StatisticsDayBoundary of Models/Statistics.swift: the reset minute of
// statisticsResetTime shifts the local calendar day back one day.
//
// Persistence
// -----------
// save() reloads the stored records first, applies an external edit to the
// live session as applyStatisticsHistory does (a newer record for this id
// replaces the live values once; a tombstone starts a fresh session with a new
// id), and then writes only when the live session has activity and its stored
// value differs. It does nothing when statistics is disabled. The record's
// modification time is max(now_ms, last_saved_modified + 1). A reload that
// replaces the live session (a newer external value or a tombstone)
// re-anchors the tracking baseline at the retained position and the call
// instant, as reloadStatisticsAfterExternalMutation does after
// loadStatistics; a reload that keeps the live session leaves the baseline
// alone, as saveStats does, so the interval before a plain save is still
// credited by the next update. The caller's
// title is used for the derived daily rows and retained so a day-rotation save
// can write without one; an empty title keeps the retained one. A failed write
// does not add the live record to the in-memory history, so a later save
// retries it.
//
// Totals
// ------
// session_total() is the live session alone. today_total() is the loaded day
// total plus the live session when its own day key matches the reporting day of
// now. all_time_total() is every loaded session plus the live session. The live
// session is never counted twice after a save, because it is excluded from the
// loaded history by its record id.
//
// Known deviations from the Swift model
// -------------------------------------
// * The stale-bookmark guard of ReaderStatisticsPersistencePolicy is not
//   applied: save() receives no bookmark or position snapshot. The caller must
//   not call save() on behalf of a model whose position is known to be stale.
// * The day rotation resets the tracking baseline before crediting, so no
//   interval is split across the boundary. Swift's updateStats credits the
//   interval since the last update to the new session.
// * A synced bookmark restore is not applied: bookmark sync does not exist in
//   this port, and it has no sync coordinates, so Swift's
//   statisticsCharacterPosition preference for
//   sharedCoordinates.canonicalCharacter is not reproduced. Swift's
//   applyingSyncedBookmark path calls resetTrackingBaseline when the restored
//   position settles; here a jump or manual restore uses reset_position
//   instead and always anchors at the reader's own character offset.
// * configure() applies a reset-minute change without first crediting and
//   saving the live session under the old boundary, because it has no instant.
//   Swift's updateStatisticsResetTime does both.
// * save() receives its title from the caller and keeps the retained title for
//   an empty argument; Swift derives it from the stored daily rows and
//   metadata.

#pragma once

#include <QTimeZone>

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

#include "models/statistics/statistics.h"

namespace niratan::reader {

// Persistence seam for one book. load() reads the records and save() replaces
// them; both take the settings that decide day keys, and neither reads the
// clock. BookStatisticsStore forwards to services/statistics; a test supplies
// an in-memory double.
class StatisticsStore {
   public:
    virtual ~StatisticsStore() = default;

    virtual statistics::ReadingSessionRecords load(int reset_minutes,
                                                   const QTimeZone& zone) = 0;
    virtual bool save(const statistics::ReadingSessionRecords& records, std::string_view title,
                      int reset_minutes, const QTimeZone& zone, std::string* error) = 0;
};

// statistics_storage-backed StatisticsStore for one book root.
class BookStatisticsStore final : public StatisticsStore {
   public:
    explicit BookStatisticsStore(std::filesystem::path book_root);

    statistics::ReadingSessionRecords load(int reset_minutes, const QTimeZone& zone) override;
    bool save(const statistics::ReadingSessionRecords& records, std::string_view title,
              int reset_minutes, const QTimeZone& zone, std::string* error) override;

   private:
    std::filesystem::path book_root_;
};

class ReadingTracker {
   public:
    // The store is borrowed and must outlive the tracker. The zone is the local
    // calendar used for reporting days.
    explicit ReadingTracker(StatisticsStore& store,
                            QTimeZone zone = QTimeZone(QTimeZone::LocalTime));

    ReadingTracker(const ReadingTracker&) = delete;
    ReadingTracker& operator=(const ReadingTracker&) = delete;
    ReadingTracker(ReadingTracker&&) = delete;
    ReadingTracker& operator=(ReadingTracker&&) = delete;

    void configure(bool enabled, statistics::StatisticsAutostartMode autostart_mode,
                   int reset_minutes);

    // Enables tracking when enabled and autostart is On; see the lifecycle
    // rules above. Does nothing while already tracking.
    void start(std::int64_t now_ms);
    // The explicit reader control can start even when autostart is Off.
    void toggle(std::int64_t now_ms);
    void reset_position(int character, std::int64_t now_ms);

    // Flushes when not paused, then leaves tracking; does nothing when not
    // tracking.
    void stop(std::int64_t now_ms);

    // `active` is the context predicate result described above. Resuming resets
    // the instant baseline; pausing flushes first.
    void set_context_active(bool active, std::int64_t now_ms);

    // The Page Turn trigger. Starts tracking only in Page Turn mode and only
    // while not tracking; it does not move the position.
    void on_manual_navigation(std::int64_t now_ms);

    // Credits and moves the position baseline as described above.
    void update_position(int character, std::int64_t now_ms);

    // Credits the elapsed time with no character change. No-op when disabled,
    // not tracking, or paused.
    void flush(std::int64_t now_ms);

    // Reloads, applies external edits, then writes when the session has
    // activity and differs from its stored record. Returns whether a write
    // happened. The title is retained for later day-rotation saves.
    bool save(std::int64_t now_ms, std::string_view title);

    bool is_tracking() const;
    bool is_paused() const;
    const statistics::ReadingSession& session() const;

    statistics::ReadingTotal session_total() const;
    statistics::ReadingTotal today_total(std::int64_t now_ms) const;
    statistics::ReadingTotal all_time_total() const;

   private:
    StatisticsStore& store_;
    QTimeZone zone_;

    bool enabled_ = false;
    statistics::StatisticsAutostartMode autostart_mode_ =
        statistics::StatisticsAutostartMode::off;
    int reset_minutes_ = 0;
    bool context_active_ = false;

    bool tracking_ = false;
    bool paused_ = false;
    bool position_known_ = false;
    std::int64_t last_timestamp_ms_ = 0;
    int last_count_ = 0;
    std::string session_id_;
    statistics::ReadingSession current_session_;
    std::string title_;
    bool loaded_ = false;

    statistics::ReadingSessionRecords history_;
    std::map<std::string, statistics::ReadingTotal, std::less<>> history_day_totals_;
    statistics::ReadingTotal history_total_;
    std::int64_t last_saved_session_modified_ = 0;

    void begin_tracking(std::int64_t now_ms);
    void reload_history();
    void apply_history(statistics::ReadingSessionRecords records, std::int64_t now_ms);
    void regroup_history();
    void credit_time(std::int64_t now_ms);
    bool rotate_session(std::int64_t now_ms);
};

}  // namespace niratan::reader
