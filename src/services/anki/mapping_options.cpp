// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/mapping_options.h"

namespace niratan::anki {

std::vector<std::string> mapping_options(const std::vector<TermDictionary>& dictionaries) {
    // AnkiView.availableHandlebars filters Handlebars.allCases without sorting.
    std::vector<std::string> options{
        "{expression}",
        "{reading}",
        "{furigana-plain}",
        "{audio}",
        "{glossary}",
        "{glossary-brief}",
        "{glossary-first}",
        "{monolingual-definition}",
        "{bilingual-definition}",
        "{monolingual-definition-fallback}",
        "{bilingual-definition-fallback}",
        "{selected-glossary}",
        "{selected-glossary-fallback}",
        "{popup-selection-text}",
        "{sentence}",
        "{frequencies}",
        "{frequency-harmonic-rank}",
        "{pitch-accent-positions}",
        "{pitch-accent-categories}",
        "{phonetic-transcriptions}",
        "{document-title}",
        "{book-cover}",
        "{sasayaki-audio}",
        "{video-file-name}",
        "{video-timestamp}",
        "{video-cue-start}",
        "{video-cue-end}",
        "{video-subtitle}",
        "{video-previous-subtitle}",
        "{video-next-subtitle}",
        "{video-screenshot}",
        "{video-audio-clip}",
    };
    options.reserve(options.size() + dictionaries.size());
    for (const auto& dictionary : dictionaries) {
        options.push_back("{single-glossary-" + dictionary.title + "}");
    }
    return options;
}

}  // namespace niratan::anki
