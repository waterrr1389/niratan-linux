// SPDX-License-Identifier: GPL-3.0-or-later

#include "dictionary_recommendation.h"

#include <utility>

namespace niratan::dictionary {
namespace {

using profiles::ContentLanguage;

DictionaryRecommendation recommendation(std::string id, std::string name, DictionaryType type,
                                        std::optional<std::string> index_url,
                                        std::optional<std::string> download_url,
                                        ContentLanguage language) {
    return DictionaryRecommendation{std::move(id),        std::move(name),         type,
                                    std::move(index_url), std::move(download_url), language};
}

}  // namespace

const std::vector<DictionaryRecommendation>& dictionary_recommendations() {
    static const std::vector<DictionaryRecommendation> table = {
        recommendation("jmdict", "JMdict", DictionaryType::term,
                       "https://github.com/yomidevs/jmdict-yomitan/releases/latest/download/"
                       "JMdict_english_without_proper_names.json",
                       std::nullopt, ContentLanguage::japanese),
        recommendation(
            "jmnedict", "JMnedict", DictionaryType::term,
            "https://github.com/yomidevs/jmdict-yomitan/releases/latest/download/JMnedict.json",
            std::nullopt, ContentLanguage::japanese),
        recommendation("jiten", "Jiten", DictionaryType::frequency,
                       "https://api.jiten.moe/api/frequency-list/index", std::nullopt,
                       ContentLanguage::japanese),
        recommendation("jitendex", "Jitendex", DictionaryType::term,
                       "https://jitendex.org/static/yomitan.json", std::nullopt,
                       ContentLanguage::japanese),
        recommendation("kanjidic", "KANJIDIC", DictionaryType::kanji,
                       "https://github.com/yomidevs/jmdict-yomitan/releases/latest/download/"
                       "KANJIDIC_english.json",
                       std::nullopt, ContentLanguage::japanese),
        recommendation(
            "wty-en-en", "Wiktionary English-English", DictionaryType::term,
            "https://huggingface.co/datasets/daxida/wty-release/resolve/main/latest/index/"
            "wty-en-en-index.json?download=true",
            std::nullopt, ContentLanguage::english),
        recommendation(
            "wty-en-en-ipa", "Wiktionary English-English IPA", DictionaryType::pitch,
            "https://huggingface.co/datasets/daxida/wty-release/resolve/main/latest/index/"
            "wty-en-en-ipa-index.json?download=true",
            std::nullopt, ContentLanguage::english),
        recommendation(
            "wty-simple-simple", "Wiktionary Simple English-Simple English", DictionaryType::term,
            "https://huggingface.co/datasets/daxida/wty-release/resolve/main/latest/index/"
            "wty-simple-simple-index.json?download=true",
            std::nullopt, ContentLanguage::english),
        recommendation(
            "wty-en-ja", "Wiktionary English-Japanese", DictionaryType::term,
            "https://huggingface.co/datasets/daxida/wty-release/resolve/main/latest/index/"
            "wty-en-ja-index.json?download=true",
            std::nullopt, ContentLanguage::english),
        recommendation(
            "wty-en-ja-gloss", "Wiktionary English-Japanese Glossary", DictionaryType::term,
            "https://huggingface.co/datasets/daxida/wty-release/resolve/main/latest/index/"
            "wty-en-ja-gloss-index.json?download=true",
            std::nullopt, ContentLanguage::english),
        recommendation(
            "leipzig-english-web-rank", "Leipzig English Web", DictionaryType::frequency,
            std::nullopt,
            "https://github.com/StefanVukovic99/leipzig-to-yomitan/releases/latest/download/"
            "Leipzig.English.Web.Rank.zip",
            ContentLanguage::english),
        recommendation(
            "leipzig-english-wikipedia-rank", "Leipzig English Wikipedia",
            DictionaryType::frequency, std::nullopt,
            "https://github.com/StefanVukovic99/leipzig-to-yomitan/releases/latest/download/"
            "Leipzig.English.Wikipedia.Rank.zip",
            ContentLanguage::english),
    };
    return table;
}

std::vector<DictionaryRecommendation>
dictionary_recommendations_for(profiles::ContentLanguage language) {
    std::vector<DictionaryRecommendation> result;
    for (const DictionaryRecommendation& entry : dictionary_recommendations()) {
        if (entry.language == language) {
            result.push_back(entry);
        }
    }
    return result;
}

}  // namespace niratan::dictionary
