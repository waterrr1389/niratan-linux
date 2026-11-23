// SPDX-License-Identifier: GPL-3.0-or-later
//
// State for the video Inspector side panel, following
// Features/Video/VideoInspectorView.swift (`displayTitle`,
// `currentEpisodeIndex`, `headerCaption`, `episodesTab`, `trackSection`,
// `subtitleSourcesSection`, `subtitleTrackDetail`, `subtitleTimingSection`,
// `subtitleTimingMilliseconds`, `subtitleTimingValueText`,
// `applySubtitleTimingMilliseconds`, `commitSubtitleTimingInput`,
// `audioTab`, `speedRows`, `isSpeedSelected`, `setSpeed`,
// `commitSpeedInput`), VideoInspectorState.swift (`speed`, `subtitleDelay`,
// `audioDelay`, `tracks`), VideoPlaylist.swift, VideoTrack.swift
// (`displayName`), PlaybackEngine.swift (`VideoSubtitleTiming`,
// `VideoPlaybackSpeed`) and VideoPlayerViewModel.swift (`playlistURLs`,
// `setSubtitleDelay`, `setAudioDelay`, `setSpeed`, `selectTrack`).
//
// The model holds a copy of what the page shows; playback state arrives
// through `setPlaybackState` and tracks through `setTracks`, both taken from
// the mpv snapshot. Commands are signals the page forwards to the player, so
// the model never calls mpv, opens a file or writes the playback history.
// `selectTrack` emits mpv's track type spelling and id, with 0 for Off;
// audio and subtitle ids are the ones the history stores, never renumbered.
//
// Display strings match Swift exactly where they show a stored or computed
// value: the episode names, the `Now Playing · n / total` caption, the
// `+50 ms` subtitle timing text, `%+.1f s` audio timing and the `0.3x` speed
// label. Timing values round half away from zero and clamp; subtitle offsets
// clamp to +-60000 ms (slider to +-10000), audio delay to +-30 s. Text input
// is trimmed, `,` becomes `.`, and a value that does not parse restores the
// field without changing anything.
//
// The tab strip and its remembered selection are view state, not here.

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace niratan::viewmodels {

class PlayerInspectorViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString displayTitle READ display_title NOTIFY displayTitleChanged)
    Q_PROPERTY(QString headerCaption READ header_caption NOTIFY headerCaptionChanged)
    Q_PROPERTY(QVariantList episodes READ episodes NOTIFY episodesChanged)
    Q_PROPERTY(QVariantMap currentEpisode READ current_episode NOTIFY episodesChanged)
    Q_PROPERTY(QVariantList videoTracks READ video_tracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList audioTracks READ audio_tracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitle_tracks NOTIFY tracksChanged)
    Q_PROPERTY(int subtitleMilliseconds READ subtitle_milliseconds NOTIFY subtitleChanged)
    Q_PROPERTY(QString subtitleValueText READ subtitle_value_text NOTIFY subtitleChanged)
    Q_PROPERTY(QString subtitleInputText READ subtitle_input_text NOTIFY subtitleInputChanged)
    Q_PROPERTY(
        int subtitleSliderMilliseconds READ subtitle_slider_milliseconds NOTIFY subtitleChanged)
    Q_PROPERTY(double audioDelay READ audio_delay NOTIFY audioChanged)
    Q_PROPERTY(QString audioValueText READ audio_value_text NOTIFY audioChanged)
    Q_PROPERTY(bool audioNeutral READ audio_neutral NOTIFY audioChanged)
    Q_PROPERTY(double speed READ speed NOTIFY speedChanged)
    Q_PROPERTY(QString speedLabel READ speed_label NOTIFY speedChanged)
    Q_PROPERTY(QString speedInputText READ speed_input_text NOTIFY speedInputChanged)
    Q_PROPERTY(QVariantList speedRows READ speed_rows CONSTANT)

   public:
    explicit PlayerInspectorViewModel(QObject* parent = nullptr);
    ~PlayerInspectorViewModel() override;

    QString display_title() const;
    QString header_caption() const;
    QVariantList episodes() const;
    QVariantMap current_episode() const;
    QVariantList video_tracks() const;
    QVariantList audio_tracks() const;
    QVariantList subtitle_tracks() const;
    int subtitle_milliseconds() const;
    QString subtitle_value_text() const;
    QString subtitle_input_text() const;
    int subtitle_slider_milliseconds() const;
    double audio_delay() const;
    QString audio_value_text() const;
    bool audio_neutral() const;
    double speed() const;
    QString speed_label() const;
    QString speed_input_text() const;
    QVariantList speed_rows() const;

    Q_INVOKABLE void setVideo(const QUrl& url, const QString& title);
    Q_INVOKABLE void setTracks(const QVariantList& tracks);
    Q_INVOKABLE void setPlaybackState(double speed, double subtitleDelay, double audioDelay);
    Q_INVOKABLE void selectEpisode(int index);
    Q_INVOKABLE void selectTrack(const QString& type, int id);
    Q_INVOKABLE void stepSubtitleMilliseconds(int delta);
    Q_INVOKABLE void setSubtitleMilliseconds(int milliseconds);
    Q_INVOKABLE void setSubtitleSliderMilliseconds(double milliseconds);
    Q_INVOKABLE void setSubtitleInput(const QString& text);
    Q_INVOKABLE void commitSubtitleInput();
    Q_INVOKABLE void stepAudioDelay(double delta);
    Q_INVOKABLE void setAudioDelay(double seconds);
    Q_INVOKABLE void setSpeedInput(const QString& text);
    Q_INVOKABLE void commitSpeedInput();
    Q_INVOKABLE void chooseSpeed(double speed);
    Q_INVOKABLE bool isSpeedSelected(double speed) const;

   signals:
    void displayTitleChanged();
    void headerCaptionChanged();
    void episodesChanged();
    void tracksChanged();
    void subtitleChanged();
    void subtitleInputChanged();
    void audioChanged();
    void speedChanged();
    void speedInputChanged();

    void episodeRequested(const QUrl& url);
    void selectTrackRequested(const QString& type, int id);
    void speedRequested(double speed);
    void subtitleDelayRequested(double seconds);
    void audioDelayRequested(double seconds);

   private:
    void rebuild_playlist();
    void rebuild_tracks();
    void apply_speed(double requested);
    void apply_subtitle_milliseconds(int milliseconds);

    QUrl current_url_;
    QString current_title_;
    QList<QUrl> episode_urls_;
    int current_index_ = -1;

    QVariantList tracks_;
    QVariantList video_tracks_;
    QVariantList audio_tracks_;
    QVariantList subtitle_tracks_;

    double speed_ = 1.0;
    double subtitle_delay_ = 0.0;
    double audio_delay_ = 0.0;
    QString subtitle_input_ = QStringLiteral("0");
    QString speed_input_ = QStringLiteral("1");
};

}  // namespace niratan::viewmodels
