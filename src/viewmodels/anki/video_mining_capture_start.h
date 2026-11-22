// SPDX-License-Identifier: GPL-3.0-or-later
//
// Module-private: builds one VideoMiningCapture with the hooks the calling
// view model supplies, without starting it, so the caller can store it before
// a start that may complete inline. Kept out of video_mining_capture.h so the
// mining view model and the capture implementation share it without widening
// the public seam.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "viewmodels/anki/video_mining_capture.h"

namespace niratan::viewmodels {

std::unique_ptr<VideoMiningCapture> make_video_capture(
    std::function<void(std::function<void(std::optional<std::string>)>)> resolve_media_directory,
    std::function<void(const QString&)> screenshot_requested);

}  // namespace niratan::viewmodels
