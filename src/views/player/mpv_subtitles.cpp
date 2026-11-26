// SPDX-License-Identifier: GPL-3.0-or-later
// VideoPlayerScreen+Subtitles.swift: cycleSubtitleTrack, selectSubtitleTrack,
// applySubtitlesOff, restoreRememberedSubtitleSelectionOrAutoload,
// autoloadSubtitleIfAvailable and applyEmbeddedSubtitleDefaultIfReady.
// HSMpvClient.mm: existingExternalSubtitleTrackIDForURL and
// loadExternalSubtitle.

#include "mpv_item.h"

#include <QFileInfo>

#include <algorithm>

namespace {

constexpr auto kRestoreGenerationProperty = "niratanSubtitleRestoreGeneration";

QList<QVariantMap> subtitle_tracks(const QVariantList& tracks) {
    QList<QVariantMap> result;
    for (const auto& entry : tracks) {
        const auto track = entry.toMap();
        if (track.value(QStringLiteral("type")).toString() == QStringLiteral("sub")) {
            result.append(track);
        }
    }
    return result;
}

int track_id(const QVariantMap& track) {
    return track.value(QStringLiteral("id")).toInt();
}

std::optional<QString> track_text(const QVariantMap& track, const QString& key) {
    const QString value = track.value(key).toString();
    return value.isEmpty() ? std::nullopt : std::optional(value);
}

niratan::player::VideoTrackType video_track_type(const QString& type) {
    if (type == QStringLiteral("audio")) {
        return niratan::player::VideoTrackType::audio;
    }
    if (type == QStringLiteral("sub")) {
        return niratan::player::VideoTrackType::subtitle;
    }
    return niratan::player::VideoTrackType::video;
}

niratan::player::SubtitleTrackIdentity identity(const QVariantMap& track) {
    const int id = track_id(track);
    const int index = track.value(QStringLiteral("ffIndex"), -1).toInt();
    auto title = track.value(QStringLiteral("title")).toString();
    if (title.isEmpty()) {
        // HSMpvClient's track list names untitled tracks by type and ID.
        title = QStringLiteral("Sub %1").arg(id);
    }
    return {id, index >= 0 ? std::optional(index) : std::nullopt, title,
            track.value(QStringLiteral("language")).toString(),
            track.value(QStringLiteral("codec")).toString()};
}

}  // namespace

QList<niratan::player::VideoTrack> MpvItem::video_tracks() const {
    QList<niratan::player::VideoTrack> result;
    for (const auto& entry : tracks_) {
        const auto track = entry.toMap();
        std::optional<int> index;
        if (track.contains(QStringLiteral("ffIndex"))) {
            index = track.value(QStringLiteral("ffIndex")).toInt();
        }
        result.append({track_id(track),
                       video_track_type(track.value(QStringLiteral("type")).toString()),
                       track.value(QStringLiteral("title")).toString(),
                       track_text(track, QStringLiteral("language")),
                       track_text(track, QStringLiteral("codec")), index,
                       track_text(track, QStringLiteral("externalFilename")),
                       track.value(QStringLiteral("isImage")).toBool(),
                       track.value(QStringLiteral("isSelected")).toBool()});
    }
    return result;
}

void MpvItem::reset_subtitle_selection_state() {
    subtitle_restore_pending_ = false;
    embedded_subtitle_default_.invalidate();
    set_property("sid", QStringLiteral("no"));
}

void MpvItem::refresh_tracks_and_embedded_default() {
    refresh_tracks();
    if (file_ready_) {
        restore_subtitle_selection();
    }
    apply_pending_embedded_subtitle_default();
}

void MpvItem::apply_pending_embedded_subtitle_default() {
    const auto activation = embedded_subtitle_default_.apply_if_ready(
        file_ready_ && !tracks_dirty_, video_tracks(), !subtitle_document_.isEmpty());
    if (!activation) {
        return;
    }
    const auto tracks = subtitle_tracks(tracks_);
    const auto track = std::ranges::find(tracks, activation->track_id, track_id);
    if (track != tracks.end()) {
        select_subtitle_track_with_transcript(*track, false);
    }
}

void MpvItem::remember_subtitle_selection(const niratan::player::SubtitleSelection& selection) {
    if (video_settings_.value("videoRememberPlaybackPosition").toBool()) {
        playback_history_.save_subtitle_selection(local_video_path_, selection);
    }
}

bool MpvItem::loadExternalSubtitle(const QUrl& file) {
    return load_external_subtitle(file, true);
}

bool MpvItem::load_external_subtitle(const QUrl& file, bool remember) {
    if (handle_ == nullptr || !file.isLocalFile() || file.toLocalFile().isEmpty()) {
        return false;
    }
    subtitle_restore_pending_ = false;
    embedded_subtitle_default_.invalidate();
    const QString absolute_path = QFileInfo(file.toLocalFile()).absoluteFilePath();
    if (file_ready_) {
        refresh_tracks();
    }
    const auto existing = niratan::player::existing_external_subtitle_track_id(
        QUrl::fromLocalFile(absolute_path), video_tracks());
    if (existing) {
        // mpv's sub-auto may already own the sidecar; select that track
        // instead of adding a second one.
        set_property("sid", *existing);
    } else {
        const QByteArray path = absolute_path.toUtf8();
        const char* args[] = {"sub-add", path.constData(), "select", nullptr};
        if (mpv_command(handle_, args) < 0) {
            return false;
        }
    }
    last_subtitle_id_ = static_cast<int>(get_double("sid", -1.0));
    setSubtitlesVisible(true);
    tracks_dirty_ = true;
    if (file_ready_) {
        refresh_tracks();
    }
    set_subtitle_cue({}, -1.0, -1.0);
    if (remember) {
        if (const auto bytes = niratan::player::read_subtitle_document(absolute_path)) {
            const auto path = niratan::player::primary_subtitle_selection_path(
                QUrl::fromLocalFile(absolute_path), *bytes);
            if (path) {
                remember_subtitle_selection(path->toLocalFile());
            }
        }
    }
    return true;
}

void MpvItem::selectSubtitleTrack(int id) {
    select_subtitle_track(id, true);
}

void MpvItem::select_subtitle_track(int id, bool remember) {
    subtitle_restore_pending_ = false;
    embedded_subtitle_default_.invalidate();
    if (id <= 0) {
        return;
    }
    set_property("sid", id);
    setSubtitlesVisible(true);
    if (file_ready_) {
        refresh_tracks();
    }
    set_subtitle_cue({}, -1.0, -1.0);
    if (selected_subtitle_id_ != id) {
        return;
    }
    last_subtitle_id_ = id;
    if (!remember) {
        return;
    }
    const auto tracks = subtitle_tracks(tracks_);
    const auto track = std::ranges::find(tracks, id, track_id);
    if (track == tracks.end()) {
        return;
    }
    if (!subtitle_document_.isEmpty()) {
        remember_subtitle_selection(QFileInfo(subtitle_document_.toLocalFile()).absoluteFilePath());
    } else if (!track->value(QStringLiteral("isExternal")).toBool()) {
        remember_subtitle_selection(identity(*track));
    }
}

void MpvItem::select_subtitle_track_with_transcript(const QVariantMap& track, bool remember) {
    const int id = track_id(track);
    select_subtitle_track(id, remember);
    if (selected_subtitle_id_ == id && !track.value(QStringLiteral("isExternal")).toBool() &&
        !track.value(QStringLiteral("isImage")).toBool()) {
        extractTranscript();
    }
}

void MpvItem::disableSubtitles() {
    disable_subtitles(true);
}

void MpvItem::disable_subtitles(bool remember) {
    subtitle_restore_pending_ = false;
    embedded_subtitle_default_.invalidate();
    if (selected_subtitle_id_ > 0) {
        last_subtitle_id_ = selected_subtitle_id_;
    }
    set_property("sid", QStringLiteral("no"));
    setSubtitlesVisible(false);
    if (file_ready_) {
        refresh_tracks();
    }
    set_subtitle_cue({}, -1.0, -1.0);
    if (remember) {
        remember_subtitle_selection(std::monostate{});
    }
}

int MpvItem::cycleSubtitles() {
    const auto tracks = subtitle_tracks(tracks_);
    if (!file_ready_ || tracks.isEmpty()) {
        return -1;
    }
    auto next = tracks.begin();
    if (subtitles_visible_) {
        const auto selected = std::ranges::find(tracks, selected_subtitle_id_, track_id);
        if (selected != tracks.end()) {
            next = selected + 1;
            if (next == tracks.end()) {
                disableSubtitles();
                return 0;
            }
        }
    } else {
        const auto previous = std::ranges::find(tracks, last_subtitle_id_, track_id);
        if (previous != tracks.end()) {
            next = previous;
        }
    }
    select_subtitle_track_with_transcript(*next, true);
    return selected_subtitle_id_ == track_id(*next) ? selected_subtitle_id_ : -1;
}

void MpvItem::restore_subtitle_selection() {
    if (!file_ready_ || local_video_path_.isEmpty()) {
        return;
    }
    const quint64 restored_generation = property(kRestoreGenerationProperty).toULongLong();
    if (!subtitle_restore_pending_ && restored_generation == load_generation_) {
        return;
    }
    setProperty(kRestoreGenerationProperty, load_generation_);
    if (video_settings_.value("videoRememberPlaybackPosition").toBool()) {
        if (const auto saved = playback_history_.subtitle_selection(local_video_path_)) {
            const auto resolution = niratan::player::resolve_subtitle_restore(
                *saved, video_tracks(), file_ready_ && !tracks_dirty_);
            switch (resolution.kind) {
                case niratan::player::SubtitleRestoreKind::off:
                    disable_subtitles(false);
                    return;
                case niratan::player::SubtitleRestoreKind::external:
                    if (!load_external_subtitle(resolution.external, false)) {
                        emit subtitleLoadFailed();
                    }
                    return;
                case niratan::player::SubtitleRestoreKind::embedded_track: {
                    const auto tracks = subtitle_tracks(tracks_);
                    const auto track = std::ranges::find(tracks, resolution.track_id, track_id);
                    if (track != tracks.end()) {
                        select_subtitle_track_with_transcript(*track, false);
                    }
                    return;
                }
                case niratan::player::SubtitleRestoreKind::waiting_for_tracks:
                    subtitle_restore_pending_ = true;
                    return;
                case niratan::player::SubtitleRestoreKind::unavailable:
                    break;
            }
        }
    }

    const auto plan =
        niratan::player::plan_subtitle_first_open(QUrl::fromLocalFile(local_video_path_));
    if (plan.sidecar) {
        if (!load_external_subtitle(*plan.sidecar, true)) {
            emit subtitleLoadFailed();
        }
        return;
    }
    embedded_subtitle_default_.wait_for_tracks();
    apply_pending_embedded_subtitle_default();
}
