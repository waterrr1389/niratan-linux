// SPDX-License-Identifier: GPL-3.0-or-later
//
// Audio source values and the drag payloads of the audio source list, matching
// Models/Dictionary.swift (AudioSource, AudioSourceReorder.payload(for:),
// AudioSourceReorder.audioSourceID(from:),
// AudioSourceReorder.destinationOffset(sourceIndex:targetIndex:) and
// AudioSourceReorder.synchronizingLocalSource(_:enabled:canonicalSource:
// legacyURLs:)), and the built-in sources of Core/UserConfig.swift
// (defaultAudioSource, localAudioSource, legacyLocalAudioURLs).
//
// AudioSource mirrors the stored Swift value. Swift's computed id is the url,
// so it has no member of its own.
//
// payload() prefixes the identifier with "hoshi-audio-source:" and validates
// nothing. audio_source_id() returns the text after that prefix and nullopt
// when the prefix is missing or nothing follows it. Everything after the
// prefix is returned unchanged, including empty names, whitespace and text
// that is not valid UTF-8.
//
// destination_offset() answers where a dragged row lands. Moving down inserts
// after the target row (target_index + 1); moving up inserts before it
// (target_index). Dropping on the same row gives nullopt. The indices come
// from a list view and are not validated: negative and out-of-range values run
// the same arithmetic, except that results outside the range of int are not
// reproduced.
//
// synchronizing_local_source() treats legacy_urls plus canonical_source.url as
// the local URLs and compares only urls, never names. The first source, in
// list order, whose url is local decides both the insertion position and the
// enabled state of the copy. Every source whose url is local is dropped,
// including canonical_source itself and duplicates of a legacy url, and the
// rest keep their order. When !enabled that filtered list is the result.
//
// When enabled the copy of canonical_source takes name, url and is_default
// from canonical_source and is_enabled from the first local source, or keeps
// canonical_source.is_enabled when there is none. It is inserted at that
// source's index in the filtered list, or at 0 when no local source exists.
// The entries before the first local source are never local and stay, so the
// index never exceeds the filtered size; Swift's min with that size keeps the
// empty case and a local source at the end well defined, and is mirrored here.

#pragma once

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::dictionary {

struct AudioSource {
    std::string name;
    std::string url;
    bool is_enabled = true;
    bool is_default = false;

    bool operator==(const AudioSource&) const = default;
};

std::string payload(std::string_view audio_source_id);

std::optional<std::string> audio_source_id(std::string_view payload);

std::optional<int> destination_offset(int source_index, int target_index);

std::vector<AudioSource> synchronizing_local_source(
    const std::vector<AudioSource>& sources,
    bool enabled,
    const AudioSource& canonical_source,
    const std::set<std::string>& legacy_urls);

// Core/UserConfig.swift's static let defaultAudioSource and
// static let localAudioSource, and its private static let
// legacyLocalAudioURLs. The URLs keep the {term} and {reading} placeholders.
AudioSource default_audio_source();
AudioSource local_audio_source();
std::vector<std::string> legacy_local_audio_urls();

}  // namespace niratan::dictionary
