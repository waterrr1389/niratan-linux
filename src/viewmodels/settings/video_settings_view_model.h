// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Video settings page, following Features/Settings/VideoSettingsView.swift
// body and Core/UserConfig.swift. One property per stored key: the Playback
// toggles, the gap fast-forward speed, the control bar layout and seek
// interval, the enhancement toggles and equalizer, the mining limit and the
// subtitle mask keys. Defaults, ranges and steps are the UserConfig.init values
// and the controls of the Swift body; a setter clamps to range and rounds to
// step before it writes, so the stored value is always one the page can show.
//
// Every setter writes only its edited key through
// services/player/video_settings and leaves the other keys of settings.json
// untouched. A successful write updates the cached map, clears the error and
// emits the property's change and videoSettingsChanged; a failed write leaves
// the value unchanged and exposes the error. The page passes
// videoSettingsChanged to the shell so an open player reloads its settings.
//
// The stored values are the Swift raw values: videoControlBarLayout is
// "floating" or "compactBottom", videoSubtitleMaskMode is "Blur" or
// "Transparent". An unsupported enum value is ignored rather than written.
// The mask keys follow the mode: Blur Radius is shown and written only in
// Blur, Hidden Opacity only in Transparent, and both are stored independently
// as Swift does. Anime4K is not part of this page (V0 Decision).

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace niratan::viewmodels {

class VideoSettingsViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool videoAutoPlayNext READ videoAutoPlayNext WRITE setVideoAutoPlayNext NOTIFY
                   videoAutoPlayNextChanged)
    Q_PROPERTY(bool videoRememberPlaybackPosition READ videoRememberPlaybackPosition WRITE
                   setVideoRememberPlaybackPosition NOTIFY videoRememberPlaybackPositionChanged)
    Q_PROPERTY(bool videoAutoPauseOnLookup READ videoAutoPauseOnLookup WRITE
                   setVideoAutoPauseOnLookup NOTIFY videoAutoPauseOnLookupChanged)
    Q_PROPERTY(
        bool videoSubtitleGapFastForwardEnabled READ videoSubtitleGapFastForwardEnabled WRITE
            setVideoSubtitleGapFastForwardEnabled NOTIFY videoSubtitleGapFastForwardEnabledChanged)
    Q_PROPERTY(
        double videoSubtitleGapFastForwardSpeed READ videoSubtitleGapFastForwardSpeed WRITE
            setVideoSubtitleGapFastForwardSpeed NOTIFY videoSubtitleGapFastForwardSpeedChanged)
    Q_PROPERTY(QString videoControlBarLayout READ videoControlBarLayout WRITE
                   setVideoControlBarLayout NOTIFY videoControlBarLayoutChanged)
    Q_PROPERTY(int videoSeekInterval READ videoSeekInterval WRITE setVideoSeekInterval NOTIFY
                   videoSeekIntervalChanged)
    Q_PROPERTY(bool videoHardwareDecodingEnabled READ videoHardwareDecodingEnabled WRITE
                   setVideoHardwareDecodingEnabled NOTIFY videoHardwareDecodingEnabledChanged)
    Q_PROPERTY(bool videoDeinterlacingEnabled READ videoDeinterlacingEnabled WRITE
                   setVideoDeinterlacingEnabled NOTIFY videoDeinterlacingEnabledChanged)
    Q_PROPERTY(bool videoHDREnhancementEnabled READ videoHDREnhancementEnabled WRITE
                   setVideoHDREnhancementEnabled NOTIFY videoHDREnhancementEnabledChanged)
    Q_PROPERTY(int videoBrightness READ videoBrightness WRITE setVideoBrightness NOTIFY
                   videoBrightnessChanged)
    Q_PROPERTY(
        int videoContrast READ videoContrast WRITE setVideoContrast NOTIFY videoContrastChanged)
    Q_PROPERTY(int videoSaturation READ videoSaturation WRITE setVideoSaturation NOTIFY
                   videoSaturationChanged)
    Q_PROPERTY(int videoGamma READ videoGamma WRITE setVideoGamma NOTIFY videoGammaChanged)
    Q_PROPERTY(int videoHue READ videoHue WRITE setVideoHue NOTIFY videoHueChanged)
    Q_PROPERTY(int videoMiningHistoryLimit READ videoMiningHistoryLimit WRITE
                   setVideoMiningHistoryLimit NOTIFY videoMiningHistoryLimitChanged)
    Q_PROPERTY(bool videoSubtitleMaskEnabled READ videoSubtitleMaskEnabled WRITE
                   setVideoSubtitleMaskEnabled NOTIFY videoSubtitleMaskEnabledChanged)
    Q_PROPERTY(QString videoSubtitleMaskMode READ videoSubtitleMaskMode WRITE
                   setVideoSubtitleMaskMode NOTIFY videoSubtitleMaskModeChanged)
    Q_PROPERTY(int videoSubtitleMaskBlurRadius READ videoSubtitleMaskBlurRadius WRITE
                   setVideoSubtitleMaskBlurRadius NOTIFY videoSubtitleMaskBlurRadiusChanged)
    Q_PROPERTY(double videoSubtitleMaskHiddenOpacity READ videoSubtitleMaskHiddenOpacity WRITE
                   setVideoSubtitleMaskHiddenOpacity NOTIFY videoSubtitleMaskHiddenOpacityChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

   public:
    explicit VideoSettingsViewModel(QObject* parent = nullptr);

    bool videoAutoPlayNext() const;
    bool videoRememberPlaybackPosition() const;
    bool videoAutoPauseOnLookup() const;
    bool videoSubtitleGapFastForwardEnabled() const;
    double videoSubtitleGapFastForwardSpeed() const;
    QString videoControlBarLayout() const;
    int videoSeekInterval() const;
    bool videoHardwareDecodingEnabled() const;
    bool videoDeinterlacingEnabled() const;
    bool videoHDREnhancementEnabled() const;
    int videoBrightness() const;
    int videoContrast() const;
    int videoSaturation() const;
    int videoGamma() const;
    int videoHue() const;
    int videoMiningHistoryLimit() const;
    bool videoSubtitleMaskEnabled() const;
    QString videoSubtitleMaskMode() const;
    int videoSubtitleMaskBlurRadius() const;
    double videoSubtitleMaskHiddenOpacity() const;
    QString error() const;

    void setVideoAutoPlayNext(bool value);
    void setVideoRememberPlaybackPosition(bool value);
    void setVideoAutoPauseOnLookup(bool value);
    void setVideoSubtitleGapFastForwardEnabled(bool value);
    void setVideoSubtitleGapFastForwardSpeed(double value);
    void setVideoControlBarLayout(const QString& value);
    void setVideoSeekInterval(int value);
    void setVideoHardwareDecodingEnabled(bool value);
    void setVideoDeinterlacingEnabled(bool value);
    void setVideoHDREnhancementEnabled(bool value);
    void setVideoBrightness(int value);
    void setVideoContrast(int value);
    void setVideoSaturation(int value);
    void setVideoGamma(int value);
    void setVideoHue(int value);
    void setVideoMiningHistoryLimit(int value);
    void setVideoSubtitleMaskEnabled(bool value);
    void setVideoSubtitleMaskMode(const QString& value);
    void setVideoSubtitleMaskBlurRadius(int value);
    void setVideoSubtitleMaskHiddenOpacity(double value);

   signals:
    void videoAutoPlayNextChanged();
    void videoRememberPlaybackPositionChanged();
    void videoAutoPauseOnLookupChanged();
    void videoSubtitleGapFastForwardEnabledChanged();
    void videoSubtitleGapFastForwardSpeedChanged();
    void videoControlBarLayoutChanged();
    void videoSeekIntervalChanged();
    void videoHardwareDecodingEnabledChanged();
    void videoDeinterlacingEnabledChanged();
    void videoHDREnhancementEnabledChanged();
    void videoBrightnessChanged();
    void videoContrastChanged();
    void videoSaturationChanged();
    void videoGammaChanged();
    void videoHueChanged();
    void videoMiningHistoryLimitChanged();
    void videoSubtitleMaskEnabledChanged();
    void videoSubtitleMaskModeChanged();
    void videoSubtitleMaskBlurRadiusChanged();
    void videoSubtitleMaskHiddenOpacityChanged();
    void errorChanged();
    void videoSettingsChanged();

   private:
    // Writes one key, keeping the cached value only on success.
    bool write(const QString& key, const QJsonValue& value);
    void set_error(const QString& error);

    QJsonObject settings_;
    QString error_;
};

}  // namespace niratan::viewmodels
