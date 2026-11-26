// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video surface, following the mpv state mirrored by PlaybackEngine.swift
// and HSMpvClient.mm: one libmpv handle, the render API drawn into the Quick
// framebuffer object, and the property set the player UI needs.
//
// mpv draws the video and subtitles. Native sub-* options approximate the
// Swift typography, placement and edges; they do not expose interactive text.
//
// All commands are marshalled to the main thread; the render context lives on
// the render thread and reports frames through the update callback. Property
// polling runs at 10 Hz and emits only when a value the UI shows changes,
// which keeps the QML bindings quiet while mpv plays.
// Cue times are in the subtitle timeline (before delay); missing times read
// as -1. Track types use mpv's video, audio and sub identifiers.
// Local resume follows VideoPlayerViewModel.swift's save and loaded-restore
// order through player::PlaybackHistory: saved every 5 s while playing and
// when the file changes or the item goes away, recorded as finished at the
// end. Opening from the beginning clears progress after saving the outgoing
// file. The position and per-video options are applied once duration is known.
// Embedded transcripts follow VideoPlayerScreen+Subtitles.swift's track
// lifecycle: the item picks the selected text track and hands mpv's ff-index
// (not its subtitle selection ID) to player::TranscriptExtractor, and cancels
// the extraction when the track or file changes.
//
// The handle is created with the same safety options as HSMpvClient:
// config=no, osc=no, input-default-bindings=no, keep-open=yes, vo=libmpv.
// Hardware decoding defaults to auto-safe; if the render context cannot be
// created the item reports an error and stays black.
//
// nativeSubtitleRendering selects who draws subtitles. It stays true for mpv
// (image tracks and no text document); the player sets it false while the
// lookup overlay draws a parsed text document.

#pragma once

#include "services/player/playback_controls.h"
#include "services/player/playback_history.h"
#include "services/player/subtitle_default.h"
#include "services/player/subtitle_document.h"
#include "services/player/subtitle_gap_playback.h"
#include "services/player/transcript_extractor.h"

#include <QQuickFramebufferObject>
#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <mpv/client.h>

#include <optional>

class MpvItem : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal position READ position NOTIFY positionChanged)
    Q_PROPERTY(qreal duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    // mpv's seeking flag: the current frame is not final while a seek runs.
    Q_PROPERTY(bool seeking READ seeking NOTIFY seekingChanged)
    // mpv's dwidth/dheight (the display size with the aspect applied) and
    // video-rotate, empty and 0 while no file is loaded. The Video window
    // derives its aspect fit from these.
    Q_PROPERTY(QSizeF videoDisplaySize READ videoDisplaySize NOTIFY videoDisplaySizeChanged)
    Q_PROPERTY(int videoRotation READ videoRotation NOTIFY videoRotationChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(qreal speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(qreal loopStart READ loopStart NOTIFY abLoopChanged)
    Q_PROPERTY(qreal loopEnd READ loopEnd NOTIFY abLoopChanged)
    Q_PROPERTY(qreal subtitleDelay READ subtitleDelay WRITE setSubtitleDelay NOTIFY
                   subtitleDelayChanged)
    Q_PROPERTY(qreal audioDelay READ audioDelay WRITE setAudioDelay NOTIFY audioDelayChanged)
    Q_PROPERTY(bool looping READ looping WRITE setLooping NOTIFY loopingChanged)
    // VideoAspectRatio.rawValue: "-1" automatic, otherwise "16:9", "4:3",
    // "1:1" or "21:9". Written to mpv's video-aspect-override and kept across
    // a file change.
    Q_PROPERTY(QString aspectRatio READ aspectRatio WRITE setAspectRatio NOTIFY aspectRatioChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(QVariantMap videoSettings READ videoSettings NOTIFY videoSettingsChanged)
    Q_PROPERTY(bool resumeAvailable READ resumeAvailable NOTIFY resumeChanged)
    Q_PROPERTY(qreal resumePosition READ resumePosition NOTIFY resumeChanged)
    Q_PROPERTY(bool extractingTranscript READ extractingTranscript NOTIFY extractingTranscriptChanged)
    Q_PROPERTY(QString subtitleText READ subtitleText NOTIFY subtitleCueChanged)
    Q_PROPERTY(qreal subtitleStart READ subtitleStart NOTIFY subtitleCueChanged)
    Q_PROPERTY(qreal subtitleEnd READ subtitleEnd NOTIFY subtitleCueChanged)
    Q_PROPERTY(bool subtitlesVisible READ subtitlesVisible WRITE setSubtitlesVisible NOTIFY
                   subtitlesVisibleChanged)
    Q_PROPERTY(bool nativeSubtitleRendering READ nativeSubtitleRendering WRITE
                   setNativeSubtitleRendering NOTIFY nativeSubtitleRenderingChanged)

   public:
    explicit MpvItem(QQuickItem* parent = nullptr);
    ~MpvItem() override;

    Renderer* createRenderer() const override;

    qreal position() const;
    qreal duration() const;
    bool paused() const;
    void setPaused(bool paused);
    bool loaded() const;
    bool seeking() const;
    QSizeF videoDisplaySize() const;
    int videoRotation() const;
    bool muted() const;
    void setMuted(bool muted);
    qreal volume() const;
    void setVolume(qreal volume);
    qreal speed() const;
    void setSpeed(qreal speed);
    qreal loopStart() const;
    qreal loopEnd() const;
    qreal subtitleDelay() const;
    void setSubtitleDelay(qreal delay);
    qreal audioDelay() const;
    void setAudioDelay(qreal delay);
    bool looping() const;
    void setLooping(bool looping);
    QString aspectRatio() const;
    void setAspectRatio(const QString& aspectRatio);
    QString error() const;
    QVariantList chapters() const;
    QVariantList tracks() const;
    bool resumeAvailable() const;
    qreal resumePosition() const;
    bool extractingTranscript() const;
    QString subtitleText() const;
    qreal subtitleStart() const;
    qreal subtitleEnd() const;
    bool subtitlesVisible() const;
    void setSubtitlesVisible(bool visible);
    // False while the overlay draws a text subtitle document; mpv then keeps
    // its own renderer off. Image tracks and no document keep it true.
    bool nativeSubtitleRendering() const;
    void setNativeSubtitleRendering(bool native);

    Q_INVOKABLE void open(const QUrl& file, bool fromBeginning = false);
    Q_INVOKABLE void unload();
    Q_INVOKABLE void restartFromBeginning();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seek(qreal seconds);
    Q_INVOKABLE void skip(qreal delta);
    Q_INVOKABLE void seekToChapter(int index);
    Q_INVOKABLE bool loadExternalSubtitle(const QUrl& file);
    Q_INVOKABLE void selectSubtitleTrack(int id);
    // Audio and video track selection by mpv's type spelling; 0 turns the
    // track off. Subtitle selection keeps its remembering call sites.
    Q_INVOKABLE void selectTrack(const QString& type, int id);
    Q_INVOKABLE void extractTranscript();
    Q_INVOKABLE void disableSubtitles();
    // Reports whether mpv accepted the asynchronous request; screenshotSaved
    // follows once mpv has finished writing (or failed to write) the file.
    Q_INVOKABLE bool screenshot(const QUrl& file);
    // The save panel's temporary capture, and moving it to the chosen
    // destination. An empty destination discards the capture.
    Q_INVOKABLE QUrl temporaryScreenshotFile() const;
    Q_INVOKABLE bool writeScreenshotFile(const QUrl& source, const QUrl& destination);
    // Returns the selected ID, 0 for Off, or -1 if there are no tracks.
    Q_INVOKABLE int cycleSubtitles();
    Q_INVOKABLE void toggleSubtitles();
    // false when the current position cannot start or end a range; the
    // caller must not show the range as set in that case.
    Q_INVOKABLE bool setABLoopStart();
    Q_INVOKABLE bool setABLoopEnd();
    Q_INVOKABLE bool setABLoopStartAt(qreal time);
    Q_INVOKABLE bool setABLoopEndAt(qreal time);
    Q_INVOKABLE void clearABLoop();
    Q_INVOKABLE void rotateClockwise();
    Q_INVOKABLE void stepSpeed(int direction);
    Q_INVOKABLE void resetSpeed();
    Q_INVOKABLE QString speedLabel(qreal value) const;
    Q_INVOKABLE bool acceptSpeedInput(const QString& input);

    QVariantMap videoSettings() const;
    Q_INVOKABLE bool changeVideoSetting(const QString& key, const QVariant& value);
    // Re-reads settings.json after another surface wrote it, applies the map
    // again and notifies the page.
    Q_INVOKABLE void reloadVideoSettings();
    Q_INVOKABLE bool resetSubtitleAppearance();
    // The primary transcript cues, in subtitle order, for the gap policy.
    Q_INVOKABLE void setPrimaryCues(const QVariantList& cues);

   signals:
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void loadedChanged();
    void seekingChanged();
    void videoDisplaySizeChanged();
    void videoRotationChanged();
    void mutedChanged();
    void volumeChanged();
    void speedChanged();
    void abLoopChanged();
    void videoSettingsChanged();
    void subtitleDelayChanged();
    void audioDelayChanged();
    void loopingChanged();
    void aspectRatioChanged();
    void errorChanged();
    void ended();
    void chapterChanged(int index);
    void chaptersChanged();
    void tracksChanged();
    void resumeChanged();
    void extractingTranscriptChanged();
    void embeddedTranscriptChanged(const QByteArray& bytes, const QString& error);
    void subtitleCueChanged();
    void subtitleLoadFailed();
    void subtitlesVisibleChanged();
    void nativeSubtitleRenderingChanged();
    void subtitleDocumentChanged(const QUrl& file, const QByteArray& bytes, bool readable);
    // ok when mpv's screenshot-to-file command succeeded and the file is not empty.
    void screenshotSaved(const QUrl& file, bool ok);

   private:
    void poll();
    void replace_file(const QUrl& file, bool from_beginning = false);
    void open_when_ready(const QUrl& file, quint64 generation);
    void save_playback_position();
    void record_finished_playback();
    void restore_playback_position();
    void apply_speed(double speed);
    void apply_gap_settings();
    void update_gap_playback();
    void restore_gap_speed();
    void restore_subtitle_selection();
    void reset_subtitle_selection_state();
    void refresh_tracks_and_embedded_default();
    void apply_pending_embedded_subtitle_default();
    QList<niratan::player::VideoTrack> video_tracks() const;
    void remember_subtitle_selection(const niratan::player::SubtitleSelection& selection);
    bool load_external_subtitle(const QUrl& file, bool remember);
    void disable_subtitles(bool remember);
    void select_subtitle_track(int id, bool remember);
    void select_subtitle_track_with_transcript(const QVariantMap& track, bool remember);
    QVariantList current_track_fields() const;
    void set_resume_position(double position);
    void cancel_transcript();
    void refresh_chapters();
    void refresh_tracks();
    void set_subtitle_cue(const QString& text, double start, double end);
    void command(const QByteArray& name);
    void set_property(const char* name, const QString& value);
    void set_property(const char* name, double value);
    void set_property(const char* name, int value);
    void set_property(const char* name, bool value);
    double get_double(const char* name, double fallback) const;
    bool get_bool(const char* name, bool fallback) const;
    QString get_string(const char* name) const;
    bool save_video_settings(const QJsonObject& changes);
    void apply_video_settings();

    QJsonObject video_settings_;
    mpv_handle* handle_ = nullptr;
    QTimer poll_timer_;
    qreal position_ = 0.0;
    qreal duration_ = 0.0;
    bool paused_ = false;
    bool loaded_ = false;
    bool seeking_ = false;
    QSizeF video_display_size_;
    int video_rotation_ = 0;
    bool muted_ = false;
    qreal volume_ = 100.0;
    qreal speed_ = 1.0;
    qreal subtitle_delay_ = 0.0;
    qreal audio_delay_ = 0.0;
    bool looping_ = false;
    QString aspect_ratio_ = QStringLiteral("-1");
    QString error_;
    niratan::player::ABLoop ab_loop_;
    int chapter_index_ = -1;
    quint64 load_generation_ = 0;
    // Screenshot commands awaiting mpv's reply, by reply_userdata. The ids
    // start above the property observation ids.
    QHash<quint64, QUrl> pending_screenshots_;
    quint64 next_screenshot_reply_ = 1000;
    bool waiting_for_context_ = false;
    bool file_ready_ = false;
    bool ended_ = false;
    QString local_video_path_;
    QElapsedTimer resume_save_timer_;
    double resume_position_ = 0.0;
    niratan::player::PlaybackOpenState pending_playback_;
    niratan::player::PlaybackHistory playback_history_ = niratan::player::PlaybackHistory::at_data_root();
    niratan::player::SubtitleGapPlayback gap_playback_;
    std::optional<niratan::player::SubtitleCueStore> primary_store_;
    int selected_subtitle_id_ = -1;
    int last_subtitle_id_ = -1;
    bool subtitle_restore_pending_ = false;
    niratan::player::EmbeddedSubtitleDefault embedded_subtitle_default_;
    niratan::player::TranscriptExtractor transcript_extractor_{
        niratan::player::TranscriptExtractor::default_cache_directory()};
    bool chapters_dirty_ = true;
    bool tracks_dirty_ = true;
    QVariantList chapters_;
    QVariantList tracks_;
    QUrl subtitle_document_;
    QString subtitle_text_;
    qreal subtitle_start_ = -1.0;
    qreal subtitle_end_ = -1.0;
    bool subtitles_visible_ = true;
    bool native_subtitle_rendering_ = true;
};
