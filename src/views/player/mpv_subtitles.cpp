// SPDX-License-Identifier: GPL-3.0-or-later
// VideoPlayerScreen+Subtitles.swift: cycleSubtitleTrack, selectSubtitleTrack,
// applySubtitlesOff and restoreRememberedSubtitleSelectionOrAutoload.

#include "mpv_item.h"

#include <QFileInfo>

#include <algorithm>

namespace {

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
    const QString absolute_path = QFileInfo(file.toLocalFile()).absoluteFilePath();
    const QByteArray path = absolute_path.toUtf8();
    const char* args[] = {"sub-add", path.constData(), "select", nullptr};
    if (mpv_command(handle_, args) < 0) {
        return false;
    }
    last_subtitle_id_ = static_cast<int>(get_double("sid", -1.0));
    setSubtitlesVisible(true);
    tracks_dirty_ = true;
    set_subtitle_cue({}, -1.0, -1.0);
    if (remember) {
        remember_subtitle_selection(absolute_path);
    }
    return true;
}

void MpvItem::selectSubtitleTrack(int id) {
    select_subtitle_track(id, true);
}

void MpvItem::select_subtitle_track(int id, bool remember) {
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
    if (!video_settings_.value("videoRememberPlaybackPosition").toBool()) {
        return;
    }
    const auto saved = playback_history_.subtitle_selection(local_video_path_);
    if (!saved) {
        return;
    }
    if (const auto* path = std::get_if<QString>(&*saved)) {
        if (QFileInfo(*path).isFile() && !load_external_subtitle(QUrl::fromLocalFile(*path), false)) {
            emit subtitleLoadFailed();
        }
    } else if (const auto* embedded = std::get_if<niratan::player::SubtitleTrackIdentity>(&*saved)) {
        const auto tracks = subtitle_tracks(tracks_);
        QList<niratan::player::SubtitleTrackIdentity> identities;
        for (const auto& track : tracks) {
            if (!track.value(QStringLiteral("isExternal")).toBool()) {
                identities.append(identity(track));
            }
        }
        if (const auto id = niratan::player::matching_subtitle_track_id(*embedded, identities)) {
            const auto track = std::ranges::find(tracks, *id, track_id);
            if (track != tracks.end()) {
                select_subtitle_track_with_transcript(*track, false);
            }
        }
    } else {
        disable_subtitles(false);
    }
}
