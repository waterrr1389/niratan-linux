// SPDX-License-Identifier: GPL-3.0-or-later

#include "field_content.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "glossary_markup.h"
#include "models/anki/mining_context.h"

namespace niratan::anki {
namespace {

// The pattern {.*?} holds no line break, so a '{' only starts a handlebar
// when the next '}' is on the same line.
constexpr char kOpen = '{';
constexpr char kClose = '}';

// The Swift names carry their braces; here the braces are removed first.
constexpr std::string_view kSingleGlossaryPrefix = "single-glossary-";
constexpr std::string_view kBriefSuffix = "-brief";
constexpr std::string_view kNoDictionarySuffix = "-no-dictionary";

// Which glossary clean-up a -brief or -no-dictionary handlebar applies.
enum class Strip { none, glossary_headers, dictionary_name };

std::string lookup(const Strings& values, std::string_view key) {
    const auto it = values.find(key);
    return it == values.end() ? std::string{} : it->second;
}

std::string apply_strip(std::string_view html, Strip strip) {
    switch (strip) {
        case Strip::glossary_headers:
            return strip_glossary_headers(html);
        case Strip::dictionary_name:
            return strip_dictionary_name(html);
        case Strip::none:
            break;
    }
    return std::string(html);
}

// The selected glossary falls back to glossaryFirst only when the key is
// absent, like `dictionary[key] ?? fallback`; a present empty value wins.
std::string selected_glossary(const Strings& content, const Strings& singles, bool fallback) {
    const std::string key = lookup(content, "selectedDictionary");
    const auto it = singles.find(key);
    if (it != singles.end()) {
        return it->second;
    }
    return fallback ? lookup(content, "glossaryFirst") : std::string{};
}

// First non-empty single glossary of the category, in dictionary order.
std::string category_glossary(DictionaryCategory category, const Strings& singles,
                              const std::vector<TermDictionary>& dictionaries) {
    for (const TermDictionary& dictionary : dictionaries) {
        if (dictionary.category != category) {
            continue;
        }
        const auto it = singles.find(dictionary.title);
        if (it != singles.end() && !it->second.empty()) {
            return it->second;
        }
    }
    return {};
}

std::string definition_glossary(DictionaryCategory category, bool fallback,
                                const Strings& singles,
                                const std::vector<TermDictionary>& dictionaries) {
    std::string glossary = category_glossary(category, singles, dictionaries);
    if (!glossary.empty() || !fallback) {
        return glossary;
    }
    const DictionaryCategory other =
        category == DictionaryCategory::monolingual ? DictionaryCategory::bilingual
                                                    : DictionaryCategory::monolingual;
    return category_glossary(other, singles, dictionaries);
}

// screenshotFilename ?? screenshotURL?.path ?? "": an empty file name is kept,
// only an absent one falls through.
std::string media_value(const std::optional<std::string>& file_name,
                        const std::optional<std::string>& path) {
    if (file_name) {
        return *file_name;
    }
    if (path) {
        return *path;
    }
    return {};
}

std::string video_media(const VideoMiningContext& video, bool audio) {
    return audio ? media_value(video.audio_clip_filename, video.audio_clip_path)
                 : media_value(video.screenshot_filename, video.screenshot_path);
}

// Wraps every occurrence of `matched` in <b></b>, left to right and
// non-overlapping. Swift's replacingOccurrences leaves the text alone when the
// needle is empty, which also keeps the scan from looping.
std::string wrap_matches(std::string_view sentence, std::string_view matched) {
    if (matched.empty()) {
        return std::string(sentence);
    }
    std::string marked;
    marked.reserve(sentence.size() + 7);
    std::size_t pos = 0;
    while (pos <= sentence.size()) {
        const std::size_t hit = sentence.find(matched, pos);
        if (hit == std::string_view::npos) {
            break;
        }
        marked.append(sentence.substr(pos, hit - pos));
        marked.append("<b>");
        marked.append(matched);
        marked.append("</b>");
        pos = hit + matched.size();
    }
    if (pos < sentence.size()) {
        marked.append(sentence.substr(pos));
    }
    return marked;
}

std::string handlebar_value(std::string_view handlebar, const MiningContext& context,
                            const Strings& content, const Strings& singles,
                            const std::vector<TermDictionary>& dictionaries) {
    if (handlebar.size() < 2 || handlebar.front() != kOpen || handlebar.back() != kClose) {
        return {};
    }
    std::string_view name = handlebar.substr(1, handlebar.size() - 2);

    if (name.starts_with(kSingleGlossaryPrefix)) {
        std::string_view dict_name = name.substr(kSingleGlossaryPrefix.size());
        if (dict_name.ends_with(kBriefSuffix)) {
            dict_name.remove_suffix(kBriefSuffix.size());
            return strip_glossary_headers(lookup(singles, dict_name));
        }
        if (dict_name.ends_with(kNoDictionarySuffix)) {
            dict_name.remove_suffix(kNoDictionarySuffix.size());
            return strip_dictionary_name(lookup(singles, dict_name));
        }
        return lookup(singles, dict_name);
    }

    // Unlike every other name, the fallback variants of the selected glossary
    // put the strip suffix before "-fallback".
    if (name == "selected-glossary-brief-fallback") {
        return strip_glossary_headers(selected_glossary(content, singles, true));
    }
    if (name == "selected-glossary-no-dictionary-fallback") {
        return strip_dictionary_name(selected_glossary(content, singles, true));
    }

    // The suffix applies only to the names that have such a variant; the
    // remaining names are matched without it below.
    Strip strip = Strip::none;
    if (name.ends_with(kBriefSuffix)) {
        name.remove_suffix(kBriefSuffix.size());
        strip = Strip::glossary_headers;
    } else if (name.ends_with(kNoDictionarySuffix)) {
        name.remove_suffix(kNoDictionarySuffix.size());
        strip = Strip::dictionary_name;
    }

    if (name == "glossary") {
        return apply_strip(lookup(content, "glossary"), strip);
    }
    if (name == "glossary-first") {
        return apply_strip(lookup(content, "glossaryFirst"), strip);
    }
    if (name == "selected-glossary") {
        return apply_strip(selected_glossary(content, singles, false), strip);
    }
    if (name == "selected-glossary-fallback" && strip == Strip::none) {
        return selected_glossary(content, singles, true);
    }
    if (name == "monolingual-definition") {
        return apply_strip(
            definition_glossary(DictionaryCategory::monolingual, false, singles, dictionaries),
            strip);
    }
    if (name == "monolingual-definition-fallback") {
        return apply_strip(
            definition_glossary(DictionaryCategory::monolingual, true, singles, dictionaries),
            strip);
    }
    if (name == "bilingual-definition") {
        return apply_strip(
            definition_glossary(DictionaryCategory::bilingual, false, singles, dictionaries),
            strip);
    }
    if (name == "bilingual-definition-fallback") {
        return apply_strip(
            definition_glossary(DictionaryCategory::bilingual, true, singles, dictionaries),
            strip);
    }

    if (strip != Strip::none) {
        return {};
    }

    if (name == "expression") {
        return lookup(content, "expression");
    }
    if (name == "reading") {
        return lookup(content, "reading");
    }
    if (name == "furigana-plain") {
        return lookup(content, "furiganaPlain");
    }
    if (name == "audio") {
        return lookup(content, "audio");
    }
    if (name == "popup-selection-text") {
        return lookup(content, "popupSelectionText");
    }
    if (name == "frequencies") {
        return lookup(content, "frequenciesHtml");
    }
    if (name == "frequency-harmonic-rank") {
        return lookup(content, "freqHarmonicRank");
    }
    if (name == "pitch-accent-positions") {
        return lookup(content, "pitchPositions");
    }
    if (name == "pitch-accent-categories") {
        return lookup(content, "pitchCategories");
    }
    if (name == "phonetic-transcriptions") {
        return lookup(content, "phoneticTranscriptions");
    }
    if (name == "sentence") {
        const auto matched = content.find("matched");
        if (matched == content.end()) {
            return context.sentence;
        }
        return wrap_matches(context.sentence, matched->second);
    }
    if (name == "document-title") {
        return context.document_title.value_or(std::string{});
    }

    if (name == "book-cover") {
        return context.video ? video_media(*context.video, false) : context.cover_url;
    }
    if (name == "sasayaki-audio") {
        return context.video ? video_media(*context.video, true) : context.sasayaki_audio_url;
    }

    if (!context.video) {
        return {};
    }
    const VideoMiningContext& video = *context.video;
    if (name == "video-file-name") {
        return video.file_name;
    }
    if (name == "video-timestamp") {
        return video_timestamp(video.cue_start);
    }
    if (name == "video-cue-start") {
        return video_timestamp(video.cue_start);
    }
    if (name == "video-cue-end") {
        return video_timestamp(video.cue_end);
    }
    if (name == "video-subtitle") {
        return video.cue_text;
    }
    if (name == "video-previous-subtitle") {
        return video.previous_cue_text.value_or(std::string{});
    }
    if (name == "video-next-subtitle") {
        return video.next_cue_text.value_or(std::string{});
    }
    if (name == "video-screenshot") {
        return video_media(video, false);
    }
    if (name == "video-audio-clip") {
        return video_media(video, true);
    }
    return {};
}

}  // namespace

std::string video_timestamp(double seconds) {
    // Int((time * 1000).rounded()), then clamped: negative and NaN times are 0.
    const double milliseconds = seconds * 1000.0;
    long long total = 0;
    if (milliseconds > 0.0) {
        total = std::llround(milliseconds);
    }
    const long long hours = total / 3'600'000;
    const long long minutes = (total / 60'000) % 60;
    const long long remainder_seconds = (total / 1000) % 60;
    const long long remainder_milliseconds = total % 1000;

    char formatted[64];
    std::snprintf(formatted, sizeof formatted, "%lld:%02lld:%02lld.%03lld", hours, minutes,
                  remainder_seconds, remainder_milliseconds);
    return formatted;
}

std::string render_field(std::string_view field_content, const MiningContext& context,
                         const Strings& content, const Strings& single_glossaries,
                         const std::vector<TermDictionary>& dictionaries) {
    std::string rendered;
    rendered.reserve(field_content.size());
    std::size_t pos = 0;
    while (pos < field_content.size()) {
        if (field_content[pos] != kOpen) {
            rendered.push_back(field_content[pos]);
            ++pos;
            continue;
        }
        std::size_t close = pos + 1;
        while (close < field_content.size() && field_content[close] != kClose &&
               field_content[close] != '\n' && field_content[close] != '\r') {
            ++close;
        }
        if (close == field_content.size() || field_content[close] != kClose) {
            rendered.push_back(field_content[pos]);
            ++pos;
            continue;
        }
        rendered.append(handlebar_value(field_content.substr(pos, close - pos + 1), context,
                                        content, single_glossaries, dictionaries));
        pos = close + 1;
    }
    return rendered;
}

}  // namespace niratan::anki
