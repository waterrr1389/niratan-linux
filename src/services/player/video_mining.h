// SPDX-License-Identifier: GPL-3.0-or-later
//
// Video mining values that never touch mpv: the cue's audio clip range, the
// deterministic Anki media names, the clicked cue's mining selection and the
// video mining context the card builder reads. Follows
// Features/Video/VideoAudioClipRange.swift (VideoAudioClipRange.resolve),
// Models/Anki.swift (VideoMiningContext.deterministicMediaFilenames for both
// the videoURL and identityKey overloads, millisecondRange, milliseconds,
// sha1Hex, VideoMiningMediaFilenames, VideoScreenshotFormat,
// AnkiAudioCompressionFormat.fileExtension, AnkiImageCompressionFormat and
// AnkiAnimatedAVIFConfiguration),
// Features/Video/VideoMiningSelectionResolution.swift (resolve with a nil
// selectedContext), Features/Video/VideoMiningCoordinator.swift (context:
// screenshotFormat, the VideoMiningContext fields and the two capture error
// strings) and Models/Subtitle.swift (SubtitleCue).
//
// audio_clip_range pads a cue by subtitleDelay and padding: the start is
// max(0, cueStart + subtitleDelay - padding), the end is cueEnd +
// subtitleDelay + padding clamped to duration only when duration > 0, and a
// range whose end is not greater than its start is absent. The padding
// defaults to 0.12 s.
//
// deterministic_media_filenames builds
//   hoshi_video_frame_<sha1>_<startMs>-<endMs><tokens>.<ext>,
//   hoshi_video_audio_<sha1>_<startMs>-<endMs><tokens>.<ext>,
// with <sha1> the lowercase hex SHA-1 of identity_key's UTF-8 bytes and a
// millisecond value max(0, round(time * 1000)) in decimal. The screenshot
// range is the cue range, or for AVIF the screenshotStart/screenshotEnd
// override with each absent field falling back to the cue field. The quality
// percent is round(clamp(quality, 0.40, 0.95) * 100). PNG has no quality
// token; JPEG has "_q<P>" unless P is 80. AVIF always starts with "_avif5",
// then "_q<P>" unless P is 80, then "_h<H>" when the clamped maximum height is
// not 350 and "_fps<F>" when the clamped frame rate is not 10. The audio
// bitrate is clamped to 32...192 and adds "_<B>k" unless it is 64. Extensions
// are png/jpg/avif and m4a/mp3. The same request always gives the same names.
//
// screenshot_format is png unless compress is set; a compressed JPEG or AVIF
// request is a JPEG still, because an AVIF still is out of scope (Decision
// D3). The name function still builds Swift's AVIF tokens.
//
// resolve_mining_selection takes the first cue whose id equals the clicked
// cue's id, uses that cue's text and timing, and takes the previous and next
// text from its neighbours in the list. An id absent from the list, an empty
// list and either end leave the neighbour absent. Duplicate ids use the first
// occurrence for the position. This is the no-selection branch of Swift's
// resolve; the mining-context selection sheet is out of scope (Decision D4).
//
// build_video_mining_context copies the selection into sentence and the
// video's cue fields, sets documentTitle and the video file name to the video
// title, and passes each captured file name and path through unchanged. A
// requested capture with neither a file name nor a path carries Swift's
// "Unable to capture the video screenshot." or "Unable to capture the
// subtitle audio clip."; an unrequested capture stays silently empty. An
// invalid clip range reaches the card as a requested capture with no file
// and so gets the audio message too.
//
// Recorded differences:
//   - identity_key is hashed verbatim; the caller passes
//     VideoMediaIdentity.persistenceKey, whose local-file form is the
//     standardized path, and this module does not standardize it again.
//   - A non-finite or out-of-int64 time, where Swift's Int() traps, yields a
//     clamped millisecond count instead; normal playback positions never
//     reach it.
//   - The mining-context selection branch (MiningContextSelection,
//     addNext/addPrevious) is not ported (Decision D4).

#pragma once

#include <QList>
#include <QString>

#include <optional>
#include <string>
#include <string_view>

#include "models/anki/media_compression.h"
#include "models/anki/mining_context.h"
#include "services/player/subtitle_document.h"

namespace niratan::player {

struct AudioClipRange {
    double start = 0;
    double end = 0;

    bool operator==(const AudioClipRange&) const = default;
};

std::optional<AudioClipRange> audio_clip_range(double cue_start, double cue_end,
                                               double subtitle_delay, double duration,
                                               double padding = 0.12);

enum class VideoScreenshotFormat { png, jpeg, avif };

// AnkiAnimatedAVIFConfiguration.clampedMaximumHeight and
// clampedFramesPerSecond. An absent value uses 350 and 10.
int clamped_avif_maximum_height(std::optional<int> value);
int clamped_avif_frames_per_second(std::optional<int> value);

// VideoMiningCoordinator.context's screenshotFormat: an uncompressed capture
// stays PNG, and a compressed AVIF request falls back to a JPEG still.
VideoScreenshotFormat screenshot_format(bool compress, anki::ImageCompressionFormat format);

struct MediaFilenameRequest {
    std::string identity_key;
    double cue_start = 0;
    double cue_end = 0;
    double audio_start = 0;
    double audio_end = 0;
    VideoScreenshotFormat screenshot_format = VideoScreenshotFormat::png;
    std::optional<double> screenshot_start;
    std::optional<double> screenshot_end;
    double screenshot_quality = 0.80;
    // Absent means 350 and 10.
    std::optional<int> animated_avif_maximum_height;
    std::optional<int> animated_avif_frames_per_second;
    anki::AudioCompressionFormat audio_format = anki::AudioCompressionFormat::aac;
    int audio_bitrate_kbps = 64;
};

struct MediaFilenames {
    std::string screenshot;
    std::string audio_clip;

    bool operator==(const MediaFilenames&) const = default;
};

MediaFilenames deterministic_media_filenames(const MediaFilenameRequest& request);

struct MiningSelection {
    QString sentence;
    QString cue_text;
    double cue_start = 0;
    double cue_end = 0;
    std::optional<QString> previous_cue_text;
    std::optional<QString> next_cue_text;

    bool operator==(const MiningSelection&) const = default;
};

MiningSelection resolve_mining_selection(const SubtitleCue& cue, const QList<SubtitleCue>& cues);

// What the caller captured for the card. A file name is the direct-media
// name in Anki's collection.media; a path is a temporary file to upload. Both
// may be absent.
struct VideoMediaCapture {
    bool screenshot_requested = false;
    bool audio_clip_requested = false;
    std::optional<std::string> screenshot_filename;
    std::optional<std::string> audio_clip_filename;
    std::optional<std::string> screenshot_path;
    std::optional<std::string> audio_clip_path;
};

anki::MiningContext build_video_mining_context(std::string_view video_title,
                                               const MiningSelection& selection,
                                               const VideoMediaCapture& capture);

}  // namespace niratan::player
