// SPDX-License-Identifier: GPL-3.0-or-later

#include "services/anki/field_content.h"

#include <QTest>

using niratan::anki::DictionaryCategory;
using niratan::anki::MiningContext;
using niratan::anki::Strings;
using niratan::anki::TermDictionary;
using niratan::anki::VideoMiningContext;
using niratan::anki::render_field;
using niratan::anki::video_timestamp;

namespace {

std::string render(std::string_view field, const MiningContext& context = {},
                   const Strings& content = {}, const Strings& singles = {},
                   const std::vector<TermDictionary>& dictionaries = {}) {
    return render_field(field, context, content, singles, dictionaries);
}

const std::string kHeader = R"q(<li data-dictionary="A"><i>(n, A)</i> x)q";

VideoMiningContext video() {
    VideoMiningContext v;
    v.file_name = "ep1.mkv";
    v.cue_text = "こんにちは";
    v.cue_start = 3725.5;
    v.cue_end = 3727;
    v.previous_cue_text = "prev";
    v.screenshot_filename = "s.png";
    v.audio_clip_filename = "a.m4a";
    return v;
}

}  // namespace

// Core/AnkiManager.swift (handlebarToValue and the handlebar pattern) has no
// test script of its own, so the cases follow its branches directly.
class FieldContentTest : public QObject {
    Q_OBJECT

   private slots:
    void text_without_handlebars_is_unchanged();
    void simple_values_come_from_content();
    void missing_values_and_unknown_handlebars_are_empty();
    void a_handlebar_ends_at_the_first_closing_brace();
    void an_unclosed_or_broken_brace_is_plain_text();
    void glossary_variants_strip_the_header();
    void single_glossary_by_dictionary_name();
    void selected_glossary_and_its_fallback();
    void selected_glossary_variants_use_the_swift_names();
    void category_glossary_skips_empty_and_follows_order();
    void category_fallback_tries_the_other_category_once();
    void sentence_marks_every_matched_occurrence();
    void document_title_and_cover_and_sasayaki();
    void video_values();
    void video_screenshot_falls_back_to_the_path();
    void video_handlebars_are_empty_without_a_video();
    void timestamps_round_to_milliseconds();
};

void FieldContentTest::text_without_handlebars_is_unchanged() {
    QCOMPARE(render("abc 決定"), std::string("abc 決定"));
    QCOMPARE(render(""), std::string());
}

void FieldContentTest::simple_values_come_from_content() {
    const Strings content = {{"expression", "決定"},       {"reading", "けってい"},
                             {"furiganaPlain", "決定[けってい]"}, {"frequenciesHtml", "f"},
                             {"freqHarmonicRank", "7"},    {"pitchPositions", "p"},
                             {"pitchCategories", "c"},     {"phoneticTranscriptions", "t"},
                             {"popupSelectionText", "s"},  {"audio", "u"}};
    QCOMPARE(render("{expression}・{reading}", {}, content), std::string("決定・けってい"));
    QCOMPARE(render("{furigana-plain}", {}, content), std::string("決定[けってい]"));
    QCOMPARE(render("{frequencies}{frequency-harmonic-rank}", {}, content), std::string("f7"));
    QCOMPARE(render("{pitch-accent-positions}{pitch-accent-categories}", {}, content),
             std::string("pc"));
    QCOMPARE(render("{phonetic-transcriptions}{popup-selection-text}{audio}", {}, content),
             std::string("tsu"));
}

void FieldContentTest::missing_values_and_unknown_handlebars_are_empty() {
    QCOMPARE(render("[{reading}]"), std::string("[]"));
    QCOMPARE(render("a{nope}b"), std::string("ab"));
    QCOMPARE(render("a{}b"), std::string("ab"));
}

void FieldContentTest::a_handlebar_ends_at_the_first_closing_brace() {
    const Strings content = {{"expression", "E"}, {"reading", "R"}};
    QCOMPARE(render("{expression} {reading}", {}, content), std::string("E R"));
    QCOMPARE(render("x{{expression}y", {}, content), std::string("xy"));
    QCOMPARE(render("{expression}}", {}, content), std::string("E}"));
}

void FieldContentTest::an_unclosed_or_broken_brace_is_plain_text() {
    const Strings content = {{"expression", "E"}};
    QCOMPARE(render("{expression", {}, content), std::string("{expression"));
    QCOMPARE(render("{expression\n}", {}, content), std::string("{expression\n}"));
    QCOMPARE(render("{ {expression}", {}, content), std::string(""));
}

void FieldContentTest::glossary_variants_strip_the_header() {
    const Strings content = {{"glossary", kHeader}, {"glossaryFirst", kHeader}};
    const std::string brief = R"q(<li data-dictionary="A">x)q";
    const std::string bare = R"q(<li data-dictionary="A"><i>(n)</i> x)q";
    QCOMPARE(render("{glossary}", {}, content), kHeader);
    QCOMPARE(render("{glossary-brief}", {}, content), brief);
    QCOMPARE(render("{glossary-no-dictionary}", {}, content), bare);
    QCOMPARE(render("{glossary-first}", {}, content), kHeader);
    QCOMPARE(render("{glossary-first-brief}", {}, content), brief);
    QCOMPARE(render("{glossary-first-no-dictionary}", {}, content), bare);
}

void FieldContentTest::single_glossary_by_dictionary_name() {
    const Strings singles = {{"A", kHeader}, {"B", "b"}};
    QCOMPARE(render("{single-glossary-B}", {}, {}, singles), std::string("b"));
    QCOMPARE(render("{single-glossary-A-brief}", {}, {}, singles),
             std::string(R"q(<li data-dictionary="A">x)q"));
    QCOMPARE(render("{single-glossary-A-no-dictionary}", {}, {}, singles),
             std::string(R"q(<li data-dictionary="A"><i>(n)</i> x)q"));
    QCOMPARE(render("[{single-glossary-Z}]", {}, {}, singles), std::string("[]"));
    QCOMPARE(render("[{single-glossary-B-brief}]", {}, {}, singles), std::string("[b]"));
}

void FieldContentTest::selected_glossary_and_its_fallback() {
    const Strings singles = {{"A", "a"}, {"B", "b"}, {"Z", ""}};
    Strings content = {{"selectedDictionary", "B"}, {"glossaryFirst", "first"}};
    QCOMPARE(render("{selected-glossary}", {}, content, singles), std::string("b"));
    QCOMPARE(render("{selected-glossary-fallback}", {}, content, singles), std::string("b"));

    content["selectedDictionary"] = "Q";
    QCOMPARE(render("{selected-glossary}", {}, content, singles), std::string());
    QCOMPARE(render("{selected-glossary-fallback}", {}, content, singles), std::string("first"));

    content["selectedDictionary"] = "Z";
    QCOMPARE(render("{selected-glossary-fallback}", {}, content, singles), std::string());

    content.erase("selectedDictionary");
    QCOMPARE(render("{selected-glossary-fallback}", {}, content, singles), std::string("first"));
}

// Models/Anki.swift spells these as selected-glossary-brief-fallback and
// selected-glossary-no-dictionary-fallback; the reverse order is not a name.
void FieldContentTest::selected_glossary_variants_use_the_swift_names() {
    const Strings singles = {{"A", kHeader}};
    const std::string brief = R"q(<li data-dictionary="A">x)q";
    const std::string bare = R"q(<li data-dictionary="A"><i>(n)</i> x)q";
    Strings content = {{"selectedDictionary", "A"}, {"glossaryFirst", kHeader}};
    QCOMPARE(render("{selected-glossary-brief}", {}, content, singles), brief);
    QCOMPARE(render("{selected-glossary-no-dictionary}", {}, content, singles), bare);
    QCOMPARE(render("{selected-glossary-brief-fallback}", {}, content, singles), brief);
    QCOMPARE(render("{selected-glossary-no-dictionary-fallback}", {}, content, singles), bare);
    QCOMPARE(render("[{selected-glossary-fallback-brief}]", {}, content, singles),
             std::string("[]"));
    QCOMPARE(render("[{selected-glossary-fallback-no-dictionary}]", {}, content, singles),
             std::string("[]"));

    content["selectedDictionary"] = "Q";
    QCOMPARE(render("[{selected-glossary-brief}]", {}, content, singles), std::string("[]"));
    QCOMPARE(render("{selected-glossary-brief-fallback}", {}, content, singles), brief);
    QCOMPARE(render("{selected-glossary-no-dictionary-fallback}", {}, content, singles), bare);
}

void FieldContentTest::category_glossary_skips_empty_and_follows_order() {
    const std::vector<TermDictionary> dictionaries = {
        {"M1", DictionaryCategory::monolingual},
        {"X", DictionaryCategory::exclude},
        {"B1", DictionaryCategory::bilingual},
        {"M2", DictionaryCategory::monolingual},
        {"M3", DictionaryCategory::monolingual}};
    const Strings singles = {{"M1", ""}, {"X", "x"}, {"B1", "bil"}, {"M2", "mono2"}, {"M3", "mono3"}};
    QCOMPARE(render("{monolingual-definition}", {}, {}, singles, dictionaries), std::string("mono2"));
    QCOMPARE(render("{bilingual-definition}", {}, {}, singles, dictionaries), std::string("bil"));
    QCOMPARE(render("[{monolingual-definition-brief}]", {}, {}, singles, dictionaries),
             std::string("[mono2]"));
    QCOMPARE(render("[{bilingual-definition-no-dictionary}]", {}, {}, singles, dictionaries),
             std::string("[bil]"));
}

void FieldContentTest::category_fallback_tries_the_other_category_once() {
    const std::vector<TermDictionary> dictionaries = {{"M1", DictionaryCategory::monolingual},
                                                      {"B1", DictionaryCategory::bilingual}};
    const Strings only_bilingual = {{"B1", "bil"}};
    QCOMPARE(render("{monolingual-definition}", {}, {}, only_bilingual, dictionaries), std::string());
    QCOMPARE(render("{monolingual-definition-fallback}", {}, {}, only_bilingual, dictionaries),
             std::string("bil"));
    const Strings only_monolingual = {{"M1", "mono"}};
    QCOMPARE(render("{bilingual-definition-fallback}", {}, {}, only_monolingual, dictionaries),
             std::string("mono"));
    QCOMPARE(render("{bilingual-definition-fallback-brief}", {}, {}, {}, dictionaries), std::string());
}

void FieldContentTest::sentence_marks_every_matched_occurrence() {
    MiningContext context;
    context.sentence = "決定を決定する";
    QCOMPARE(render("{sentence}", context), std::string("決定を決定する"));
    QCOMPARE(render("{sentence}", context, {{"matched", "決定"}}),
             std::string("<b>決定</b>を<b>決定</b>する"));
    QCOMPARE(render("{sentence}", context, {{"matched", ""}}), std::string("決定を決定する"));
    QCOMPARE(render("{sentence}", context, {{"matched", "無"}}), std::string("決定を決定する"));
}

void FieldContentTest::document_title_and_cover_and_sasayaki() {
    MiningContext context;
    QCOMPARE(render("[{document-title}]", context), std::string("[]"));
    context.document_title = "吾輩は猫である";
    context.cover_url = "http://localhost:1/cover/cover.png";
    context.sasayaki_audio_url = "http://localhost:1/sasayaki/audio.m4a";
    QCOMPARE(render("{document-title}", context), std::string("吾輩は猫である"));
    QCOMPARE(render("{book-cover}", context), context.cover_url);
    QCOMPARE(render("{sasayaki-audio}", context), context.sasayaki_audio_url);
}

void FieldContentTest::video_values() {
    MiningContext context;
    context.cover_url = "cover";
    context.sasayaki_audio_url = "audio";
    context.video = video();
    QCOMPARE(render("{video-file-name}", context), std::string("ep1.mkv"));
    QCOMPARE(render("{video-timestamp}", context), std::string("1:02:05.500"));
    QCOMPARE(render("{video-cue-start}", context), std::string("1:02:05.500"));
    QCOMPARE(render("{video-cue-end}", context), std::string("1:02:07.000"));
    QCOMPARE(render("{video-subtitle}", context), std::string("こんにちは"));
    QCOMPARE(render("{video-previous-subtitle}", context), std::string("prev"));
    QCOMPARE(render("[{video-next-subtitle}]", context), std::string("[]"));
    QCOMPARE(render("{video-screenshot}", context), std::string("s.png"));
    QCOMPARE(render("{video-audio-clip}", context), std::string("a.m4a"));
    QCOMPARE(render("{book-cover}", context), std::string("s.png"));
    QCOMPARE(render("{sasayaki-audio}", context), std::string("a.m4a"));
}

void FieldContentTest::video_screenshot_falls_back_to_the_path() {
    MiningContext context;
    VideoMiningContext v = video();
    v.screenshot_filename.reset();
    v.audio_clip_filename.reset();
    v.screenshot_path = "/tmp/s.png";
    context.video = v;
    QCOMPARE(render("{video-screenshot}", context), std::string("/tmp/s.png"));
    QCOMPARE(render("[{video-audio-clip}]", context), std::string("[]"));
    QCOMPARE(render("{book-cover}", context), std::string("/tmp/s.png"));
}

void FieldContentTest::video_handlebars_are_empty_without_a_video() {
    QCOMPARE(render("[{video-file-name}{video-timestamp}{video-subtitle}{video-screenshot}]"),
             std::string("[]"));
}

void FieldContentTest::timestamps_round_to_milliseconds() {
    QCOMPARE(video_timestamp(0), std::string("0:00:00.000"));
    QCOMPARE(video_timestamp(-4), std::string("0:00:00.000"));
    QCOMPARE(video_timestamp(59.9996), std::string("0:01:00.000"));
    QCOMPARE(video_timestamp(0.0005), std::string("0:00:00.001"));
    QCOMPARE(video_timestamp(36000.25), std::string("10:00:00.250"));
}

QTEST_APPLESS_MAIN(FieldContentTest)
#include "tst_field_content.moc"
