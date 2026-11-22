// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_mining_capture.h"

#include "video_mining_capture_start.h"

#include <QPointer>
#include <QStandardPaths>
#include <QUuid>

#include <utility>

#include "services/player/audio_clip.h"

namespace niratan::viewmodels {

namespace {

QString temporary_stem() {
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
        QStringLiteral("/hoshi-video-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString screenshot_suffix(player::VideoScreenshotFormat format) {
    return format == player::VideoScreenshotFormat::png ? QStringLiteral(".png")
                                                        : QStringLiteral(".jpg");
}

}  // namespace

VideoMiningCapture::VideoMiningCapture(Hooks hooks, QObject* parent)
    : QObject(parent), hooks_(std::move(hooks)) {}

VideoMiningCapture::~VideoMiningCapture() = default;

void VideoMiningCapture::start(const VideoMiningCaptureRequest& request,
                               std::function<void(anki::MiningContext)> callback) {
    request_ = request;
    callback_ = std::move(callback);
    completed_ = false;
    owns_screenshot_ = false;
    owns_audio_ = false;
    waiting_screenshot_ = false;
    waiting_audio_ = false;
    remaining_ = (request.capture_screenshot ? 1 : 0) + (request.capture_audio_clip ? 1 : 0);
    audio_range_ = player::audio_clip_range(request.selection.cue_start, request.selection.cue_end,
                                            request.subtitle_delay, request.duration);
    const auto format =
        player::screenshot_format(request.compress_screenshot, request.image_format);
    screenshot_temp_ = temporary_stem() + screenshot_suffix(format);
    audio_temp_stem_ = temporary_stem();
    if (remaining_ == 0) {
        finish();
        return;
    }
    hooks_.resolve_media_directory(
        [self = QPointer<VideoMiningCapture>(this)](std::optional<std::string> directory) {
            if (self) {
                self->begin(std::move(directory));
            }
        });
}

void VideoMiningCapture::begin(std::optional<std::string> directory) {
    if (directory) {
        store_.emplace(*directory);
    }
    if (store_) {
        player::MediaFilenameRequest names;
        names.identity_key = request_.video_path.toStdString();
        names.cue_start = request_.selection.cue_start;
        names.cue_end = request_.selection.cue_end;
        names.audio_start = audio_range_ ? audio_range_->start : request_.selection.cue_start;
        names.audio_end = audio_range_ ? audio_range_->end : request_.selection.cue_end;
        names.screenshot_format =
            player::screenshot_format(request_.compress_screenshot, request_.image_format);
        names.screenshot_quality = request_.screenshot_quality;
        names.animated_avif_maximum_height = request_.animated_avif_maximum_height;
        names.animated_avif_frames_per_second = request_.animated_avif_frames_per_second;
        names.audio_format = request_.audio_format;
        names.audio_bitrate_kbps = request_.audio_bitrate_kbps;
        const auto media = player::deterministic_media_filenames(names);
        if (request_.capture_screenshot) {
            screenshot_name_ = media.screenshot;
            screenshot_destination_ = store_->path_for(media.screenshot);
        }
        if (request_.capture_audio_clip && audio_range_) {
            audio_name_ = media.audio_clip;
            audio_destination_ = store_->path_for(media.audio_clip);
        }
    }
    start_screenshot();
    start_audio();
}

void VideoMiningCapture::start_screenshot() {
    if (!request_.capture_screenshot) {
        return;
    }
    waiting_screenshot_ = true;
    if (store_) {
        owns_screenshot_ = store_->claim(screenshot_destination_);
        if (!owns_screenshot_) {
            store_->wait_for(screenshot_destination_,
                             [self = QPointer<VideoMiningCapture>(this)](bool ready) {
                                 if (self) {
                                     self->screenshot_done(ready);
                                 }
                             });
            return;
        }
    }
    hooks_.screenshot_requested(screenshot_temp_);
}

void VideoMiningCapture::start_audio() {
    if (!request_.capture_audio_clip) {
        return;
    }
    if (!audio_range_) {
        --remaining_;
        settle();
        return;
    }
    waiting_audio_ = true;
    if (store_) {
        owns_audio_ = store_->claim(audio_destination_);
        if (!owns_audio_) {
            store_->wait_for(audio_destination_,
                             [self = QPointer<VideoMiningCapture>(this)](bool ready) {
                                 if (self) {
                                     self->audio_waited(ready);
                                 }
                             });
            return;
        }
    }
    auto* exporter = new player::AudioClipExporter(request_.ffmpeg_path, this);
    connect(exporter, &player::AudioClipExporter::finished, this,
            [this](const QString& path, const QString& error) { audio_exported(path, error); });
    player::AudioClipRequest clip;
    clip.source_path = request_.video_path;
    clip.audio_stream_index = request_.audio_stream_index;
    clip.start = audio_range_->start;
    clip.end = audio_range_->end;
    clip.format = request_.audio_format;
    clip.bitrate_kbps = request_.audio_bitrate_kbps;
    clip.output_stem = audio_temp_stem_;
    exporter->start(clip);
}

void VideoMiningCapture::screenshot_finished(bool ok) {
    screenshot_done(ok);
}

void VideoMiningCapture::screenshot_done(bool ok) {
    if (!waiting_screenshot_) {
        return;
    }
    waiting_screenshot_ = false;
    if (store_) {
        bool ready = false;
        if (owns_screenshot_) {
            ready = ok && store_->replace(screenshot_temp_.toStdString(), screenshot_destination_);
            store_->finish(screenshot_destination_, ready);
        } else {
            ready = ok;
        }
        if (!ready) {
            screenshot_name_.clear();
        }
    } else if (ok) {
        screenshot_path_ = screenshot_temp_.toStdString();
    }
    --remaining_;
    settle();
}

void VideoMiningCapture::audio_exported(const QString& path, const QString& error) {
    if (!waiting_audio_) {
        return;
    }
    waiting_audio_ = false;
    if (store_) {
        bool ready = false;
        if (owns_audio_) {
            ready = error.isEmpty() && store_->replace(path.toStdString(), audio_destination_);
            store_->finish(audio_destination_, ready);
        }
        if (!ready) {
            audio_name_.clear();
        }
    } else if (error.isEmpty()) {
        audio_path_ = path.toStdString();
    }
    --remaining_;
    settle();
}

void VideoMiningCapture::audio_waited(bool ready) {
    if (!waiting_audio_) {
        return;
    }
    waiting_audio_ = false;
    if (!ready) {
        audio_name_.clear();
    }
    --remaining_;
    settle();
}

void VideoMiningCapture::settle() {
    if (remaining_ <= 0) {
        finish();
    }
}

void VideoMiningCapture::finish() {
    if (completed_) {
        return;
    }
    completed_ = true;
    player::VideoMediaCapture capture;
    capture.screenshot_requested = request_.capture_screenshot;
    capture.audio_clip_requested = request_.capture_audio_clip;
    if (!screenshot_name_.empty()) {
        capture.screenshot_filename = screenshot_name_;
    }
    if (!audio_name_.empty()) {
        capture.audio_clip_filename = audio_name_;
    }
    capture.screenshot_path = screenshot_path_;
    capture.audio_clip_path = audio_path_;
    auto callback = std::move(callback_);
    callback(player::build_video_mining_context(request_.title.toStdString(), request_.selection,
                                                capture));
}

std::unique_ptr<VideoMiningCapture> make_video_capture(
    std::function<void(std::function<void(std::optional<std::string>)>)> resolve_media_directory,
    std::function<void(const QString&)> screenshot_requested) {
    return std::make_unique<VideoMiningCapture>(VideoMiningCapture::Hooks{
        std::move(resolve_media_directory), std::move(screenshot_requested)});
}

}  // namespace niratan::viewmodels
