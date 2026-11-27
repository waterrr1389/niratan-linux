// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_settings_view_model.h"

#include "services/player/video_settings.h"
#include "services/storage/app_roots.h"

#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>
#include <cmath>

namespace niratan::viewmodels {
namespace {

QString text(const QJsonObject& settings, const char* key) {
    return settings.value(QLatin1StringView(key)).toString();
}

bool flag(const QJsonObject& settings, const char* key) {
    return settings.value(QLatin1StringView(key)).toBool();
}

int number(const QJsonObject& settings, const char* key) {
    return settings.value(QLatin1StringView(key)).toInt();
}

double decimal(const QJsonObject& settings, const char* key) {
    return settings.value(QLatin1StringView(key)).toDouble();
}

// Rounds to the step of the control that edits the value, ties away from zero.
double on_step(double value, double step) {
    return std::round(value / step) * step;
}

}  // namespace

VideoSettingsViewModel::VideoSettingsViewModel(QObject* parent)
    : QObject(parent), settings_(player::load_video_settings(storage::data_root())) {}

bool VideoSettingsViewModel::videoAutoPlayNext() const {
    return flag(settings_, "videoAutoPlayNext");
}
bool VideoSettingsViewModel::videoRememberPlaybackPosition() const {
    return flag(settings_, "videoRememberPlaybackPosition");
}
bool VideoSettingsViewModel::videoAutoPauseOnLookup() const {
    return flag(settings_, "videoAutoPauseOnLookup");
}
bool VideoSettingsViewModel::videoSubtitleGapFastForwardEnabled() const {
    return flag(settings_, "videoSubtitleGapFastForwardEnabled");
}
double VideoSettingsViewModel::videoSubtitleGapFastForwardSpeed() const {
    return decimal(settings_, "videoSubtitleGapFastForwardSpeed");
}
QString VideoSettingsViewModel::videoControlBarLayout() const {
    return text(settings_, "videoControlBarLayout");
}
int VideoSettingsViewModel::videoSeekInterval() const {
    return number(settings_, "videoSeekInterval");
}
bool VideoSettingsViewModel::videoHardwareDecodingEnabled() const {
    return flag(settings_, "videoHardwareDecodingEnabled");
}
bool VideoSettingsViewModel::videoDeinterlacingEnabled() const {
    return flag(settings_, "videoDeinterlacingEnabled");
}
bool VideoSettingsViewModel::videoHDREnhancementEnabled() const {
    return flag(settings_, "videoHDREnhancementEnabled");
}
int VideoSettingsViewModel::videoBrightness() const {
    return number(settings_, "videoBrightness");
}
int VideoSettingsViewModel::videoContrast() const {
    return number(settings_, "videoContrast");
}
int VideoSettingsViewModel::videoSaturation() const {
    return number(settings_, "videoSaturation");
}
int VideoSettingsViewModel::videoGamma() const {
    return number(settings_, "videoGamma");
}
int VideoSettingsViewModel::videoHue() const {
    return number(settings_, "videoHue");
}
int VideoSettingsViewModel::videoMiningHistoryLimit() const {
    return number(settings_, "videoMiningHistoryLimit");
}
bool VideoSettingsViewModel::videoSubtitleMaskEnabled() const {
    return flag(settings_, "videoSubtitleMaskEnabled");
}
QString VideoSettingsViewModel::videoSubtitleMaskMode() const {
    return text(settings_, "videoSubtitleMaskMode");
}
int VideoSettingsViewModel::videoSubtitleMaskBlurRadius() const {
    return number(settings_, "videoSubtitleMaskBlurRadius");
}
double VideoSettingsViewModel::videoSubtitleMaskHiddenOpacity() const {
    return decimal(settings_, "videoSubtitleMaskHiddenOpacity");
}
QString VideoSettingsViewModel::error() const {
    return error_;
}

void VideoSettingsViewModel::setVideoAutoPlayNext(bool value) {
    if (flag(settings_, "videoAutoPlayNext") == value || !write("videoAutoPlayNext", value)) {
        return;
    }
    emit videoAutoPlayNextChanged();
}

void VideoSettingsViewModel::setVideoRememberPlaybackPosition(bool value) {
    if (flag(settings_, "videoRememberPlaybackPosition") == value ||
        !write("videoRememberPlaybackPosition", value)) {
        return;
    }
    emit videoRememberPlaybackPositionChanged();
}

void VideoSettingsViewModel::setVideoAutoPauseOnLookup(bool value) {
    if (flag(settings_, "videoAutoPauseOnLookup") == value ||
        !write("videoAutoPauseOnLookup", value)) {
        return;
    }
    emit videoAutoPauseOnLookupChanged();
}

void VideoSettingsViewModel::setVideoSubtitleGapFastForwardEnabled(bool value) {
    if (flag(settings_, "videoSubtitleGapFastForwardEnabled") == value ||
        !write("videoSubtitleGapFastForwardEnabled", value)) {
        return;
    }
    emit videoSubtitleGapFastForwardEnabledChanged();
}

void VideoSettingsViewModel::setVideoSubtitleGapFastForwardSpeed(double value) {
    const double bounded = on_step(std::clamp(value, 1.1, 5.0), 0.1);
    if (decimal(settings_, "videoSubtitleGapFastForwardSpeed") == bounded ||
        !write("videoSubtitleGapFastForwardSpeed", bounded)) {
        return;
    }
    emit videoSubtitleGapFastForwardSpeedChanged();
}

void VideoSettingsViewModel::setVideoControlBarLayout(const QString& value) {
    if ((value != QLatin1StringView("floating") && value != QLatin1StringView("compactBottom")) ||
        text(settings_, "videoControlBarLayout") == value ||
        !write("videoControlBarLayout", value)) {
        return;
    }
    emit videoControlBarLayoutChanged();
}

void VideoSettingsViewModel::setVideoSeekInterval(int value) {
    const int bounded = std::clamp(value, 1, 60);
    if (number(settings_, "videoSeekInterval") == bounded || !write("videoSeekInterval", bounded)) {
        return;
    }
    emit videoSeekIntervalChanged();
}

void VideoSettingsViewModel::setVideoHardwareDecodingEnabled(bool value) {
    if (flag(settings_, "videoHardwareDecodingEnabled") == value ||
        !write("videoHardwareDecodingEnabled", value)) {
        return;
    }
    emit videoHardwareDecodingEnabledChanged();
}

void VideoSettingsViewModel::setVideoDeinterlacingEnabled(bool value) {
    if (flag(settings_, "videoDeinterlacingEnabled") == value ||
        !write("videoDeinterlacingEnabled", value)) {
        return;
    }
    emit videoDeinterlacingEnabledChanged();
}

void VideoSettingsViewModel::setVideoHDREnhancementEnabled(bool value) {
    if (flag(settings_, "videoHDREnhancementEnabled") == value ||
        !write("videoHDREnhancementEnabled", value)) {
        return;
    }
    emit videoHDREnhancementEnabledChanged();
}

void VideoSettingsViewModel::setVideoBrightness(int value) {
    const int bounded = std::clamp(value, -100, 100);
    if (number(settings_, "videoBrightness") == bounded || !write("videoBrightness", bounded)) {
        return;
    }
    emit videoBrightnessChanged();
}

void VideoSettingsViewModel::setVideoContrast(int value) {
    const int bounded = std::clamp(value, -100, 100);
    if (number(settings_, "videoContrast") == bounded || !write("videoContrast", bounded)) {
        return;
    }
    emit videoContrastChanged();
}

void VideoSettingsViewModel::setVideoSaturation(int value) {
    const int bounded = std::clamp(value, -100, 100);
    if (number(settings_, "videoSaturation") == bounded || !write("videoSaturation", bounded)) {
        return;
    }
    emit videoSaturationChanged();
}

void VideoSettingsViewModel::setVideoGamma(int value) {
    const int bounded = std::clamp(value, -100, 100);
    if (number(settings_, "videoGamma") == bounded || !write("videoGamma", bounded)) {
        return;
    }
    emit videoGammaChanged();
}

void VideoSettingsViewModel::setVideoHue(int value) {
    const int bounded = std::clamp(value, -100, 100);
    if (number(settings_, "videoHue") == bounded || !write("videoHue", bounded)) {
        return;
    }
    emit videoHueChanged();
}

void VideoSettingsViewModel::setVideoMiningHistoryLimit(int value) {
    const int bounded = std::clamp(value, 0, 1000);
    if (number(settings_, "videoMiningHistoryLimit") == bounded ||
        !write("videoMiningHistoryLimit", bounded)) {
        return;
    }
    emit videoMiningHistoryLimitChanged();
}

void VideoSettingsViewModel::setVideoSubtitleMaskEnabled(bool value) {
    if (flag(settings_, "videoSubtitleMaskEnabled") == value ||
        !write("videoSubtitleMaskEnabled", value)) {
        return;
    }
    emit videoSubtitleMaskEnabledChanged();
}

void VideoSettingsViewModel::setVideoSubtitleMaskMode(const QString& value) {
    if ((value != QLatin1StringView("Blur") && value != QLatin1StringView("Transparent")) ||
        text(settings_, "videoSubtitleMaskMode") == value ||
        !write("videoSubtitleMaskMode", value)) {
        return;
    }
    emit videoSubtitleMaskModeChanged();
}

void VideoSettingsViewModel::setVideoSubtitleMaskBlurRadius(int value) {
    const int bounded = std::clamp(value, 0, 20);
    if (number(settings_, "videoSubtitleMaskBlurRadius") == bounded ||
        !write("videoSubtitleMaskBlurRadius", bounded)) {
        return;
    }
    emit videoSubtitleMaskBlurRadiusChanged();
}

void VideoSettingsViewModel::setVideoSubtitleMaskHiddenOpacity(double value) {
    const double bounded = on_step(std::clamp(value, 0.0, 1.0), 0.05);
    if (decimal(settings_, "videoSubtitleMaskHiddenOpacity") == bounded ||
        !write("videoSubtitleMaskHiddenOpacity", bounded)) {
        return;
    }
    emit videoSubtitleMaskHiddenOpacityChanged();
}

bool VideoSettingsViewModel::write(const QString& key, const QJsonValue& value) {
    if (!player::store_video_settings(storage::data_root(), {{key, value}})) {
        set_error(tr("Could not save the video setting."));
        return false;
    }
    settings_.insert(key, value);
    set_error({});
    emit videoSettingsChanged();
    return true;
}

void VideoSettingsViewModel::set_error(const QString& error) {
    if (error_ == error) {
        return;
    }
    error_ = error;
    emit errorChanged();
}

}  // namespace niratan::viewmodels
