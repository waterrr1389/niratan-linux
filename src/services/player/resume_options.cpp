// SPDX-License-Identifier: GPL-3.0-or-later
// VideoPlaybackHistoryStore.swift: VideoPlaybackResumeOptions and VideoAudioTrackIdentity.

#include "services/player/resume_options.h"

#include <algorithm>
#include <cmath>

namespace niratan::player {
namespace {

double resume_speed(double speed) {
    if (!std::isfinite(speed)) {
        return 1.0;
    }
    if (speed <= 0.25) {
        return 0.25;
    }
    return std::clamp(std::round(speed / 0.1) * 0.1, 0.3, 5.0);
}

bool is_integer(const QJsonValue& value) {
    return value.isDouble() && value.toInteger(0) == value.toInteger(1);
}

bool absent(const QJsonValue& value) {
    return value.isUndefined() || value.isNull();
}

std::optional<ResumeAudioSelection> decode_audio_selection(const QJsonObject& object) {
    const auto off = object.value("off");
    const auto embedded = object.value("embedded");
    if (!off.isUndefined()) {
        return off.isObject() && embedded.isUndefined()
            ? std::optional<ResumeAudioSelection>(AudioOff{}) : std::nullopt;
    }
    const auto payload = embedded.toObject().value("_0");
    const auto fields = payload.toObject();
    const auto id = fields.value("trackID");
    const auto index = fields.value("ffIndex");
    const auto title = fields.value("title");
    const auto language = fields.value("language");
    const auto codec = fields.value("codec");
    if (!is_integer(id) || !title.isString() || (!absent(index) && !is_integer(index)) ||
        (!absent(language) && !language.isString()) || (!absent(codec) && !codec.isString())) {
        return std::nullopt;
    }
    return AudioTrackIdentity{id.toInteger(),
                              absent(index) ? std::nullopt : std::optional(index.toInteger()),
                              title.toString(),
                              absent(language) ? std::nullopt : std::optional(language.toString()),
                              absent(codec) ? std::nullopt : std::optional(codec.toString())};
}

QJsonObject encode_audio_selection(const ResumeAudioSelection& selection) {
    const auto* identity = std::get_if<AudioTrackIdentity>(&selection);
    if (!identity) {
        return {{"off", QJsonObject{}}};
    }
    QJsonObject fields{{"trackID", identity->track_id}, {"title", identity->title}};
    if (identity->ff_index) {
        fields.insert("ffIndex", *identity->ff_index);
    }
    if (identity->language) {
        fields.insert("language", *identity->language);
    }
    if (identity->codec) {
        fields.insert("codec", *identity->codec);
    }
    return {{"embedded", QJsonObject{{"_0", fields}}}};
}

}  // namespace

bool ResumeOptions::empty() const {
    return !speed && !subtitle_delay && !audio_delay && !audio_selection;
}

ResumeOptions capture_resume_options(double speed, double subtitle_delay, double audio_delay,
                                     const QList<ResumeAudioTrack>& tracks) {
    auto options = normalize_resume_options({speed, subtitle_delay, audio_delay, {}});
    if (std::abs(*options.speed - 1.0) < 0.001) {
        options.speed.reset();
    }
    if (!(std::abs(*options.subtitle_delay) >= 0.005)) {
        options.subtitle_delay.reset();
    }
    if (!(std::abs(*options.audio_delay) >= 0.005)) {
        options.audio_delay.reset();
    }
    if (!tracks.isEmpty()) {
        const auto selected = std::ranges::find_if(tracks, &ResumeAudioTrack::selected);
        options.audio_selection = selected == tracks.end()
            ? ResumeAudioSelection(AudioOff{}) : ResumeAudioSelection(selected->identity);
    }
    return options;
}

ResumeOptions normalize_resume_options(ResumeOptions options) {
    options.speed = options.speed.transform(resume_speed);
    options.subtitle_delay = options.subtitle_delay.transform([](double delay) {
        return std::clamp(delay, -60.0, 60.0);
    });
    options.audio_delay = options.audio_delay.transform([](double delay) {
        return std::clamp(delay, -30.0, 30.0);
    });
    return options;
}

std::optional<qint64> matching_audio_track_id(const AudioTrackIdentity& identity,
                                             const QList<ResumeAudioTrack>& tracks) {
    const auto match = std::ranges::find_if(tracks, [&identity](const ResumeAudioTrack& track) {
        return identity.ff_index ? track.identity.ff_index == identity.ff_index
                                 : track.identity.track_id == identity.track_id;
    });
    if (match != tracks.end()) {
        return match->identity.track_id;
    }
    std::optional<qint64> metadata_match;
    for (const auto& [candidate, selected] : tracks) {
        if (candidate.title == identity.title && candidate.language == identity.language &&
            candidate.codec == identity.codec) {
            if (metadata_match) {
                return std::nullopt;
            }
            metadata_match = candidate.track_id;
        }
    }
    return metadata_match;
}

QJsonObject encode_resume_options(const ResumeOptions& options) {
    QJsonObject object;
    for (const auto& [key, value] : {std::pair{"speed", options.speed},
                                     std::pair{"subtitleDelay", options.subtitle_delay},
                                     std::pair{"audioDelay", options.audio_delay}}) {
        if (value) {
            object.insert(QLatin1String(key), *value);
        }
    }
    if (options.audio_selection) {
        object.insert("audioSelection", encode_audio_selection(*options.audio_selection));
    }
    return object;
}

std::optional<ResumeOptions> decode_resume_options(const QJsonObject& object) {
    ResumeOptions options;
    for (const auto& [key, field] : {std::pair{"speed", &ResumeOptions::speed},
                                     std::pair{"subtitleDelay", &ResumeOptions::subtitle_delay},
                                     std::pair{"audioDelay", &ResumeOptions::audio_delay}}) {
        const auto value = object.value(QLatin1String(key));
        if (absent(value)) {
            continue;
        }
        if (!value.isDouble()) {
            return std::nullopt;
        }
        options.*field = value.toDouble();
    }
    const auto selection = object.value("audioSelection");
    if (!absent(selection)) {
        options.audio_selection = decode_audio_selection(selection.toObject());
        if (!options.audio_selection) {
            return std::nullopt;
        }
    }
    return options;
}

}  // namespace niratan::player
