// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/dictionary/audio_source.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace niratan::dictionary {

namespace {

constexpr std::string_view payload_prefix = "hoshi-audio-source:";

}  // namespace

std::string payload(std::string_view audio_source_id) {
    std::string result{payload_prefix};
    result.append(audio_source_id);
    return result;
}

std::optional<std::string> audio_source_id(std::string_view payload) {
    if (!payload.starts_with(payload_prefix)) {
        return std::nullopt;
    }
    payload.remove_prefix(payload_prefix.size());
    if (payload.empty()) {
        return std::nullopt;
    }
    return std::string{payload};
}

std::optional<int> destination_offset(int source_index, int target_index) {
    if (source_index == target_index) {
        return std::nullopt;
    }
    return target_index > source_index ? target_index + 1 : target_index;
}

std::vector<AudioSource> synchronizing_local_source(
    const std::vector<AudioSource>& sources,
    bool enabled,
    const AudioSource& canonical_source,
    const std::set<std::string>& legacy_urls) {
    std::set<std::string> local_urls = legacy_urls;
    local_urls.insert(canonical_source.url);

    std::optional<std::size_t> existing_index;
    std::vector<AudioSource> synchronized;
    synchronized.reserve(sources.size());
    for (std::size_t index = 0; index < sources.size(); ++index) {
        if (local_urls.contains(sources[index].url)) {
            if (!existing_index) {
                existing_index = index;
            }
            continue;
        }
        synchronized.push_back(sources[index]);
    }

    if (!enabled) {
        return synchronized;
    }

    AudioSource local_source = canonical_source;
    if (existing_index) {
        local_source.is_enabled = sources[*existing_index].is_enabled;
    }
    const std::size_t position =
        std::min(existing_index.value_or(std::size_t{0}), synchronized.size());
    synchronized.insert(synchronized.begin() + static_cast<std::ptrdiff_t>(position),
                       std::move(local_source));
    return synchronized;
}

}  // namespace niratan::dictionary
