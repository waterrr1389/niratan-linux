// SPDX-License-Identifier: GPL-3.0-or-later
//
// The video capture PopupView.mineEntry runs between preflight and the add,
// following Features/Popup/PopupView.swift (mineEntry, miningContextProvider),
// Features/Video/VideoPlayerScreen.swift (popupView: the needs flags, the
// media directory only when a capture is needed, and the video parameters),
// Features/Video/VideoMiningCoordinator.swift (context: the direct-media
// branch, names, claims, the screenshot and clip, and the fallback when no
// media directory is usable), Features/Video/VideoMiningMediaStore.swift and
// +Audio.swift (temporary paths, directMediaURL, claim/wait/finish, replace,
// preparedScreenshot, preparedAudioClip), and Core/AnkiManager.swift
// (needsVideoScreenshot, needsVideoAudioClip, compressImages,
// imageCompressionFormat, imageCompressionQuality, audioCompressionFormat,
// audioCompressionBitrateKbps).
//
// One capture is active at a time; start runs the media-directory lookup,
// then the requested media. Capture callbacks complete on the calling thread
// and may complete inline, so the hooks and the screenshot answer may reenter
// start. The object must outlive a pending capture.
//
// The deterministic names come from services/player/video_mining: the
// identity is the video path, the screenshot range is the cue range, the
// audio range is the padded clip range (or the cue range when the range is
// invalid), and the screenshot format is the JPEG still of Decision D3. A
// still with compressImages off is a temporary .png and with it on a
// temporary .jpg; an AVIF request is also a .jpg.
//
// With a usable media directory the capture claims the deterministic
// destination; a destination that already exists, or that another capture
// owns, is waited for instead of captured again. A claimed still is requested
// through screenshot_requested, moved into place by replace and reported to
// waiters by finish. A claimed clip is exported to a temporary stem and moved
// the same way. Without a usable media directory the capture writes the
// still and the clip to temporary paths and reports those paths for upload.
//
// Recorded differences: the still and the clip run one after the other where
// Swift runs them concurrently and suspends video thumbnails (there are none
// in the port); the card sees the same names and messages. imageCompression
// quality reaches the deterministic name but not the still, which mpv writes
// directly (Decision D3). Temporary-directory naming and the 24-hour sweep
// are not reproduced.

#pragma once

#include <QObject>
#include <QString>

#include <functional>
#include <optional>
#include <string>

#include "models/anki/mining_context.h"
#include "services/anki/anki_config.h"
#include "services/player/video_mining.h"
#include "services/player/video_mining_store.h"

namespace niratan::viewmodels {

// Runs the capture and reports the finished VideoMiningContext. Hooks answer
// with the Anki media directory path as the services expose it.
struct VideoMiningCaptureRequest {
    QString video_path;
    QString title;
    int audio_stream_index = -1;
    player::MiningSelection selection;
    double subtitle_delay = 0;
    double duration = 0;
    bool capture_screenshot = false;
    bool capture_audio_clip = false;
    bool compress_screenshot = true;
    anki::ImageCompressionFormat image_format = anki::ImageCompressionFormat::jpeg;
    double screenshot_quality = 0.80;
    anki::AudioCompressionFormat audio_format = anki::AudioCompressionFormat::aac;
    int audio_bitrate_kbps = 64;
    int animated_avif_maximum_height = 350;
    int animated_avif_frames_per_second = 10;
    QString ffmpeg_path;
};

class VideoMiningCapture : public QObject {
   public:
    struct Hooks {
        // MiningService::media_directory, as a path string.
        std::function<void(std::function<void(std::optional<std::string>)>)>
            resolve_media_directory;
        // The page writes the still at this path and calls screenshot_finished.
        std::function<void(const QString&)> screenshot_requested;
    };

    explicit VideoMiningCapture(Hooks hooks, QObject* parent = nullptr);
    ~VideoMiningCapture() override;

    // Reports the finished context exactly once.
    void start(const VideoMiningCaptureRequest& request,
               std::function<void(anki::MiningContext)> callback);

    void screenshot_finished(bool ok);

   private:
    void begin(std::optional<std::string> directory);
    void start_screenshot();
    void start_audio();
    void screenshot_done(bool ok);
    void audio_exported(const QString& path, const QString& error);
    void audio_waited(bool ready);
    void settle();
    void finish();

    Hooks hooks_;
    VideoMiningCaptureRequest request_;
    std::function<void(anki::MiningContext)> callback_;
    std::optional<player::DirectMediaStore> store_;
    std::optional<player::AudioClipRange> audio_range_;
    QString screenshot_temp_;
    QString audio_temp_stem_;
    std::string screenshot_name_;
    std::string audio_name_;
    std::string screenshot_destination_;
    std::string audio_destination_;
    std::optional<std::string> screenshot_path_;
    std::optional<std::string> audio_path_;
    bool owns_screenshot_ = false;
    bool owns_audio_ = false;
    bool waiting_screenshot_ = false;
    bool waiting_audio_ = false;
    bool completed_ = false;
    int remaining_ = 0;
};

}  // namespace niratan::viewmodels
