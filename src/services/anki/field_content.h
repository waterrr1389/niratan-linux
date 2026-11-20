// SPDX-License-Identifier: GPL-3.0-or-later
//
// Replaces the handlebars of one card field mapping with their values,
// matching Core/AnkiManager.swift (handlebarRegex, handlebarToValue,
// categoryGlossary) and VideoMiningContext.value(for:) in Models/Anki.swift.
//
// Handlebars are found with the pattern {.*?}: from each '{' the shortest run
// up to the next '}' that holds no line break. A '{' without such a '}' is
// plain text. Matches do not overlap, so "{{a}" is one handlebar named
// "{{a}". Text outside matches is copied unchanged. A handlebar with no value
// source, known or not, becomes an empty string.
//
// Values: content keys come from miningContent in Features/Popup/popup.js.
//   {expression} expression, {reading} reading, {furigana-plain}
//   furiganaPlain, {glossary} glossary, {glossary-first} glossaryFirst,
//   {frequencies} frequenciesHtml, {frequency-harmonic-rank} freqHarmonicRank,
//   {pitch-accent-positions} pitchPositions, {pitch-accent-categories}
//   pitchCategories, {phonetic-transcriptions} phoneticTranscriptions,
//   {popup-selection-text} popupSelectionText, {audio} audio. A missing key
//   is empty.
//   - The -brief variants of {glossary} and {glossary-first} apply
//     strip_glossary_headers, the -no-dictionary variants strip_dictionary_name.
//   - {sentence} is the context sentence with every occurrence of content
//     "matched" wrapped in <b></b>. Without "matched", or with an empty one,
//     it is the sentence unchanged.
//   - {document-title} is the title, or empty.
//   - {single-glossary-NAME} is single_glossaries[NAME]. NAME ending in
//     "-brief" or "-no-dictionary" looks up the name without that suffix and
//     applies the matching strip; "-brief" is tested first. Missing is empty.
//   - {selected-glossary} is single_glossaries[content selectedDictionary],
//     with the key "" when selectedDictionary is missing. The -fallback
//     variants use content glossaryFirst (or empty) only when the key is
//     absent; a present empty glossary stays empty. The names are
//     {selected-glossary}, {selected-glossary-fallback},
//     {selected-glossary-brief}, {selected-glossary-brief-fallback},
//     {selected-glossary-no-dictionary} and
//     {selected-glossary-no-dictionary-fallback}: brief strips headers and
//     no-dictionary strips the dictionary name. "-fallback" is never followed
//     by a strip suffix, so {selected-glossary-fallback-brief} is empty.
//   - {monolingual-definition}, {bilingual-definition} and their fallback
//     variants take the first dictionary of that category, in the order given,
//     whose single glossary exists and is not empty. The fallback variants try
//     the other category when none does, once. -brief and -no-dictionary
//     strip the result.
//   - {book-cover} is the video screenshot value for a video, otherwise
//     cover_url. {sasayaki-audio} is the video audio clip value for a video,
//     otherwise sasayaki_audio_url.
//   - {video-file-name}, {video-timestamp}, {video-cue-start},
//     {video-cue-end}, {video-subtitle}, {video-previous-subtitle},
//     {video-next-subtitle}, {video-screenshot}, {video-audio-clip} are empty
//     without a video. Timestamps are video_timestamp of the cue start or end.
//     The screenshot and audio clip value is the file name, else the path,
//     else empty.
//
// video_timestamp formats h:mm:ss.mmm from seconds rounded to whole
// milliseconds, half away from zero, and clamps negatives to zero.

#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "models/anki/mining_context.h"

namespace niratan::anki {

using Strings = std::map<std::string, std::string, std::less<>>;

std::string video_timestamp(double seconds);

std::string render_field(std::string_view field_content, const MiningContext& context,
                         const Strings& content, const Strings& single_glossaries,
                         const std::vector<TermDictionary>& dictionaries);

}  // namespace niratan::anki
