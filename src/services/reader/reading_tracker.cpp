// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/reader/reading_tracker.h"

#include <QUuid>

#include <algorithm>
#include <utility>

#include "services/statistics/statistics_storage.h"

namespace niratan::reader {
namespace {

// A fresh record id. The case matches Swift's UUID.uuidString, so an id this
// module writes is spelled like one the original model wrote.
std::string new_session_id() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

QDateTime instant(std::int64_t milliseconds, const QTimeZone& zone) {
    return QDateTime::fromMSecsSinceEpoch(milliseconds, zone);
}

}  // namespace

BookStatisticsStore::BookStatisticsStore(std::filesystem::path book_root)
    : book_root_(std::move(book_root)) {}

statistics::ReadingSessionRecords BookStatisticsStore::load(int reset_minutes,
                                                            const QTimeZone& zone) {
    return statistics::load_statistics(book_root_, reset_minutes, zone).records;
}

bool BookStatisticsStore::save(const statistics::ReadingSessionRecords& records,
                               std::string_view title, int reset_minutes, const QTimeZone& zone,
                               std::string* error) {
    return statistics::save_statistics(book_root_, records, title, reset_minutes, zone, error);
}

ReadingTracker::ReadingTracker(StatisticsStore& store, QTimeZone zone)
    : store_(store), zone_(std::move(zone)) {}

void ReadingTracker::configure(bool enabled,
                               statistics::StatisticsAutostartMode autostart_mode,
                               int reset_minutes) {
    const int normalized = statistics::normalized_reset_minutes(reset_minutes);
    const bool reset_changed = normalized != reset_minutes_;
    enabled_ = enabled;
    autostart_mode_ = autostart_mode;
    reset_minutes_ = normalized;

    // The read-only totals must be available while autostart is Off or still
    // waiting for a page turn, so the first enabled configure loads them.
    if (enabled_ && !loaded_) {
        reload_history();
        return;
    }
    // Regrouping needs no instant: the live session and its baseline keep
    // their keys until the next timestamped call. A disabled configure still
    // re-keys loaded history, so re-enabling cannot inherit stale day keys.
    if (loaded_ && reset_changed) {
        regroup_history();
    }
}

void ReadingTracker::start(std::int64_t now_ms) {
    if (!enabled_ || autostart_mode_ != statistics::StatisticsAutostartMode::on) {
        return;
    }
    if (tracking_) {
        return;
    }
    begin_tracking(now_ms);
}

void ReadingTracker::stop(std::int64_t now_ms) {
    if (!tracking_) {
        return;
    }
    flush(now_ms);
    tracking_ = false;
    paused_ = false;
}

void ReadingTracker::set_context_active(bool active, std::int64_t now_ms) {
    context_active_ = active;
    if (active) {
        // Resuming restarts the time baseline without crediting the paused
        // interval; the character baseline is re-established by the view.
        if (!tracking_ || !paused_) {
            return;
        }
        last_timestamp_ms_ = now_ms;
        position_known_ = false;
        paused_ = false;
        return;
    }

    if (!tracking_ || paused_) {
        return;
    }
    flush(now_ms);
    paused_ = true;
}

void ReadingTracker::on_manual_navigation(std::int64_t now_ms) {
    if (!enabled_ || autostart_mode_ != statistics::StatisticsAutostartMode::page_turn ||
        tracking_) {
        return;
    }
    begin_tracking(now_ms);
}

void ReadingTracker::toggle(std::int64_t now_ms) {
    if (!enabled_) {
        return;
    }
    if (tracking_) {
        stop(now_ms);
    } else {
        begin_tracking(now_ms);
    }
}

void ReadingTracker::reset_position(int character, std::int64_t now_ms) {
    if (enabled_ && tracking_ && !paused_) {
        flush(now_ms);
        last_count_ = character;
        position_known_ = true;
    }
}

void ReadingTracker::update_position(int character, std::int64_t now_ms) {
    if (!enabled_ || !tracking_ || paused_) {
        return;
    }

    // A session belongs to the reporting day it started on; the interval that
    // spans the boundary is credited to neither session.
    if (rotate_session(now_ms)) {
        last_count_ = character;
        position_known_ = true;
        return;
    }

    // The start or resume instant has no position: this call only sets the
    // character baseline and credits the elapsed time.
    if (!position_known_) {
        credit_time(now_ms);
        last_count_ = character;
        position_known_ = true;
        return;
    }

    // A non-positive delta credits nothing and keeps the baseline, so a clock
    // that steps back cannot produce a negative interval.
    if (now_ms <= last_timestamp_ms_) {
        return;
    }

    const int character_delta = character - last_count_;
    const int credited = std::max(character_delta, -current_session_.characters_read);
    current_session_.track(credited, static_cast<double>(now_ms - last_timestamp_ms_) / 1000.0,
                           instant(now_ms, zone_));
    last_timestamp_ms_ = now_ms;
    last_count_ = character;
}

void ReadingTracker::flush(std::int64_t now_ms) {
    if (!enabled_ || !tracking_ || paused_) {
        return;
    }
    if (rotate_session(now_ms)) {
        return;
    }
    credit_time(now_ms);
}

bool ReadingTracker::save(std::int64_t now_ms, std::string_view title) {
    if (!enabled_) {
        return false;
    }

    // The title is retained so a day-rotation save can write without one.
    if (!title.empty()) {
        title_ = std::string(title);
    }

    reload_history();
    apply_history(history_, now_ms);

    if (!current_session_.has_activity()) {
        return false;
    }
    const auto stored = history_.find(session_id_);
    if (stored != history_.end() && stored->second.value.has_value() &&
        *stored->second.value == current_session_) {
        return false;
    }

    // Keep the modification time strictly increasing against this model's own
    // last write, so an equal stamp from another writer still loses.
    const std::int64_t modified = std::max(now_ms, last_saved_session_modified_ + 1);
    statistics::ReadingSessionRecords next = history_;
    next[session_id_] = statistics::TimestampedSession{modified, current_session_};
    if (!store_.save(next, title_, reset_minutes_, zone_, nullptr)) {
        // A failed write leaves the history untouched, so a later save retries.
        return false;
    }
    history_ = std::move(next);
    last_saved_session_modified_ = modified;
    regroup_history();
    return true;
}

bool ReadingTracker::is_tracking() const { return tracking_; }

bool ReadingTracker::is_paused() const { return paused_; }

const statistics::ReadingSession& ReadingTracker::session() const { return current_session_; }

statistics::ReadingTotal ReadingTracker::session_total() const {
    return current_session_.total();
}

statistics::ReadingTotal ReadingTracker::today_total(std::int64_t now_ms) const {
    const std::string key = statistics::date_key(instant(now_ms, zone_), reset_minutes_);
    statistics::ReadingTotal total;
    if (const auto day = history_day_totals_.find(key); day != history_day_totals_.end()) {
        total = day->second;
    }
    if (statistics::session_date_key(current_session_, reset_minutes_, zone_) == key) {
        total.add(current_session_);
    }
    return total;
}

statistics::ReadingTotal ReadingTracker::all_time_total() const {
    statistics::ReadingTotal total = history_total_;
    total.add(current_session_);
    return total;
}

void ReadingTracker::begin_tracking(std::int64_t now_ms) {
    // A stop and start on the same session continues the session that already
    // has activity instead of losing the interval before the stop.
    if (!current_session_.has_activity()) {
        current_session_ = statistics::ReadingSession::starting_at(instant(now_ms, zone_));
    }
    if (session_id_.empty()) {
        session_id_ = new_session_id();
    }

    tracking_ = true;
    paused_ = !context_active_;
    last_timestamp_ms_ = now_ms;
    last_count_ = 0;
    position_known_ = false;
}

void ReadingTracker::reload_history() {
    history_ = store_.load(reset_minutes_, zone_);
    loaded_ = true;
    regroup_history();
}

void ReadingTracker::apply_history(statistics::ReadingSessionRecords records,
                                   std::int64_t now_ms) {
    const auto record = records.find(session_id_);
    if (record != records.end()) {
        if (record->second.value.has_value()) {
            // An external edit made after this model last saved replaces the
            // live values once, so the newer writer wins.
            if (record->second.modified > last_saved_session_modified_ &&
                !(*record->second.value == current_session_)) {
                current_session_ = *record->second.value;
                last_saved_session_modified_ = record->second.modified;
            }
        } else {
            // The record was deleted elsewhere; a fresh id keeps the tombstone
            // from being overwritten by the next save.
            session_id_ = new_session_id();
            last_saved_session_modified_ = 0;
            current_session_ = statistics::ReadingSession::starting_at(instant(now_ms, zone_));
        }
    }
    history_ = std::move(records);
    regroup_history();
}

void ReadingTracker::regroup_history() {
    history_day_totals_.clear();
    history_total_ = statistics::ReadingTotal{};
    for (const statistics::ReadingSessionEntry& entry : statistics::session_entries(history_)) {
        // The live session is added by the totals and must not count twice.
        if (entry.id == session_id_) {
            continue;
        }
        history_total_.add(entry.session);
        history_day_totals_[statistics::session_date_key(entry.session, reset_minutes_, zone_)]
            .add(entry.session);
    }
}

void ReadingTracker::credit_time(std::int64_t now_ms) {
    const std::int64_t delta_ms = now_ms - last_timestamp_ms_;
    if (delta_ms <= 0) {
        return;
    }
    current_session_.track(0, static_cast<double>(delta_ms) / 1000.0, instant(now_ms, zone_));
    last_timestamp_ms_ = now_ms;
}

bool ReadingTracker::rotate_session(std::int64_t now_ms) {
    if (!current_session_.has_activity()) {
        return false;
    }
    const std::string now_key = statistics::date_key(instant(now_ms, zone_), reset_minutes_);
    if (statistics::session_date_key(current_session_, reset_minutes_, zone_) == now_key) {
        return false;
    }

    save(now_ms, title_);
    session_id_ = new_session_id();
    last_saved_session_modified_ = 0;
    current_session_ = statistics::ReadingSession::starting_at(instant(now_ms, zone_));

    // Neither session receives the interval that spans the boundary.
    last_timestamp_ms_ = now_ms;
    last_count_ = 0;
    position_known_ = false;
    regroup_history();
    return true;
}

}  // namespace niratan::reader
