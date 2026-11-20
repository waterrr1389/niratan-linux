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
//
// The handle is created with the same safety options as HSMpvClient:
// config=no, osc=no, input-default-bindings=no, keep-open=yes, vo=libmpv.
// Hardware decoding is auto-safe; if the render context cannot be created the
// item reports an error and stays black.

#pragma once

#include <QQuickFramebufferObject>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <mpv/client.h>

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

    Q_INVOKABLE void open(const QUrl& file);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seek(qreal seconds);
    Q_INVOKABLE void skip(qreal delta);
    Q_INVOKABLE void seekToChapter(int index);
    Q_INVOKABLE void cycleSubtitles();
    Q_INVOKABLE void toggleSubtitles();
    Q_INVOKABLE void setABLoopStart();
    Q_INVOKABLE void setABLoopEnd();
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

   private:
    void poll();
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
};
