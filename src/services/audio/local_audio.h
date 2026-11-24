// SPDX-License-Identifier: GPL-3.0-or-later
//
// Local word-audio database lookups, following Core/LocalFileServer.swift
// (getAudioSources, getAudio, mimeType(forAudioFile:), katakanaToHiragana,
// defaultSources, supportedAudioExtensions, localAudioPath),
// Core/LocalAudioEndpoint.swift (port, url) and the local audio source
// constants of Core/UserConfig.swift (localAudioSource, defaultAudioSource,
// legacyLocalAudioURLs). Those three constants are AudioSource values of the
// model layer; this module supplies the endpoint and source-list values they
// embed.
//
// find_local_audio runs the getAudioSources query. The reading is converted to
// hiragana first. With an empty reading only `expression = ?` matches. With a
// non-empty reading, a row matching both expression and reading ranks first,
// then a reading-only match, then an expression-only match, then any other
// row. Inside a rank the default source order from default_sources() wins,
// and any other source comes after all of them (999). Rows whose file does not
// end in a supported audio extension are never returned; the filter compares
// LOWER(file), which SQLite folds for ASCII only, so an uppercase extension is
// accepted. At most one row is returned because the server answers with one
// audio source or with an empty list.
//
// The callers own the I/O. find_local_audio neither opens the database at
// kLocalAudioPath, nor checks that the file exists, nor closes the connection;
// it prepares, binds and steps one statement against a connection whose
// entries(expression, reading, source, file) table already exists. A NULL
// source or file column yields nullopt, where Swift's String(cString:) would
// trap; binding failures are not checked, as in Swift.
//
// katakana_to_hiragana maps every Unicode scalar from U+30A1 to U+30F6 to the
// scalar 0x60 below and leaves every other scalar alone. It decodes UTF-8 and
// re-encodes the mapped scalars, so ordinary multibyte text round-trips.
// Swift's strings are always valid UTF-8; bytes that are not a structurally
// valid UTF-8 sequence are copied through unchanged.
//
// percent_encode_path reproduces addingPercentEncoding(withAllowedCharacters:
// .urlPathAllowed): A-Z, a-z, 0-9 and "-._~!$&'()*+,;=:@/" stay literal, and
// every other UTF-8 byte becomes uppercase %XX. Only the file part of a URL is
// encoded; local_audio_url inserts the source verbatim, as the Swift
// interpolation does.
//
// audio_mime_type reads the extension of the last path component and
// lowercases it. A name without a dot, with a dot as its first character or
// with a trailing dot has no extension. Swift's lowercased() follows Unicode,
// so a non-ASCII scalar that lowercases to an ASCII letter (U+017F to "s")
// would match a supported extension there and not here.

#pragma once

#include <sqlite3.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace niratan::audio {

// Path of the imported AnkiConnect Android audio database inside the app
// directory.
inline constexpr std::string_view kLocalAudioPath = "Audio/android.db";

// Kept away from AnkiConnect's default 8765 port.
inline constexpr std::uint16_t kLocalAudioPort = 18765;

inline constexpr std::string_view kLocalAudioUrlTemplate =
    "http://localhost:18765/localaudio/get/?term={term}&reading={reading}";

// Source names in the order they break ties between otherwise equal rows.
// Any source missing from this list sorts after all of them.
const std::array<std::string_view, 10>& default_sources();

// True when the last path component ends in one of the supported audio
// extensions, compared case-insensitively for ASCII.
bool is_supported_audio_file(std::string_view file);

std::string katakana_to_hiragana(std::string_view text);

std::string percent_encode_path(std::string_view text);

std::string audio_mime_type(std::string_view file);

struct LocalAudioEntry {
    std::string source;
    std::string file;
    bool operator==(const LocalAudioEntry&) const = default;
};

// One row of the local audio table, or nullopt when nothing matches or the
// statement cannot be prepared or stepped.
std::optional<LocalAudioEntry> find_local_audio(sqlite3* db, std::string_view term,
                                                std::string_view raw_reading);

// Full URL of a served file: the listener port, the raw source and the encoded
// file path.
std::string local_audio_url(std::string_view source, std::string_view file,
                            std::uint16_t port = kLocalAudioPort);

}  // namespace niratan::audio
