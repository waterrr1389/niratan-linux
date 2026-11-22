// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_mining.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace niratan::player {

namespace {

qint64 milliseconds(double time) {
    const double value = std::round(time * 1000.0);
    if (!(value > 0.0)) {
        return 0;
    }
    constexpr qint64 maximum = std::numeric_limits<qint64>::max();
    if (value >= static_cast<double>(maximum)) {
        return maximum;
    }
    return static_cast<qint64>(value);
}

std::string millisecond_range(double start, double end) {
    return std::to_string(milliseconds(start)) + "-" + std::to_string(milliseconds(end));
}

int quality_percent(double quality) {
    const double clamped = std::min(0.95, std::max(0.40, quality));
    return static_cast<int>(std::round(clamped * 100.0));
}

std::string screenshot_extension(VideoScreenshotFormat format) {
    switch (format) {
        case VideoScreenshotFormat::png:
            return "png";
        case VideoScreenshotFormat::jpeg:
            return "jpg";
        case VideoScreenshotFormat::avif:
            return "avif";
    }
    return "png";
}

std::string audio_extension(anki::AudioCompressionFormat format) {
    return format == anki::AudioCompressionFormat::aac ? "m4a" : "mp3";
}

std::string sha1_hex(std::string_view value) {
    const QByteArray bytes(value.data(), static_cast<qsizetype>(value.size()));
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex().toStdString();
}

}  // namespace

std::optional<AudioClipRange> audio_clip_range(double cue_start, double cue_end,
                                               double subtitle_delay, double duration,
                                               double padding) {
    const double padded_start = std::max(0.0, cue_start + subtitle_delay - padding);
    double padded_end = cue_end + subtitle_delay + padding;
    if (duration > 0) {
        padded_end = std::min(duration, padded_end);
    }
    if (!(padded_end > padded_start)) {
        return std::nullopt;
    }
    return AudioClipRange{padded_start, padded_end};
}

int clamped_avif_maximum_height(std::optional<int> value) {
    return std::min(1080, std::max(100, value.value_or(350)));
}

int clamped_avif_frames_per_second(std::optional<int> value) {
    return std::min(30, std::max(1, value.value_or(10)));
}

VideoScreenshotFormat screenshot_format(bool compress, anki::ImageCompressionFormat format) {
    // Both compressed stills are JPEG until an AVIF encoder is ported.
    (void)format;
    return compress ? VideoScreenshotFormat::jpeg : VideoScreenshotFormat::png;
}

MediaFilenames deterministic_media_filenames(const MediaFilenameRequest& request) {
    const std::string hash = sha1_hex(request.identity_key);
    const std::string cue_range = millisecond_range(request.cue_start, request.cue_end);
    const std::string screenshot_range = request.screenshot_format == VideoScreenshotFormat::avif
        ? millisecond_range(request.screenshot_start.value_or(request.cue_start),
                            request.screenshot_end.value_or(request.cue_end))
        : cue_range;
    const std::string audio_range = millisecond_range(request.audio_start, request.audio_end);
    const int percent = quality_percent(request.screenshot_quality);
    std::string screenshot_tokens;
    if (request.screenshot_format == VideoScreenshotFormat::avif) {
        const int height = clamped_avif_maximum_height(request.animated_avif_maximum_height);
        const int frames = clamped_avif_frames_per_second(request.animated_avif_frames_per_second);
        screenshot_tokens = "_avif5";
        if (percent != 80) {
            screenshot_tokens += "_q" + std::to_string(percent);
        }
        if (height != 350) {
            screenshot_tokens += "_h" + std::to_string(height);
        }
        if (frames != 10) {
            screenshot_tokens += "_fps" + std::to_string(frames);
        }
    } else if (request.screenshot_format != VideoScreenshotFormat::png && percent != 80) {
        screenshot_tokens = "_q" + std::to_string(percent);
    }
    const int bitrate = std::min(192, std::max(32, request.audio_bitrate_kbps));
    const std::string audio_tokens =
        bitrate == 64 ? std::string{} : "_" + std::to_string(bitrate) + "k";
    return MediaFilenames{"hoshi_video_frame_" + hash + "_" + screenshot_range + screenshot_tokens +
                              "." + screenshot_extension(request.screenshot_format),
                          "hoshi_video_audio_" + hash + "_" + audio_range + audio_tokens + "." +
                              audio_extension(request.audio_format)};
}

MiningSelection resolve_mining_selection(const SubtitleCue& cue, const QList<SubtitleCue>& cues) {
    MiningSelection selection;
    selection.sentence = cue.text;
    selection.cue_text = cue.text;
    selection.cue_start = cue.start_time;
    selection.cue_end = cue.end_time;
    qsizetype index = -1;
    for (qsizetype i = 0; i < cues.size(); ++i) {
        if (cues[i].id == cue.id) {
            index = i;
            break;
        }
    }
    if (index > 0) {
        selection.previous_cue_text = cues[index - 1].text;
    }
    if (index >= 0 && index + 1 < cues.size()) {
        selection.next_cue_text = cues[index + 1].text;
    }
    return selection;
}

anki::MiningContext build_video_mining_context(std::string_view video_title,
                                               const MiningSelection& selection,
                                               const VideoMediaCapture& capture) {
    anki::MiningContext context;
    context.sentence = selection.sentence.toStdString();
    context.document_title = std::string(video_title);
    anki::VideoMiningContext video;
    video.file_name = std::string(video_title);
    video.cue_text = selection.cue_text.toStdString();
    video.cue_start = selection.cue_start;
    video.cue_end = selection.cue_end;
    if (selection.previous_cue_text) {
        video.previous_cue_text = selection.previous_cue_text->toStdString();
    }
    if (selection.next_cue_text) {
        video.next_cue_text = selection.next_cue_text->toStdString();
    }
    video.screenshot_filename = capture.screenshot_filename;
    video.audio_clip_filename = capture.audio_clip_filename;
    video.screenshot_path = capture.screenshot_path;
    video.audio_clip_path = capture.audio_clip_path;
    if (capture.screenshot_requested && !capture.screenshot_filename && !capture.screenshot_path) {
        video.screenshot_error_message =
            QCoreApplication::translate("AnkiMining", "Unable to capture the video screenshot.")
                .toStdString();
    }
    if (capture.audio_clip_requested && !capture.audio_clip_filename && !capture.audio_clip_path) {
        video.audio_clip_error_message =
            QCoreApplication::translate("AnkiMining", "Unable to capture the subtitle audio clip.")
                .toStdString();
    }
    context.video = std::move(video);
    return context;
}

}  // namespace niratan::player
