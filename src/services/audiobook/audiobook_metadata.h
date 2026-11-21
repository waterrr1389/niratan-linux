// SPDX-License-Identifier: GPL-3.0-or-later
//
// M4B metadata and chapters, following Features/Sasayaki/
// SasayakiAudiobookMetadata.swift (SasayakiAudiobookMetadataLoader,
// SasayakiMP4MetadataParser, SasayakiMP4ChapterParser, MP4MetadataReader,
// MP4ChapterReader). Decision D2 in notes/specs/sasayaki-gap.md applies: Linux
// has no AVFoundation, so loadMetadata and loadChapters are the iTunes box and
// chpl fallbacks alone; there is no commonMetadata and no native chapter group.
//
// read_audiobook_metadata() reads moov/udta/meta/ilst, or moov/meta/ilst when
// udta has no meta; the first four payload bytes of meta (version and flags)
// are skipped. The items ©nam, ©ART, aART and covr supply title, artist, album
// artist and artwork; the artist falls back to the album artist. Text is UTF-8
// with invalid sequences replaced by U+FFFD and trimmed of Unicode whitespace
// and newlines, and an empty result is absent. For each key the first item with
// a non-empty value wins, so an earlier empty item does not block a later one.
// Artwork is handed on as stored. The function never reports failure: a missing
// or unreadable file, no moov or no item list yields empty metadata, matching
// the Swift parser that returns .empty when reading throws.
//
// read_audiobook_chapters() reads moov/udta/chpl, or moov/chpl when udta has no
// chpl. A chpl entry is a big-endian 100 ns start, a one-byte title length and
// that many UTF-8 title bytes. Entries are returned sorted by start time with
// the post-sort index as id; an empty title becomes "Chapter N" with the
// post-sort index + 1. A chapter's end is the next chapter's start and the last
// chapter's end is the mvhd duration when that is not earlier than its start.
// A chpl block that ends early keeps the entries read before the cut. The
// function also never reports failure, but an mvhd that cannot supply the
// duration of its version fails the whole read, matching the Swift parser that
// returns an empty list when reading throws.
//
// Box reading follows MP4Box: a 32-bit size of 0 runs to the parent's end, size
// 1 reads a 64-bit largesize after the type (16-byte header), any other value
// is the size (8-byte header). A size below the header or past the parent's end
// is malformed and ends the parent scan; a box that starts at the parent's end
// is not read. A read past the end of the file fails the whole parse.
//
// current_audiobook_chapter_id() follows SasayakiPlayer.currentAudiobookChapterID:
// the last chapter in vector order whose start is at most the time and whose
// end admits it. A chapter without an end admits any later time; the last
// element (the chapter whose id equals the last chapter's id) admits a time
// equal to its end, every other chapter requires the time to be strictly
// before its end. nullopt when no chapter admits the time or the list is empty.

#pragma once

#include <QString>

#include <optional>
#include <vector>

#include "models/sasayaki/sasayaki.h"

namespace niratan::audiobook {

niratan::sasayaki::SasayakiAudiobookMetadata read_audiobook_metadata(const QString& path);

std::vector<niratan::sasayaki::SasayakiAudiobookChapter>
read_audiobook_chapters(const QString& path);

std::optional<int> current_audiobook_chapter_id(
    const std::vector<niratan::sasayaki::SasayakiAudiobookChapter>& chapters, double current_time);

}  // namespace niratan::audiobook
