// SPDX-License-Identifier: GPL-3.0-or-later
//
// The engine side of subtitle gap fast-forward: VideoPlayerViewModel.swift
// `updateSubtitleGapPlayback`, `setSpeed`, `adjustSpeed` and the restore
// points before seek, opening media and shutdown. The policy itself is
// player::SubtitleGapPlayback; this file feeds it the primary transcript the
// study model parsed and applies the effective speed it returns, so a manual
// speed change during acceleration only moves the base.

#include "mpv_item.h"

void MpvItem::apply_speed(double speed) {
    const qreal normalized = niratan::player::normalized_speed(speed);
    if (speed_ == normalized) {
        return;
    }
    speed_ = normalized;
    set_property("speed", static_cast<double>(speed_));
    emit speedChanged();
}

void MpvItem::setSpeed(qreal speed) {
    if (const auto applied = gap_playback_.set_user_speed(speed, speed_)) {
        apply_speed(*applied);
    }
}

void MpvItem::apply_gap_settings() {
    if (const auto applied = gap_playback_.set_enabled(
            video_settings_.value("videoSubtitleGapFastForwardEnabled").toBool(), speed_)) {
        apply_speed(*applied);
    }
    if (const auto applied = gap_playback_.set_configured_speed(
            video_settings_.value("videoSubtitleGapFastForwardSpeed").toDouble(), speed_)) {
        apply_speed(*applied);
    }
    update_gap_playback();
}

void MpvItem::update_gap_playback() {
    if (!primary_store_) {
        restore_gap_speed();
        return;
    }
    if (const auto applied = gap_playback_.update(*primary_store_, position_, subtitle_delay_,
                                                  loaded_ && !paused_ && !ended_, speed_)) {
        apply_speed(*applied);
    }
}

void MpvItem::restore_gap_speed() {
    if (const auto applied = gap_playback_.restore(speed_)) {
        apply_speed(*applied);
    }
}

void MpvItem::setPrimaryCues(const QVariantList& cues) {
    if (cues.isEmpty()) {
        primary_store_.reset();
    } else {
        niratan::player::SubtitleDocument document;
        document.cues.reserve(cues.size());
        for (const QVariant& entry : cues) {
            const QVariantMap cue = entry.toMap();
            document.cues.append({QString(), cue.value(QStringLiteral("start")).toDouble(),
                                  cue.value(QStringLiteral("end")).toDouble(), QString()});
        }
        primary_store_.emplace(std::move(document));
    }
    update_gap_playback();
}
