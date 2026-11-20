// SPDX-License-Identifier: GPL-3.0-or-later
//
// One audio:// popup request, following AudioHandler in
// Features/Popup/PopupWebView.swift.
//
// The request URL carries the real audio URL in its first "url" query item,
// percent-decoded once. A missing, empty or unparsable target is rejected.
// The transfer timeout is 5 seconds when the target host is exactly
// "localhost" and 4 seconds otherwise, as the Swift handler does; no other
// local host spelling is treated specially.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace niratan::audio {

struct AudioRequest {
    std::string url;
    int timeout_ms = 0;

    bool operator==(const AudioRequest&) const = default;
};

std::optional<AudioRequest> parse_audio_request(std::string_view request_url);

}  // namespace niratan::audio
