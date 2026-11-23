// SPDX-License-Identifier: GPL-3.0-or-later

#include "viewmodels/player/player_inspector_view_model.h"

#include "services/video/video_playlist.h"

#include <QFileInfo>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace niratan::viewmodels {
namespace {

constexpr int kSubtitleLimitMilliseconds = 60'000;
constexpr int kSubtitleSliderLimitMilliseconds = 10'000;
constexpr double kAudioLimitSeconds = 30.0;
constexpr double kSpeedMinimum = 0.25;
constexpr double kSpeedMaximum = 5.0;
constexpr double kSpeedCustomLowerBound = 0.3;
constexpr double kSpeedCustomStep = 0.1;
constexpr double kSpeedSelectionTolerance = 0.001;

const std::array<double, 8>& preset_speeds() {
    static const std::array<double, 8> presets{0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0};
    return presets;
}

// VideoPlaybackSpeed.presetChoices split into the inspector's two rows.
const QVariantList& speed_choice_rows() {
    static const QVariantList rows{QVariantList{0.25, 0.5, 1.0, 1.5},
                                   QVariantList{2.0, 3.0, 4.0, 5.0}};
    return rows;
}

// VideoPlaybackSpeed.normalized: non-finite falls back to 1, speeds at or
// below the minimum clamp to it, otherwise round to the 0.1 step and clamp to
// the custom input range.
double normalized_speed(double speed) {
    if (!std::isfinite(speed)) {
        return 1.0;
    }
    if (!(speed > kSpeedMinimum)) {
        return kSpeedMinimum;
    }
    const double rounded = std::round(speed / kSpeedCustomStep) * kSpeedCustomStep;
    return std::min(std::max(rounded, kSpeedCustomLowerBound), kSpeedMaximum);
}

// VideoPlaybackSpeed.label: 0.25 keeps its decimals, whole numbers print
// without one, everything else prints one decimal.
QString speed_label_text(double speed, bool includesSuffix) {
    const double value = normalized_speed(speed);
    QString formatted;
    if (std::abs(value - kSpeedMinimum) < kSpeedSelectionTolerance) {
        formatted = QStringLiteral("0.25");
    } else if (std::abs(value - std::round(value)) < kSpeedSelectionTolerance) {
        formatted = QString::number(std::round(value), 'f', 0);
    } else {
        formatted = QString::number(value, 'f', 1);
    }
    return includesSuffix ? formatted + QLatin1Char('x') : formatted;
}

// Swift's Int(String): an optional sign and ASCII digits, nothing else, and
// no overflow. The caller has already trimmed and replaced the comma.
std::optional<int> parse_whole_number(QString text) {
    text = text.trimmed();
    text.replace(u',', u'.');
    qsizetype index = 0;
    if (!text.isEmpty() && (text.at(0) == u'+' || text.at(0) == u'-')) {
        index = 1;
    }
    if (index >= text.size()) {
        return std::nullopt;
    }
    for (qsizetype i = index; i < text.size(); ++i) {
        const QChar character = text.at(i);
        if (character < u'0' || character > u'9') {
            return std::nullopt;
        }
    }
    bool ok = false;
    const long long value = text.toLongLong(&ok);
    if (!ok || value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

// Swift's Double(String) over the trimmed text, with the comma already
// replaced.
std::optional<double> parse_speed_text(QString text) {
    text = text.trimmed();
    text.replace(u',', u'.');
    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return value;
}

QString track_title(const QVariantMap& track) {
    const QString title = track.value(QStringLiteral("title")).toString();
    const QString language = track.value(QStringLiteral("language")).toString();
    return language.isEmpty() ? title : title + QStringLiteral(" · ") + language;
}

QVariantMap track_row(int id, const QString& title, const QString& detail, bool selected) {
    return QVariantMap{
        {QStringLiteral("id"), id},
        {QStringLiteral("title"), title},
        {QStringLiteral("detail"), detail},
        {QStringLiteral("selected"), selected},
    };
}

QString media_stem(const QUrl& url) {
    if (url.isLocalFile()) {
        return QFileInfo(url.toLocalFile()).completeBaseName();
    }
    return QFileInfo(url.fileName()).completeBaseName();
}

double clamp_audio_delay(double seconds) {
    return std::min(std::max(seconds, -kAudioLimitSeconds), kAudioLimitSeconds);
}

}  // namespace

PlayerInspectorViewModel::PlayerInspectorViewModel(QObject* parent) : QObject(parent) {}

PlayerInspectorViewModel::~PlayerInspectorViewModel() = default;

QString PlayerInspectorViewModel::display_title() const {
    if (!current_title_.isEmpty()) {
        return current_title_;
    }
    if (!current_url_.isEmpty()) {
        return media_stem(current_url_);
    }
    return tr("Inspector");
}

QString PlayerInspectorViewModel::header_caption() const {
    const QString nowPlaying = tr("Now Playing");
    if (episode_urls_.size() > 1 && current_index_ >= 0) {
        return nowPlaying +
            QStringLiteral(" · %1 / %2").arg(current_index_ + 1).arg(episode_urls_.size());
    }
    return nowPlaying;
}

QVariantList PlayerInspectorViewModel::episodes() const {
    QVariantList rows;
    rows.reserve(episode_urls_.size());
    for (qsizetype index = 0; index < episode_urls_.size(); ++index) {
        const QUrl& url = episode_urls_.at(index);
        rows.append(QVariantMap{
            {QStringLiteral("number"), static_cast<int>(index) + 1},
            {QStringLiteral("title"), media_stem(url)},
            {QStringLiteral("current"), static_cast<int>(index) == current_index_},
            {QStringLiteral("url"), url},
        });
    }
    return rows;
}

QVariantMap PlayerInspectorViewModel::current_episode() const {
    if (current_index_ < 0 || current_index_ >= episode_urls_.size()) {
        return {};
    }
    return episodes().value(current_index_).toMap();
}

QVariantList PlayerInspectorViewModel::video_tracks() const {
    return video_tracks_;
}

QVariantList PlayerInspectorViewModel::audio_tracks() const {
    return audio_tracks_;
}

QVariantList PlayerInspectorViewModel::subtitle_tracks() const {
    return subtitle_tracks_;
}

int PlayerInspectorViewModel::subtitle_milliseconds() const {
    const long long rounded = std::llround(subtitle_delay_ * 1'000.0);
    return static_cast<int>(
        std::clamp<long long>(rounded, -kSubtitleLimitMilliseconds, kSubtitleLimitMilliseconds));
}

QString PlayerInspectorViewModel::subtitle_value_text() const {
    const int milliseconds = subtitle_milliseconds();
    return (milliseconds >= 0 ? QStringLiteral("+") : QString()) + QString::number(milliseconds) +
        QStringLiteral(" ms");
}

QString PlayerInspectorViewModel::subtitle_input_text() const {
    return subtitle_input_;
}

int PlayerInspectorViewModel::subtitle_slider_milliseconds() const {
    return std::clamp(subtitle_milliseconds(), -kSubtitleSliderLimitMilliseconds,
                      kSubtitleSliderLimitMilliseconds);
}

double PlayerInspectorViewModel::audio_delay() const {
    return audio_delay_;
}

QString PlayerInspectorViewModel::audio_value_text() const {
    return QString::asprintf("%+.1f s", audio_delay_);
}

bool PlayerInspectorViewModel::audio_neutral() const {
    return audio_delay_ == 0.0;
}

double PlayerInspectorViewModel::speed() const {
    return speed_;
}

QString PlayerInspectorViewModel::speed_label() const {
    return speed_label_text(speed_, true);
}

QString PlayerInspectorViewModel::speed_input_text() const {
    return speed_input_;
}

QVariantList PlayerInspectorViewModel::speed_rows() const {
    return speed_choice_rows();
}

void PlayerInspectorViewModel::setVideo(const QUrl& url, const QString& title) {
    current_url_ = url;
    current_title_ = title;
    rebuild_playlist();
    emit displayTitleChanged();
    emit headerCaptionChanged();
    emit episodesChanged();
}

void PlayerInspectorViewModel::setTracks(const QVariantList& tracks) {
    tracks_ = tracks;
    rebuild_tracks();
    emit tracksChanged();
}

void PlayerInspectorViewModel::setPlaybackState(double speed, double subtitleDelay,
                                                double audioDelay) {
    // SwiftUI's onChange only resynchronizes the inputs when the snapshot
    // value actually changes, so an in-progress edit survives a repeated
    // poll.
    if (speed_ != speed) {
        speed_ = speed;
        speed_input_ = speed_label_text(speed_, false);
        emit speedChanged();
        emit speedInputChanged();
    }
    if (subtitle_delay_ != subtitleDelay) {
        subtitle_delay_ = subtitleDelay;
        subtitle_input_ = QString::number(subtitle_milliseconds());
        emit subtitleChanged();
        emit subtitleInputChanged();
    }
    if (audio_delay_ != audioDelay) {
        audio_delay_ = audioDelay;
        emit audioChanged();
    }
}

void PlayerInspectorViewModel::selectEpisode(int index) {
    if (index < 0 || index >= episode_urls_.size()) {
        return;
    }
    emit episodeRequested(episode_urls_.at(index));
}

void PlayerInspectorViewModel::selectTrack(const QString& type, int id) {
    emit selectTrackRequested(type, id);
}

void PlayerInspectorViewModel::stepSubtitleMilliseconds(int delta) {
    const long long target = static_cast<long long>(subtitle_milliseconds()) + delta;
    apply_subtitle_milliseconds(static_cast<int>(
        std::clamp<long long>(target, -kSubtitleLimitMilliseconds, kSubtitleLimitMilliseconds)));
}

void PlayerInspectorViewModel::setSubtitleMilliseconds(int milliseconds) {
    apply_subtitle_milliseconds(milliseconds);
}

void PlayerInspectorViewModel::setSubtitleSliderMilliseconds(double milliseconds) {
    const long long rounded = std::llround(milliseconds);
    apply_subtitle_milliseconds(static_cast<int>(std::clamp<long long>(
        rounded, std::numeric_limits<int>::min(), std::numeric_limits<int>::max())));
}

void PlayerInspectorViewModel::setSubtitleInput(const QString& text) {
    subtitle_input_ = text;
    emit subtitleInputChanged();
}

void PlayerInspectorViewModel::commitSubtitleInput() {
    const std::optional<int> milliseconds = parse_whole_number(subtitle_input_);
    if (!milliseconds.has_value()) {
        subtitle_input_ = QString::number(subtitle_milliseconds());
        emit subtitleInputChanged();
        return;
    }
    apply_subtitle_milliseconds(*milliseconds);
}

void PlayerInspectorViewModel::stepAudioDelay(double delta) {
    setAudioDelay(audio_delay_ + delta);
}

void PlayerInspectorViewModel::setAudioDelay(double seconds) {
    audio_delay_ = clamp_audio_delay(seconds);
    emit audioChanged();
    emit audioDelayRequested(audio_delay_);
}

void PlayerInspectorViewModel::setSpeedInput(const QString& text) {
    speed_input_ = text;
    emit speedInputChanged();
}

void PlayerInspectorViewModel::commitSpeedInput() {
    const std::optional<double> requested = parse_speed_text(speed_input_);
    if (!requested.has_value()) {
        speed_input_ = speed_label_text(speed_, false);
        emit speedInputChanged();
        return;
    }
    apply_speed(*requested);
}

void PlayerInspectorViewModel::chooseSpeed(double speed) {
    apply_speed(speed);
}

bool PlayerInspectorViewModel::isSpeedSelected(double speed) const {
    const double current = normalized_speed(speed_);
    double selected = current;
    for (const double preset : preset_speeds()) {
        if (std::abs(preset - current) < kSpeedSelectionTolerance) {
            selected = preset;
            break;
        }
    }
    return std::abs(selected - normalized_speed(speed)) < kSpeedSelectionTolerance;
}

void PlayerInspectorViewModel::rebuild_playlist() {
    episode_urls_.clear();
    current_index_ = -1;
    if (current_url_.isEmpty() || !current_url_.isLocalFile()) {
        return;
    }
    const QString directoryPath = QFileInfo(current_url_.toLocalFile()).absolutePath();
    const video::VideoPlaylist playlist =
        video::make_video_playlist(video::playlist_directory_urls(directoryPath), current_url_);
    episode_urls_ = playlist.items;
    current_index_ = playlist.currentIndex;
}

void PlayerInspectorViewModel::rebuild_tracks() {
    video_tracks_.clear();
    QVariantList audioRows;
    QVariantList subtitleRows;
    bool audioSelected = false;
    bool subtitleSelected = false;

    for (const QVariant& entry : tracks_) {
        const QVariantMap track = entry.toMap();
        const QString type = track.value(QStringLiteral("type")).toString();
        const int id = track.value(QStringLiteral("id")).toInt();
        const QString codec = track.value(QStringLiteral("codec")).toString();
        const bool selected = track.value(QStringLiteral("isSelected")).toBool();
        if (type == QStringLiteral("video")) {
            video_tracks_.append(track_row(id, track_title(track), codec, selected));
        } else if (type == QStringLiteral("audio")) {
            audioSelected = audioSelected || selected;
            audioRows.append(track_row(id, track_title(track), codec, selected));
        } else if (type == QStringLiteral("sub")) {
            subtitleSelected = subtitleSelected || selected;
            QString detail = codec;
            if (!track.value(QStringLiteral("externalFilename")).toString().isEmpty()) {
                detail = codec.isEmpty() ? tr("External Subtitles")
                                         : tr("External Subtitles") + QStringLiteral(" · ") + codec;
            }
            subtitleRows.append(track_row(id, track_title(track), detail, selected));
        }
    }

    audio_tracks_ = QVariantList{track_row(0, tr("Off"), QString(), !audioSelected)};
    audio_tracks_ += audioRows;

    subtitle_tracks_ = QVariantList{track_row(0, tr("Off"), QString(), !subtitleSelected)};
    subtitle_tracks_ += subtitleRows;
}

void PlayerInspectorViewModel::apply_speed(double requested) {
    const double normalized = normalized_speed(requested);
    speed_ = normalized;
    speed_input_ = speed_label_text(normalized, false);
    emit speedChanged();
    emit speedInputChanged();
    emit speedRequested(normalized);
}

void PlayerInspectorViewModel::apply_subtitle_milliseconds(int milliseconds) {
    const int clamped =
        std::clamp(milliseconds, -kSubtitleLimitMilliseconds, kSubtitleLimitMilliseconds);
    subtitle_delay_ = static_cast<double>(clamped) / 1'000.0;
    subtitle_input_ = QString::number(clamped);
    emit subtitleChanged();
    emit subtitleInputChanged();
    emit subtitleDelayRequested(subtitle_delay_);
}

}  // namespace niratan::viewmodels
