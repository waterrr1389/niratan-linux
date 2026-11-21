// SPDX-License-Identifier: GPL-3.0-or-later
//
// The mining globals both popup hosts set before popup.js runs, following
// Core/AnkiManager.swift (needsAudio, allowDupes, useAnkiConnect, embedMedia,
// compactGlossaries), Features/Dictionary/DictionarySearchView.swift
// (buildPopupPayload) and Features/Popup/PopupView.swift (buildContent).
//
// needsAudio is true when a field mapping value is exactly "{audio}".
// {sasayaki-audio}, {video-audio} and other spellings do not count, and the
// comparison ignores no whitespace. allowDupes, embedMedia and
// compactGlossaries are the active profile's Anki settings. useAnkiConnect is
// Swift's computed constant true, so it is never false.
//
// Swift's activateProfile also autofills field mappings for a new profile;
// that belongs to profile activation, not to this module. The five values come
// from one profile snapshot; callers reload the settings before computing them
// again after a profile change.

#pragma once

#include "services/anki/anki_config.h"

namespace niratan::anki {

struct PopupOptions {
    bool needs_audio = false;
    bool allow_dupes = false;
    bool use_anki_connect = true;
    bool embed_media = false;
    bool compact_glossaries = false;

    bool operator==(const PopupOptions&) const = default;
};

PopupOptions popup_options(const AnkiSettings& settings);

}  // namespace niratan::anki
