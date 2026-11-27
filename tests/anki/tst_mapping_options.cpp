// SPDX-License-Identifier: GPL-3.0-or-later
//
// Features/Settings/AnkiView.swift (availableHandlebars), Models/Anki.swift
// (Handlebars.allCases, singleGlossaryPrefix).

#include "services/anki/mapping_options.h"

#include <QTest>

using niratan::anki::DictionaryCategory;
using niratan::anki::mapping_options;

class MappingOptionsTest : public QObject {
    Q_OBJECT

   private slots:
    void visible_handlebars_and_literal_dictionary_titles_in_order();
};

// VM17: AnkiView.availableHandlebars, Handlebars.allCases and singleGlossaryPrefix.
void MappingOptionsTest::visible_handlebars_and_literal_dictionary_titles_in_order() {
    const std::vector<std::string> expected{
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
        "{single-glossary-国語 辞典}",
        "{single-glossary-B {literal}}",
        "{single-glossary-国語 辞典}",
    };
    QCOMPARE(mapping_options({{"国語 辞典", DictionaryCategory::monolingual},
                              {"B {literal}", DictionaryCategory::exclude},
                              {"国語 辞典", DictionaryCategory::bilingual}}),
             expected);
}

QTEST_APPLESS_MAIN(MappingOptionsTest)
#include "tst_mapping_options.moc"
