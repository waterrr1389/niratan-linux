// SPDX-License-Identifier: GPL-3.0-or-later
//
// Media file names and markup for card fields, matching Core/AnkiManager.swift
// (safeAnkiMediaFilename, safeMediaExtension, directAudioMarkup,
// directImageMarkup).
//
// safe_media_filename keeps the part after the last '/' and replaces every
// Unicode scalar outside A-Z, a-z, 0-9, '-', '_' and '.' by one '_', so a
// multibyte character becomes a single '_'. A name that is empty after
// trimming '.' and '_' from both ends becomes "hoshi_media"; otherwise the
// result is the replaced name, not the trimmed one. The input is valid UTF-8.
//
// safe_media_extension returns the fallback for an empty extension. Otherwise
// the extension is lowercased and passed through safe_media_filename, and the
// fallback is used when that result is empty or contains '.'. Lowercasing
// follows Unicode.
//
// direct_audio_markup is "[sound:NAME]" and direct_image_markup is
// "<img src=\"NAME\">", where NAME is safe_media_filename of the argument.

#pragma once

#include <string>
#include <string_view>

namespace niratan::anki {

std::string safe_media_filename(std::string_view filename);

std::string safe_media_extension(std::string_view extension, std::string_view fallback);

std::string direct_audio_markup(std::string_view filename);

std::string direct_image_markup(std::string_view filename);

}  // namespace niratan::anki
