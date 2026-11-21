// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader_view_model_state.h"

#include <QLocale>

#include <algorithm>

#include "models/statistics/reader_statistics.h"

namespace niratan::viewmodels {

int ReaderViewModel::statistics_today_characters() const {
    return state_->statistics_today_characters;
}

int ReaderViewModel::statistics_today_seconds() const { return state_->statistics_today_seconds; }

int ReaderViewModel::statistics_session_speed() const { return state_->statistics_session_speed; }

int ReaderViewModel::statistics_session_minutes() const {
    return state_->statistics_session_minutes;
}

bool ReaderViewModel::statistics_enabled() const { return state_->statistics_enabled; }

bool ReaderViewModel::tracking() const { return state_->tracking; }

QVariantMap ReaderViewModel::statistics_inspector() const {
    if (!state_->tracker || !state_->session) return {};
    const auto language = english_content() ? profiles::ContentLanguage::english
                                           : profiles::ContentLanguage::japanese;
    return statistics::reader_statistics_map(
        statistics::present_reader_statistics(
            state_->tracker->session_total(), state_->tracker->today_total(now_milliseconds()),
            state_->tracker->all_time_total(), language, state_->session->info().character_count,
            character_offset(), chapter_characters_remaining(), QLocale()),
        language);
}

void ReaderViewModel::toggleTracking() {
    if (!state_->tracker) {
        return;
    }
    const auto now = now_milliseconds();
    state_->tracker->toggle(now);
    if (state_->tracker->is_tracking()) {
        state_->tracker->reset_position(character_offset(), now);
    } else {
        save_statistics();
    }
    refresh_statistics(true);
}

void ReaderViewModel::tickTracking() {
    if (tracking()) {
        state_->tracker->flush(now_milliseconds());
        refresh_statistics(true);
    }
}

void ReaderViewModel::setReaderActive(bool active) {
    state_->reader_active = active;
    if (state_->tracker) {
        state_->tracker->set_context_active(active, now_milliseconds());
    }
    refresh_statistics();
}

void ReaderViewModel::save_statistics() {
    if (!state_->tracker) {
        return;
    }
    // Credit elapsed time at the position whose bookmark was just persisted.
    const auto now = now_milliseconds();
    state_->tracker->flush(now);
    state_->tracker->save(now, state_->book.display_title());
}

void ReaderViewModel::refresh_statistics(bool notify) {
    statistics::ReadingTotal today;
    statistics::ReadingTotal session;
    if (state_->tracker) {
        today = state_->tracker->today_total(now_milliseconds());
        session = state_->tracker->session_total();
    }
    const int today_characters = std::max(today.characters_read, 0);
    const int today_seconds = static_cast<int>(std::max(today.reading_time, 0.0));
    const int session_speed = session.reading_speed();
    const int session_minutes = static_cast<int>(std::max(session.reading_time, 0.0) / 60);
    const bool tracking = state_->tracker && state_->tracker->is_tracking();
    if (!notify && state_->statistics_today_characters == today_characters &&
        state_->statistics_today_seconds == today_seconds &&
        state_->statistics_session_speed == session_speed &&
        state_->statistics_session_minutes == session_minutes && state_->tracking == tracking) {
        return;
    }
    state_->statistics_today_characters = today_characters;
    state_->statistics_today_seconds = today_seconds;
    state_->statistics_session_speed = session_speed;
    state_->statistics_session_minutes = session_minutes;
    state_->tracking = tracking;
    emit statisticsChanged();
}

}  // namespace niratan::viewmodels
