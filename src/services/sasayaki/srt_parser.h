// SPDX-License-Identifier: GPL-3.0-or-later
//
// Subtitle handling for Sasayaki, following Features/Sasayaki/SasayakiParser.swift
// (parseCues, parseTimestamp) and Features/Sasayaki/SasayakiSRT.swift (encode,
// timestamp).
//
// parse_cues() takes the subtitle file's bytes as UTF-8, decodes them the way
// String(decoding:as: UTF8.self) does (an invalid sequence becomes U+FFFD),
// removes every U+FEFF, and normalizes CRLF and then CR to LF. A blank line
// that holds only spaces or tabs is collapsed to one blank line, and the text
// is split into blocks on the resulting empty line, keeping empty blocks.
//
// Each block accepts its first line containing "-->" as the timing line, which
// must have exactly one "-->" and a line after it. Each side is read from its
// first whitespace-separated token, with every comma read as a dot, and must
// have exactly three colon-separated parts: hours, minutes and seconds parsed
// as decimal numbers, minutes and seconds non-negative and below 60, hours
// non-negative, and the sum finite. The block is dropped when the end is not
// greater than the start. The text is every line after the timing line, each
// trimmed of spaces and tabs, joined with one space, then trimmed of spaces and
// newlines; a block whose text is empty or absent is dropped. Malformed blocks
// are skipped rather than reported.
//
// A cue's id is the decimal index of its block in the whole split, before any
// block is dropped, so a rejected or empty block consumes an index and the
// surviving ids can have gaps. Lines before the timing line, such as subtitle
// numbers, are ignored.
//
// Whitespace follows Foundation: trimming a line removes U+0009 and the
// characters in category Zs, and the final trim also removes U+000A..U+000D,
// U+0085, U+2028 and U+2029. Swift also accepts hexadecimal float spellings for
// timestamps, which never occur in a subtitle file; only decimal spellings with
// an optional sign and exponent are read here.
//
// encode_cues() writes SRT for Sasayaki's importer: cues whose timestamps are
// finite, whose start is non-negative and whose end is greater than the start
// are kept, stably sorted by start. Ties keep their input order, which Swift
// leaves unspecified. Cue text is split on newline characters and joined with
// one space, discarding empty parts. Each timestamp is rounded to the nearest
// millisecond, half away from zero, capped at 359999999 seconds, and written as
// HH:MM:SS,mmm, where HH has at least two digits. Entries are numbered from 1
// in output order; every entry, including the last, ends with a newline, and
// entries are separated by one more newline. Cues whose end is less than a
// millisecond after the start are written with an end of start + 1 ms.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "models/sasayaki/sasayaki.h"

namespace niratan::sasayaki {

std::vector<SasayakiCue> parse_cues(std::string_view text);
// The file's bytes through parse_cues, as SasayakiMatchView reads the picked
// SRT with Data(contentsOf:). None when the file cannot be read.
std::optional<std::vector<SasayakiCue>> read_cue_file(const std::filesystem::path& path);

std::string encode_cues(const std::vector<SasayakiCue>& cues);

}  // namespace niratan::sasayaki
