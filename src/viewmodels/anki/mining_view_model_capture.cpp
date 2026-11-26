// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video capture a mining attempt runs between preflight and the add,
// following Features/Popup/PopupView.swift (mineEntry, miningContextProvider)
// and Features/Video/VideoMiningCoordinator.swift (context and the media
// directory). The request carries the video's document state and the saved
// settings; the capture keeps the still/clip naming and failure messages in
// viewmodels/anki/video_mining_capture.cpp.
//
// The capture is stored in State before start, because start may complete
// inline. setContext (a reader context) and the next mine drop it; a new or
// cleared video context does not, because the page answers a screenshot only
// after mpv has written it, by which time the cue may have moved on or the
// popup may have closed. The screenshot signal is
// emitted from this model; the page writes the still and answers through
// screenshotFinished(ok).

#include "viewmodels/anki/mining_view_model_internal.h"

#include <QString>

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "services/anki/mining_gate.h"
#include "viewmodels/anki/video_mining_capture_start.h"

namespace niratan::viewmodels {

void MiningViewModel::start_video_capture(anki::MiningAttempt attempt,
                                          std::function<void(anki::AnkiMiningResult)> finish) {
    const auto& video = *state_->video;
    const auto backend = state_->backend;
    const auto& settings = backend->config.settings();

    VideoMiningCaptureRequest request;
    request.video_path = video.path;
    request.title = video.title;
    request.audio_stream_index = video.audio_stream_index;
    request.selection = video.selection;
    request.subtitle_delay = video.subtitle_delay;
    request.duration = video.duration;
    request.capture_screenshot = anki::needs_video_screenshot(settings.field_mappings);
    request.capture_audio_clip = anki::needs_video_audio_clip(settings.field_mappings);
    request.compress_screenshot = settings.compress_images;
    request.image_format = settings.image_compression_format;
    request.screenshot_quality = settings.image_compression_quality;
    request.audio_format = settings.audio_compression_format;
    request.audio_bitrate_kbps = settings.audio_compression_bitrate_kbps;
    request.animated_avif_maximum_height = settings.animated_avif_maximum_height;
    request.animated_avif_frames_per_second = settings.animated_avif_frames_per_second;
    request.ffmpeg_path = state_->ffmpeg;

    auto capture = make_video_capture(
        [backend](std::function<void(std::optional<std::string>)> complete) {
            backend->mining.media_directory([complete = std::move(complete)](auto directory) {
                complete(directory ? std::optional<std::string>(directory->string())
                                   : std::optional<std::string>{});
            });
        },
        [this](const QString& path) { emit screenshotRequested(path); });
    auto* active = capture.get();
    state_->capture = std::move(capture);
    active->start(
        request,
        [backend, attempt = std::move(attempt), finish](anki::MiningContext context) mutable {
            attempt.context = std::move(context);
            backend->mining.mine(std::move(attempt), finish);
        });
}

}  // namespace niratan::viewmodels
