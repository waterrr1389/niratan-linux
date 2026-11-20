// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/reading_tracker.h"

#include <utility>

#include "services/statistics/statistics_storage.h"

namespace niratan::reader {

BookStatisticsStore::BookStatisticsStore(std::filesystem::path book_root)
    : book_root_(std::move(book_root)) {}

statistics::StatisticsReadResult BookStatisticsStore::load(int reset_minutes,
                                                            const QTimeZone& zone) {
    return statistics::load_statistics(book_root_, reset_minutes, zone);
}

bool BookStatisticsStore::save(const statistics::ReadingSessionRecords& records,
                               std::string_view title, int reset_minutes, const QTimeZone& zone,
                               std::string* error) {
    return statistics::save_statistics(book_root_, records, title, reset_minutes, zone, error);
}

// The state machine is not implemented yet: every entry point below is inert,
// so tst_reading_tracker fails until the rules in the header are implemented.
ReadingTracker::ReadingTracker(StatisticsStore& store, QTimeZone zone)
    : store_(store), zone_(std::move(zone)) {}

void ReadingTracker::configure(bool enabled,
                               statistics::StatisticsAutostartMode autostart_mode,
                               int reset_minutes) {
    enabled_ = enabled;
    autostart_mode_ = autostart_mode;
    reset_minutes_ = statistics::normalized_reset_minutes(reset_minutes);
}

void ReadingTracker::start(std::int64_t) {}

void ReadingTracker::stop(std::int64_t) {}

void ReadingTracker::set_context_active(bool, std::int64_t) {}

void ReadingTracker::on_manual_navigation(std::int64_t) {}

void ReadingTracker::update_position(int, std::int64_t) {}

void ReadingTracker::flush(std::int64_t) {}

bool ReadingTracker::save(std::int64_t, std::string_view) { return false; }

bool ReadingTracker::is_tracking() const { return tracking_; }

bool ReadingTracker::is_paused() const { return paused_; }

const statistics::ReadingSession& ReadingTracker::session() const { return current_session_; }

statistics::ReadingTotal ReadingTracker::session_total() const {
    return statistics::ReadingTotal{};
}

statistics::ReadingTotal ReadingTracker::today_total(std::int64_t) const {
    return statistics::ReadingTotal{};
}

statistics::ReadingTotal ReadingTracker::all_time_total() const {
    return statistics::ReadingTotal{};
}

}  // namespace niratan::reader
