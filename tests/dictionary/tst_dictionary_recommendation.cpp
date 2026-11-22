// SPDX-License-Identifier: GPL-3.0-or-later
//
// The recommended dictionaries for each content language, following
// DictionaryRecommendation.all and forLanguage in Models/Dictionary.swift.

#include "models/dictionary/dictionary_recommendation.h"

#include <QtTest>

#include <optional>
#include <string>
#include <vector>

using niratan::dictionary::dictionary_recommendations;
using niratan::dictionary::dictionary_recommendations_for;
using niratan::dictionary::DictionaryRecommendation;
using niratan::dictionary::DictionaryType;
using niratan::profiles::ContentLanguage;

namespace {

DictionaryRecommendation entry(std::string id, std::string name, DictionaryType type,
                               std::optional<std::string> index_url,
                               std::optional<std::string> download_url, ContentLanguage language) {
    return DictionaryRecommendation{std::move(id),        std::move(name),         type,
                                    std::move(index_url), std::move(download_url), language};
}

const std::string kHuggingFace =
    "https://huggingface.co/datasets/daxida/wty-release/resolve/main/latest/index/";

}  // namespace

class DictionaryRecommendationTest : public QObject {
    Q_OBJECT

   private slots:
    void the_table_is_swifts_declaration_order();
    void for_language_keeps_the_table_order();
};

// B1: twelve entries in declaration order with the exact names, types, URLs
// and languages of Models/Dictionary.swift.
void DictionaryRecommendationTest::the_table_is_swifts_declaration_order() {
    const std::vector<DictionaryRecommendation> expected = {
        entry("jmdict", "JMdict", DictionaryType::term,
              "https://github.com/yomidevs/jmdict-yomitan/releases/latest/download/"
              "JMdict_english_without_proper_names.json",
              std::nullopt, ContentLanguage::japanese),
        entry("jmnedict", "JMnedict", DictionaryType::term,
              "https://github.com/yomidevs/jmdict-yomitan/releases/latest/download/JMnedict.json",
              std::nullopt, ContentLanguage::japanese),
        entry("jiten", "Jiten", DictionaryType::frequency,
              "https://api.jiten.moe/api/frequency-list/index", std::nullopt,
              ContentLanguage::japanese),
        entry("jitendex", "Jitendex", DictionaryType::term,
              "https://jitendex.org/static/yomitan.json", std::nullopt, ContentLanguage::japanese),
        entry("kanjidic", "KANJIDIC", DictionaryType::kanji,
              "https://github.com/yomidevs/jmdict-yomitan/releases/latest/download/"
              "KANJIDIC_english.json",
              std::nullopt, ContentLanguage::japanese),
        entry("wty-en-en", "Wiktionary English-English", DictionaryType::term,
              kHuggingFace + "wty-en-en-index.json?download=true", std::nullopt,
              ContentLanguage::english),
        entry("wty-en-en-ipa", "Wiktionary English-English IPA", DictionaryType::pitch,
              kHuggingFace + "wty-en-en-ipa-index.json?download=true", std::nullopt,
              ContentLanguage::english),
        entry("wty-simple-simple", "Wiktionary Simple English-Simple English", DictionaryType::term,
              kHuggingFace + "wty-simple-simple-index.json?download=true", std::nullopt,
              ContentLanguage::english),
        entry("wty-en-ja", "Wiktionary English-Japanese", DictionaryType::term,
              kHuggingFace + "wty-en-ja-index.json?download=true", std::nullopt,
              ContentLanguage::english),
        entry("wty-en-ja-gloss", "Wiktionary English-Japanese Glossary", DictionaryType::term,
              kHuggingFace + "wty-en-ja-gloss-index.json?download=true", std::nullopt,
              ContentLanguage::english),
        entry("leipzig-english-web-rank", "Leipzig English Web", DictionaryType::frequency,
              std::nullopt,
              "https://github.com/StefanVukovic99/leipzig-to-yomitan/releases/latest/download/"
              "Leipzig.English.Web.Rank.zip",
              ContentLanguage::english),
        entry("leipzig-english-wikipedia-rank", "Leipzig English Wikipedia",
              DictionaryType::frequency, std::nullopt,
              "https://github.com/StefanVukovic99/leipzig-to-yomitan/releases/latest/download/"
              "Leipzig.English.Wikipedia.Rank.zip",
              ContentLanguage::english),
    };

    const std::vector<DictionaryRecommendation>& table = dictionary_recommendations();
    QCOMPARE(table.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        QCOMPARE(table.at(index), expected.at(index));
    }
}

// B1: forLanguage filters by the entry's language and keeps table order.
void DictionaryRecommendationTest::for_language_keeps_the_table_order() {
    const std::vector<DictionaryRecommendation> english =
        dictionary_recommendations_for(ContentLanguage::english);
    const std::vector<std::string> english_ids = {"wty-en-en",
                                                  "wty-en-en-ipa",
                                                  "wty-simple-simple",
                                                  "wty-en-ja",
                                                  "wty-en-ja-gloss",
                                                  "leipzig-english-web-rank",
                                                  "leipzig-english-wikipedia-rank"};
    QCOMPARE(english.size(), english_ids.size());
    for (std::size_t index = 0; index < english_ids.size(); ++index) {
        QCOMPARE(english.at(index).id, english_ids.at(index));
        QCOMPARE(english.at(index).language, ContentLanguage::english);
    }

    const std::vector<DictionaryRecommendation> japanese =
        dictionary_recommendations_for(ContentLanguage::japanese);
    const std::vector<std::string> japanese_ids = {"jmdict", "jmnedict", "jiten", "jitendex",
                                                   "kanjidic"};
    QCOMPARE(japanese.size(), japanese_ids.size());
    for (std::size_t index = 0; index < japanese_ids.size(); ++index) {
        QCOMPARE(japanese.at(index).id, japanese_ids.at(index));
        QCOMPARE(japanese.at(index).language, ContentLanguage::japanese);
    }
}

QTEST_APPLESS_MAIN(DictionaryRecommendationTest)
#include "tst_dictionary_recommendation.moc"
