// SPDX-License-Identifier: GPL-3.0-or-later

#include "playback_history.h"

#include "services/storage/app_roots.h"
#include "services/storage/json_file.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QTimeZone>

#include <algorithm>
#include <utility>

namespace niratan::player {
namespace {

constexpr double kMinimumPosition = 2;
constexpr double kFinishedMargin = 5;
constexpr double kResumableProgress = 0.98;
// Seconds between 1970-01-01 and Foundation's 2001-01-01 reference date.
constexpr double kReferenceOffset = 978'307'200;
// Date.distantPast, 0001-01-01 UTC.
constexpr qint64 kDistantPastSeconds = -62'135'596'800;

const QString kPositions = QStringLiteral("positions");
const QString kStates = QStringLiteral("playbackStates");
const QString kSelections = QStringLiteral("subtitleSelections");
const QString kSubtitlePaths = QStringLiteral("externalSubtitlePaths");

QDateTime date_from_reference(double seconds) {
    return QDateTime::fromMSecsSinceEpoch(qint64((seconds + kReferenceOffset) * 1000),
                                          QTimeZone::UTC);
}

double reference_from_date(const QDateTime& date) {
    return double(date.toMSecsSinceEpoch()) / 1000 - kReferenceOffset;
}

bool valid_state(const QJsonValue& value) {
    if (!value.isObject()) {
        return false;
    }
    const auto state = value.toObject();
    const auto optional_is = [&state](const char* key, bool (QJsonValue::*test)() const) {
        const auto field = state.value(QLatin1String(key));
        return field.isUndefined() || field.isNull() || (field.*test)();
    };
    return state.value("position").isDouble() && state.value("updatedAt").isDouble() &&
           optional_is("duration", &QJsonValue::isDouble) &&
           optional_is("isFinished", &QJsonValue::isBool) &&
           optional_is("resumeOptions", &QJsonValue::isObject) &&
           decode_resume_options(state.value("resumeOptions").toObject()).has_value();
}

// Whether the document decodes as a Swift Snapshot; one bad member fails the
// whole decode there.
bool valid_snapshot(const QJsonObject& document) {
    for (const auto& member : {kPositions, kStates, kSelections, kSubtitlePaths}) {
        if (document.contains(member) && !document.value(member).isObject()) {
            return false;
        }
    }
    const auto positions = document.value(kPositions).toObject();
    if (!std::ranges::all_of(positions, [](const QJsonValue& value) { return value.isDouble(); })) {
        return false;
    }
    const auto states = document.value(kStates).toObject();
    return std::ranges::all_of(states, valid_state);
}

PlaybackState decode_state(const QJsonObject& object) {
    PlaybackState state;
    state.position = object.value("position").toDouble();
    if (object.value("duration").isDouble()) {
        state.duration = object.value("duration").toDouble();
    }
    state.updated_at = date_from_reference(object.value("updatedAt").toDouble());
    state.finished = object.value("isFinished").toBool(false);
    state.resume_options = *decode_resume_options(object.value("resumeOptions").toObject());
    return state;
}

std::optional<PlaybackState> state_for(const QString& key, const QJsonObject& states,
                                       const QJsonObject& positions) {
    if (const auto entry = states.constFind(key); entry != states.constEnd()) {
        return decode_state(entry.value().toObject());
    }
    if (const auto position = positions.constFind(key); position != positions.constEnd()) {
        PlaybackState legacy;
        legacy.position = position.value().toDouble();
        legacy.updated_at = QDateTime::fromSecsSinceEpoch(kDistantPastSeconds, QTimeZone::UTC);
        return legacy;
    }
    return std::nullopt;
}

QJsonObject encode_state(const PlaybackState& state) {
    QJsonObject object{{"position", state.position},
                       {"updatedAt", reference_from_date(state.updated_at)},
                       {"isFinished", state.finished},
                       {"resumeOptions", encode_resume_options(state.resume_options)}};
    if (state.duration) {
        object.insert("duration", *state.duration);
    }
    return object;
}

QJsonObject legacy_positions(const std::filesystem::path& legacy_file) {
    QJsonObject positions;
    if (legacy_file.empty()) {
        return positions;
    }
    const auto legacy = storage::read_json_file(legacy_file);
    if (legacy.status != storage::JsonReadStatus::success) {
        return positions;
    }
    const auto entries = legacy.document.object();
    for (auto entry = entries.begin(); entry != entries.end(); ++entry) {
        const auto position = entry.value().toObject().value("position");
        if (position.isDouble()) {
            positions.insert(entry.key(), position.toDouble());
        }
    }
    return positions;
}

void put_state(QJsonObject& document, const QString& key, const PlaybackState& state) {
    auto states = document.value(kStates).toObject();
    states.insert(key, encode_state(state));
    document.insert(kStates, states);
}

void remove_member_entry(QJsonObject& document, const QString& member, const QString& key) {
    auto entries = document.value(member).toObject();
    if (!entries.contains(key)) {
        return;
    }
    entries.remove(key);
    document.insert(member, entries);
}

}  // namespace

std::optional<double> PlaybackState::progress() const {
    if (finished) {
        return 1.0;
    }
    if (duration && *duration > 0) {
        return std::clamp(position / *duration, 0.0, 1.0);
    }
    return std::nullopt;
}

bool PlaybackState::resumable() const {
    if (finished || position < kMinimumPosition) {
        return false;
    }
    const auto fraction = progress();
    return !fraction || *fraction < kResumableProgress;
}

PlaybackHistory PlaybackHistory::at_data_root() {
    const auto root = storage::data_root();
    if (root.empty()) {
        return PlaybackHistory({});
    }
    return PlaybackHistory(root / "video_playback_history.json", root / "video_playback.json");
}

PlaybackHistory::PlaybackHistory(std::filesystem::path file, std::filesystem::path legacy_file)
    : file_(std::move(file)), legacy_file_(std::move(legacy_file)) {}

std::optional<PlaybackState> PlaybackHistory::state(const QString& key) const {
    const auto document = read();
    return state_for(key, document.value(kStates).toObject(),
                     document.value(kPositions).toObject());
}

QHash<QString, PlaybackState> PlaybackHistory::states_for(const QSet<QString>& keys) const {
    if (keys.isEmpty()) {
        return {};
    }
    const auto document = read();
    const auto states = document.value(kStates).toObject();
    const auto positions = document.value(kPositions).toObject();
    QHash<QString, PlaybackState> result;
    for (const auto& key : keys) {
        if (const auto state = state_for(key, states, positions)) {
            result.insert(key, *state);
        }
    }
    return result;
}

std::optional<double> PlaybackHistory::resume_position(const QString& key) const {
    const auto document = read();
    const auto states = document.value(kStates).toObject();
    if (states.contains(key)) {
        const auto state = decode_state(states.value(key).toObject());
        return state.resumable() ? std::optional(state.position) : std::nullopt;
    }
    const auto positions = document.value(kPositions).toObject();
    if (positions.contains(key)) {
        return positions.value(key).toDouble();
    }
    return std::nullopt;
}

PlaybackOpenState PlaybackHistory::prepare_open(const QString& key, bool remember,
                                                bool from_beginning) {
    if (from_beginning) {
        if (!key.isEmpty()) {
            clear(key);
        }
        return {0.0, {}};
    }
    if (remember && !key.isEmpty()) {
        if (const auto stored = state(key)) {
            return {stored->resumable() ? std::optional(stored->position) : std::nullopt,
                    normalize_resume_options(stored->resume_options)};
        }
    }
    return {};
}

bool PlaybackHistory::save(const QString& key, double position, double duration,
                           const QDateTime& now) {
    return save(key, position, duration, ResumeOptions{}, now);
}

bool PlaybackHistory::save(const QString& key, double position, double duration,
                           const ResumeOptions& options, const QDateTime& now) {
    const auto before = read();
    auto document = before;
    if (duration <= 0 || position < kMinimumPosition) {
        remove_member_entry(document, kStates, key);
        remove_member_entry(document, kPositions, key);
    } else if (position >= duration - kFinishedMargin) {
        put_state(document, key, {duration, duration, now, true, {}});
        remove_member_entry(document, kPositions, key);
    } else {
        put_state(document, key, {position, duration, now, false, normalize_resume_options(options)});
        auto positions = document.value(kPositions).toObject();
        positions.insert(key, position);
        document.insert(kPositions, positions);
    }
    return document != before && write(document);
}

bool PlaybackHistory::mark_watched(const QString& key, std::optional<double> duration,
                                   const QDateTime& now) {
    const auto before = read();
    auto document = before;
    put_state(document, key, {duration.value_or(0), duration, now, true, {}});
    remove_member_entry(document, kPositions, key);
    return document != before && write(document);
}

bool PlaybackHistory::clear(const QString& key) {
    const auto before = read();
    auto document = before;
    remove_member_entry(document, kStates, key);
    remove_member_entry(document, kPositions, key);
    return document != before && write(document);
}

std::optional<int> matching_subtitle_track_id(const SubtitleTrackIdentity& identity,
                                             const QList<SubtitleTrackIdentity>& tracks) {
    const auto exact = std::ranges::find_if(tracks, [&identity](const auto& track) {
        return identity.stream_index ? track.stream_index == identity.stream_index
                                     : track.id == identity.id;
    });
    if (exact != tracks.end()) {
        return exact->id;
    }
    std::optional<int> match;
    for (const auto& track : tracks) {
        if (track.title == identity.title && track.language == identity.language &&
            track.codec == identity.codec) {
            if (match) {
                return std::nullopt;
            }
            match = track.id;
        }
    }
    return match;
}

std::optional<SubtitleSelection> PlaybackHistory::subtitle_selection(const QString& key) const {
    if (key.isEmpty()) {
        return std::nullopt;
    }
    const auto entry = read().value(kSelections).toObject().value(key).toObject();
    if (entry.value("off").isObject() || entry.value("externalDisabled").isObject()) {
        return SubtitleSelection{};
    }
    const auto path = entry.value("external").toObject().value("path").toString();
    if (!path.isEmpty()) {
        return path;
    }
    const auto track = entry.value("embedded").toObject().value("_0").toObject();
    const int id = track.value("trackID").toInt();
    if (id > 0 && track.value("title").isString()) {
        const int index = track.value("ffIndex").toInt(-1);
        return SubtitleTrackIdentity{id, index >= 0 ? std::optional(index) : std::nullopt,
                                     track.value("title").toString(),
                                     track.value("language").toString(),
                                     track.value("codec").toString()};
    }
    return std::nullopt;
}

bool PlaybackHistory::save_subtitle_selection(const QString& key, const SubtitleSelection& selection) {
    if (key.isEmpty()) {
        return false;
    }
    const auto before = read();
    auto document = before;
    QJsonObject entry;
    if (const auto* track = std::get_if<SubtitleTrackIdentity>(&selection)) {
        QJsonObject fields{{"trackID", track->id}, {"title", track->title}};
        if (track->stream_index) {
            fields.insert("ffIndex", *track->stream_index);
        }
        if (!track->language.isNull()) {
            fields.insert("language", track->language);
        }
        if (!track->codec.isNull()) {
            fields.insert("codec", track->codec);
        }
        entry.insert("embedded", QJsonObject{{"_0", fields}});
    } else if (const auto* path = std::get_if<QString>(&selection)) {
        entry.insert("external", QJsonObject{{"path", *path}});
        auto paths = document.value(kSubtitlePaths).toObject();
        paths.insert(key, *path);
        document.insert(kSubtitlePaths, paths);
    } else {
        entry.insert("off", QJsonObject{});
    }
    auto selections = document.value(kSelections).toObject();
    selections.insert(key, entry);
    document.insert(kSelections, selections);
    return document != before && write(document);
}

QJsonObject PlaybackHistory::read() const {
    if (file_.empty()) {
        return {};
    }
    const auto result = storage::read_json_file(file_);
    if (result.status == storage::JsonReadStatus::missing) {
        const auto positions = legacy_positions(legacy_file_);
        return positions.isEmpty() ? QJsonObject{} : QJsonObject{{kPositions, positions}};
    }
    if (result.status != storage::JsonReadStatus::success || !result.document.isObject()) {
        return {};
    }
    const auto document = result.document.object();
    return valid_snapshot(document) ? document : QJsonObject{};
}

bool PlaybackHistory::write(const QJsonObject& document) const {
    if (file_.empty()) {
        return false;
    }
    auto snapshot = document;
    for (const auto& member : {kPositions, kStates, kSelections, kSubtitlePaths}) {
        if (!snapshot.contains(member)) {
            snapshot.insert(member, QJsonObject{});
        }
    }
    return storage::write_json_file(file_, QJsonDocument(snapshot));
}

}  // namespace niratan::player
