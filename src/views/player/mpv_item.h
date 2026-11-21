// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video surface, following the mpv state mirrored by PlaybackEngine.swift
// and HSMpvClient.mm: one libmpv handle, the render API drawn into the Quick
// framebuffer object, and the property set the player UI needs.
//
// mpv itself draws the video and the subtitles: subtitle appearance is mapped
// onto mpv's sub-* options (font size 36 by default, the configured vertical
// position, the edge style as an outline/shadow strength), so the same cue
// timing, delay and track selection rules as the macOS client apply.
//
// All commands are marshalled to the main thread; the render context lives on
// the render thread and reports frames through the update callback. Property
// polling runs at 10 Hz and emits only when a value the UI shows changes,
// which keeps the QML bindings quiet while mpv plays.
// Cue times are in the subtitle timeline (before delay); missing times read
// as -1. Track types use mpv's video, audio and sub identifiers.
// Local resume follows VideoPlayerViewModel.swift's save and loaded-restore
// order, with a ten-second margin at each end and no remote history.
// Embedded transcripts follow VideoPlayerScreen+Subtitles.swift's track
// lifecycle, using mpv's ff-index (not its subtitle selection ID) for ffmpeg.
//
// The handle is created with the same safety options as HSMpvClient:
// config=no, osc=no, input-default-bindings=no, keep-open=yes, vo=libmpv.
// Hardware decoding is auto-safe; if the render context cannot be created the
// item reports an error and stays black.

#pragma once

#include <QQuickFramebufferObject>
#include <QByteArray>
#include <QElapsedTimer>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <mpv/client.h>

class QProcess;

class MpvItem : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal position READ position NOTIFY positionChanged)
    Q_PROPERTY(qreal duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(qreal speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(qreal subtitleDelay READ subtitleDelay WRITE setSubtitleDelay NOTIFY
                   subtitleDelayChanged)
    Q_PROPERTY(qreal audioDelay READ audioDelay WRITE setAudioDelay NOTIFY audioDelayChanged)
    Q_PROPERTY(bool looping READ looping WRITE setLooping NOTIFY loopingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(bool resumeAvailable READ resumeAvailable NOTIFY resumeChanged)
    Q_PROPERTY(qreal resumePosition READ resumePosition NOTIFY resumeChanged)
    Q_PROPERTY(bool extractingTranscript READ extractingTranscript NOTIFY extractingTranscriptChanged)
    Q_PROPERTY(QString subtitleText READ subtitleText NOTIFY subtitleCueChanged)
    Q_PROPERTY(qreal subtitleStart READ subtitleStart NOTIFY subtitleCueChanged)
    Q_PROPERTY(qreal subtitleEnd READ subtitleEnd NOTIFY subtitleCueChanged)
    Q_PROPERTY(bool subtitlesVisible READ subtitlesVisible WRITE setSubtitlesVisible NOTIFY
                   subtitlesVisibleChanged)

   public:
    explicit MpvItem(QQuickItem* parent = nullptr);
    ~MpvItem() override;

    Renderer* createRenderer() const override;

    qreal position() const;
    qreal duration() const;
    bool paused() const;
    void setPaused(bool paused);
    bool loaded() const;
    bool muted() const;
    void setMuted(bool muted);
    qreal volume() const;
    void setVolume(qreal volume);
    qreal speed() const;
    void setSpeed(qreal speed);
    qreal subtitleDelay() const;
    void setSubtitleDelay(qreal delay);
    qreal audioDelay() const;
    void setAudioDelay(qreal delay);
    bool looping() const;
    void setLooping(bool looping);
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

    Q_INVOKABLE void open(const QUrl& file);
    Q_INVOKABLE void unload();
    Q_INVOKABLE void restartFromBeginning();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seek(qreal seconds);
    Q_INVOKABLE void skip(qreal delta);
    Q_INVOKABLE void seekToChapter(int index);
    Q_INVOKABLE bool loadExternalSubtitle(const QUrl& file);
    Q_INVOKABLE void selectSubtitleTrack(int id);
    Q_INVOKABLE void extractTranscript();
    Q_INVOKABLE void disableSubtitles();
    // Reports whether mpv accepted the asynchronous request.
    Q_INVOKABLE bool screenshot(const QUrl& file);
    Q_INVOKABLE void cycleSubtitles();
    Q_INVOKABLE void toggleSubtitles();
    // false when the current position cannot start or end a range; the
    // caller must not show the range as set in that case.
    Q_INVOKABLE bool setABLoopStart();
    Q_INVOKABLE bool setABLoopEnd();
    Q_INVOKABLE void clearABLoop();
    Q_INVOKABLE void rotateClockwise();
    Q_INVOKABLE void stepSpeed(int direction);
    Q_INVOKABLE void resetSpeed();

    // Subtitle appearance, applied to mpv's subtitle renderer.
    Q_INVOKABLE void applySubtitleStyle(int font_size, int position_percent, int weight,
                                        double background_opacity);

    // The mpv handle is shared with the renderer, which is created on the
    // scene graph thread.
    mpv_handle* handle() const;
    void setRenderContext(void* context);
    void requestUpdate();

   signals:
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void loadedChanged();
    void mutedChanged();
    void volumeChanged();
    void speedChanged();
    void subtitleDelayChanged();
    void audioDelayChanged();
    void loopingChanged();
    void errorChanged();
    void ended();
    void chapterChanged(int index);
    void chaptersChanged();
    void tracksChanged();
    void resumeChanged();
    void extractingTranscriptChanged();
    void embeddedTranscriptChanged(const QByteArray& bytes, const QString& error);
    void subtitleCueChanged();
    void subtitlesVisibleChanged();
    void subtitleDocumentChanged(const QUrl& file, const QByteArray& bytes, bool readable);

   private:
    void poll();
    void replace_file(const QUrl& file);
    void open_when_ready(const QUrl& file, quint64 generation);
    void save_playback_position();
    void clear_playback_position();
    void restore_playback_position();
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

    mpv_handle* handle_ = nullptr;
    void* render_context_ = nullptr;
    QTimer poll_timer_;
    qreal position_ = 0.0;
    qreal duration_ = 0.0;
    bool paused_ = false;
    bool loaded_ = false;
    bool muted_ = false;
    qreal volume_ = 100.0;
    qreal speed_ = 1.0;
    qreal subtitle_delay_ = 0.0;
    qreal audio_delay_ = 0.0;
    bool looping_ = false;
    QString error_;
    qreal ab_loop_start_ = -1.0;
    int chapter_index_ = -1;
    quint64 load_generation_ = 0;
    bool waiting_for_context_ = false;
    bool file_ready_ = false;
    bool ended_ = false;
    QString local_video_path_;
    QElapsedTimer resume_save_timer_;
    double resume_position_ = 0.0;
    int selected_subtitle_id_ = -1;
    QProcess* transcript_process_ = nullptr;
    bool chapters_dirty_ = true;
    bool tracks_dirty_ = true;
    QVariantList chapters_;
    QVariantList tracks_;
    QUrl subtitle_document_;
    QString subtitle_text_;
    qreal subtitle_start_ = -1.0;
    qreal subtitle_end_ = -1.0;
    bool subtitles_visible_ = true;
};
