// SPDX-License-Identifier: GPL-3.0-or-later
// VideoPlayerViewModel.swift: saveCurrentPosition, handleSnapshot and restoreResumeOptions;
// HSMpvClient.mm: track metadata and selectTrackType.

#include "mpv_item.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace niratan::player {
namespace {

std::optional<QString> track_string(const QVariantMap& fields, const QString& key) {
    const auto value = fields.value(key);
    return value.isValid() ? std::optional(value.toString()) : std::nullopt;
}

QList<ResumeAudioTrack> audio_tracks(const QVariantList& tracks) {
    QList<ResumeAudioTrack> audio;
    for (const auto& entry : tracks) {
        const auto fields = entry.toMap();
        if (fields.value(QStringLiteral("type")).toString() != QStringLiteral("audio")) {
            continue;
        }
        const auto id = fields.value(QStringLiteral("id")).toLongLong();
        const auto index = fields.value(QStringLiteral("ff-index"), -1).toLongLong();
        auto title = fields.value(QStringLiteral("title")).toString();
        if (title.isEmpty()) {
            title = QStringLiteral("Audio %1").arg(id);
        }
        audio.append(ResumeAudioTrack{
            {id, index >= 0 ? std::optional(index) : std::nullopt, std::move(title),
             track_string(fields, QStringLiteral("lang")),
             track_string(fields, QStringLiteral("codec"))},
            fields.value(QStringLiteral("selected")).toBool()});
    }
    return audio;
}

}  // namespace
}  // namespace niratan::player

void MpvItem::save_playback_position() {
    if (!video_settings_.value("videoRememberPlaybackPosition").toBool()
        || !file_ready_ || !loaded_ || ended_ || local_video_path_.isEmpty()) {
        return;
    }
    if (get_bool("eof-reached", false)) {
        record_finished_playback();
        return;
    }
    const double position = get_double("time-pos", position_);
    if (!std::isfinite(position) || !std::isfinite(duration_)) {
        return;
    }
    const auto options = niratan::player::capture_resume_options(
        gap_playback_.persistence_speed(get_double("speed", speed_)),
        get_double("sub-delay", subtitle_delay_), get_double("audio-delay", audio_delay_),
        niratan::player::audio_tracks(current_track_fields()));
    playback_history_.save(local_video_path_, position, duration_, options);
}

void MpvItem::record_finished_playback() {
    if (video_settings_.value("videoRememberPlaybackPosition").toBool()
        && !local_video_path_.isEmpty() && std::isfinite(duration_)) {
        playback_history_.save(local_video_path_, duration_, duration_);
    }
}

void MpvItem::set_resume_position(double position) {
    if (resume_position_ != position) {
        resume_position_ = position;
        emit resumeChanged();
    }
}

void MpvItem::restore_playback_position() {
    const auto [position, options] = std::exchange(pending_playback_, {});
    if (options.speed) {
        set_property("speed", *options.speed);
    }
    if (options.subtitle_delay) {
        set_property("sub-delay", *options.subtitle_delay);
    }
    if (options.audio_delay) {
        set_property("audio-delay", *options.audio_delay);
    }
    if (options.audio_selection) {
        if (const auto* identity = std::get_if<niratan::player::AudioTrackIdentity>(
                &*options.audio_selection)) {
            const auto track = niratan::player::matching_audio_track_id(
                *identity, niratan::player::audio_tracks(current_track_fields()));
            if (track) {
                set_property("aid", QString::number(*track));
            }
        } else {
            set_property("aid", QStringLiteral("no"));
        }
    }
    if (position) {
        const double target = std::min(*position, duration_);
        seek(target);
        set_resume_position(target);
    }
}

void MpvItem::restartFromBeginning() {
    if (loaded_) {
        seek(0.0);
        if (!local_video_path_.isEmpty()) {
            playback_history_.clear(local_video_path_);
        }
        resume_save_timer_.start();
        set_resume_position(0.0);
    }
}
