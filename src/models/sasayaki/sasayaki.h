// SPDX-License-Identifier: GPL-3.0-or-later
//
// Audiobook data, following Models/Sasayaki.swift (SasayakiCue, SasayakiMatch,
// SasayakiMatchData, SasayakiPlaybackData) and the files Core/BookStorage.swift
// reads and writes next to a book: sasayaki_match.json holds SasayakiMatchData
// and sasayaki_playback.json holds SasayakiPlaybackData.
//
// The match codecs follow synthesized Codable: id, startTime, endTime, text,
// chapterIndex, start and length are required with the type the Swift property
// has, and matches and unmatched are required on the document. contextInferred
// is optional in JSON, absent or null reads as false, and the encoder writes it
// only when it is true, which is the only value SasayakiMatcher produces.
// Integer properties decode only from numbers without a fractional part that
// fit int. A null in a required key, a missing key or a value of another type
// rejects the whole document; extra keys are ignored.
//
// Playback follows SasayakiPlaybackData's custom init(from:): lastPosition is
// required, delay defaults to 0 and rate to 1 when absent or null. audioBookmark
// is Data in Swift and JSONEncoder's default data strategy writes Base64, so in
// JSON it is a Base64 string; it is decoded with QByteArray::fromBase64 and
// written with toBase64, the standard alphabet with padding. Qt's decoder
// ignores characters outside the alphabet instead of rejecting the document as
// Foundation does for malformed Base64. rate is stored as a JSON number and
// narrowed to float, as JSONDecoder does when it decodes a Float.
//
// A document whose root is not an object is rejected. Encoding omits an absent
// optional key rather than writing null, matching synthesized Codable.

#pragma once

#include <QByteArray>
#include <QJsonDocument>

#include <optional>
#include <string>
#include <vector>

namespace niratan::sasayaki {

struct SasayakiCue {
    std::string id;
    double start_time = 0.0;
    double end_time = 0.0;
    std::string text;

    bool operator==(const SasayakiCue&) const = default;
};

struct SasayakiMatch {
    std::string id;
    double start_time = 0.0;
    double end_time = 0.0;
    std::string text;
    int chapter_index = 0;
    int start = 0;
    int length = 0;
    // Set only when text-free interpolation between nearby narration anchors
    // produced the match; an absent JSON key reads as false.
    bool context_inferred = false;

    bool operator==(const SasayakiMatch&) const = default;
};

struct SasayakiMatchData {
    std::vector<SasayakiMatch> matches;
    int unmatched = 0;

    bool operator==(const SasayakiMatchData&) const = default;
};

struct SasayakiPlaybackData {
    double last_position = 0.0;
    double delay = 0.0;
    float rate = 1.0F;
    std::optional<QByteArray> audio_bookmark;

    bool operator==(const SasayakiPlaybackData&) const = default;
};

// sasayaki_match.json
std::optional<SasayakiMatchData> match_data_from_json(const QJsonDocument& document);
QJsonDocument match_data_to_json(const SasayakiMatchData& data);

// sasayaki_playback.json
std::optional<SasayakiPlaybackData> playback_data_from_json(const QJsonDocument& document);
QJsonDocument playback_data_to_json(const SasayakiPlaybackData& data);

}  // namespace niratan::sasayaki
