// SPDX-License-Identifier: GPL-3.0-or-later

#include "subtitle_default.h"

#include "services/player/subtitle_document.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <array>

namespace niratan::player {
namespace {

const std::array kSidecarExtensions{QStringLiteral("srt"), QStringLiteral("vtt"),
                                    QStringLiteral("ass"), QStringLiteral("ssa")};

QUrl standardized_local_url(const QString& path) {
    return QUrl::fromLocalFile(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
}

int extension_priority(const QString& extension) {
    const QString lower = extension.toLower();
    const auto found = std::ranges::find(kSidecarExtensions, lower);
    return found == kSidecarExtensions.end()
        ? int(kSidecarExtensions.size())
        : int(std::distance(kSidecarExtensions.begin(), found));
}

bool language_is_japanese(const std::optional<QString>& language) {
    if (!language) {
        return false;
    }
    const QString lower = language->toLower();
    const std::array languages{QStringLiteral("ja"), QStringLiteral("jp"), QStringLiteral("jpn"),
                               QStringLiteral("japanese")};
    return std::ranges::any_of(languages,
                               [&lower](const QString& value) { return lower == value; }) ||
        lower.startsWith(QStringLiteral("ja-"));
}

int subtitle_rank(const VideoTrack& track) {
    const QString title = track.title.toLower();
    const std::array partial_titles{QStringLiteral("sign"), QStringLiteral("song"),
                                    QStringLiteral("forced"), QStringLiteral("看板"),
                                    QStringLiteral("歌詞")};
    const bool partial = std::ranges::any_of(
        partial_titles, [&title](const QString& value) { return title.contains(value); });
    return (partial ? 2 : 0) + (track.is_image ? 1 : 0);
}

QString cleaned_path(const QString& path) {
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(QFileInfo(path).absoluteFilePath()) : canonical;
}

SubtitleTrackIdentity identity_of(const VideoTrack& track) {
    const QString title = track.title.isEmpty() && track.type == VideoTrackType::subtitle
        ? QStringLiteral("Sub %1").arg(track.id)
        : track.title;
    return {track.id, track.ff_index, title, track.language.value_or(QString{}),
            track.codec.value_or(QString{})};
}

QList<SubtitleTrackIdentity> embedded_identities(const QList<VideoTrack>& tracks) {
    QList<SubtitleTrackIdentity> identities;
    for (const auto& track : tracks) {
        if (track.type == VideoTrackType::subtitle) {
            identities.append(identity_of(track));
        }
    }
    return identities;
}

}  // namespace

std::optional<VideoTrack> preferred_embedded_subtitle_track(const QList<VideoTrack>& tracks) {
    const auto eligible = [](const VideoTrack& track) {
        return track.type == VideoTrackType::subtitle && !track.external_filename.has_value() &&
            is_japanese_subtitle_track(track);
    };
    const auto best =
        std::ranges::find_if(tracks, [&tracks, &eligible](const VideoTrack& candidate) {
            if (!eligible(candidate)) {
                return false;
            }
            return std::ranges::none_of(tracks, [&candidate, &eligible](const VideoTrack& other) {
                return eligible(other) && subtitle_rank(other) < subtitle_rank(candidate);
            });
        });
    if (best == tracks.end()) {
        return std::nullopt;
    }
    return *best;
}

bool is_japanese_subtitle_track(const VideoTrack& track) {
    if (language_is_japanese(track.language)) {
        return true;
    }
    const QString title = track.title.toLower();
    return title.contains(QStringLiteral("日本語")) || title.contains(QStringLiteral("japanese"));
}

std::optional<QUrl> best_subtitle_sidecar(const QUrl& media_url) {
    if (!media_url.isLocalFile() || media_url.toLocalFile().isEmpty()) {
        return std::nullopt;
    }
    const QFileInfo media(media_url.toLocalFile());
    const QDir directory(media.absolutePath());
    const QString stem = media.completeBaseName();
    for (const auto& extension : kSidecarExtensions) {
        const QString exact = directory.filePath(stem + QLatin1Char('.') + extension);
        if (QFileInfo::exists(exact)) {
            return standardized_local_url(exact);
        }
    }
    if (!directory.exists()) {
        return std::nullopt;
    }

    QList<QFileInfo> candidates;
    const auto entries =
        directory.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
    for (const auto& entry : entries) {
        if (extension_priority(entry.suffix()) >= int(kSidecarExtensions.size())) {
            continue;
        }
        const QString subtitle_stem = entry.completeBaseName();
        if (subtitle_stem.startsWith(stem + QLatin1Char('.')) ||
            subtitle_stem.startsWith(stem + QLatin1Char(' '))) {
            candidates.append(entry);
        }
    }
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::ranges::stable_sort(candidates,
                             [&collator](const QFileInfo& left, const QFileInfo& right) {
                                 const int left_priority = extension_priority(left.suffix());
                                 const int right_priority = extension_priority(right.suffix());
                                 if (left_priority != right_priority) {
                                     return left_priority < right_priority;
                                 }
                                 return collator.compare(left.fileName(), right.fileName()) < 0;
                             });
    if (candidates.isEmpty()) {
        return std::nullopt;
    }
    return standardized_local_url(candidates.first().filePath());
}

SubtitleFirstOpenPlan plan_subtitle_first_open(const QUrl& media_url) {
    SubtitleFirstOpenPlan plan;
    plan.sidecar = best_subtitle_sidecar(media_url);
    plan.wait_for_embedded_default = !plan.sidecar.has_value();
    return plan;
}

std::optional<QUrl> primary_subtitle_selection_path(const QUrl& file, const QByteArray& bytes) {
    if (!file.isLocalFile() || file.toLocalFile().isEmpty()) {
        return std::nullopt;
    }
    const auto parsed = parse_subtitle_document(bytes, file);
    if (!parsed.document) {
        return std::nullopt;
    }
    return standardized_local_url(file.toLocalFile());
}

SubtitleRestoreResolution resolve_subtitle_restore(const SubtitleSelection& selection,
                                                   const QList<VideoTrack>& tracks,
                                                   bool is_loaded) {
    if (std::holds_alternative<std::monostate>(selection)) {
        return {SubtitleRestoreKind::off, {}, 0};
    }
    if (const auto* path = std::get_if<QString>(&selection)) {
        if (QFileInfo::exists(*path)) {
            return {SubtitleRestoreKind::external, standardized_local_url(*path), 0};
        }
        return {SubtitleRestoreKind::unavailable, {}, 0};
    }

    const auto& saved = std::get<SubtitleTrackIdentity>(selection);
    if (!is_loaded) {
        return {SubtitleRestoreKind::waiting_for_tracks, {}, 0};
    }
    if (const auto id = matching_subtitle_track_id(saved, embedded_identities(tracks))) {
        return {SubtitleRestoreKind::embedded_track, {}, *id};
    }
    return {!tracks.isEmpty() ? SubtitleRestoreKind::unavailable
                              : SubtitleRestoreKind::waiting_for_tracks,
            {},
            0};
}

void EmbeddedSubtitleDefault::wait_for_tracks() {
    awaiting_ = true;
}

void EmbeddedSubtitleDefault::invalidate() {
    awaiting_ = false;
}

std::optional<EmbeddedSubtitleActivation>
EmbeddedSubtitleDefault::apply_if_ready(bool is_loaded, const QList<VideoTrack>& tracks,
                                        bool has_primary_document) {
    if (!awaiting_ || !is_loaded) {
        return std::nullopt;
    }
    awaiting_ = false;
    if (has_primary_document || std::ranges::any_of(tracks, [](const VideoTrack& track) {
            return track.type == VideoTrackType::subtitle && track.is_selected;
        })) {
        return std::nullopt;
    }
    const auto preferred = preferred_embedded_subtitle_track(tracks);
    if (!preferred) {
        return std::nullopt;
    }
    return EmbeddedSubtitleActivation{preferred->id, false, false, true};
}

std::optional<int> existing_external_subtitle_track_id(const QUrl& sidecar,
                                                       const QList<VideoTrack>& tracks) {
    if (!sidecar.isLocalFile() || sidecar.toLocalFile().isEmpty()) {
        return std::nullopt;
    }
    const QString target = cleaned_path(sidecar.toLocalFile());
    for (const auto& track : tracks) {
        if (track.type != VideoTrackType::subtitle || !track.external_filename ||
            track.external_filename->isEmpty() ||
            track.title == QStringLiteral("__niratan_internal_ass_effects__")) {
            continue;
        }
        if (track.id > 0 && cleaned_path(*track.external_filename) == target) {
            return track.id;
        }
    }
    return std::nullopt;
}

}  // namespace niratan::player
