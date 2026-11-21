// SPDX-License-Identifier: GPL-3.0-or-later
//
// Matcher cases follow script/test_sasayaki_text_matcher.swift. Its legacy
// sidecar case belongs to the Sasayaki model codec tests.

#include "services/reader/character_normalizer.h"
#include "services/sasayaki/matcher.h"

#include <QString>
#include <QtTest>

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace niratan::sasayaki {
namespace {

const std::string first = "朝になって旅人たちは静かな森の奥へ出発した。";
const std::string second = "私は小さな灯りを頼りに暗い廊下を歩いていた。";
const std::string third = "窓の外には雪に覆われた山々がどこまでも続いていた。";

SasayakiCue cue(std::string_view id, std::string_view text, double start, double end) {
    return {std::string(id), start, end, std::string(text)};
}

MatchChapter chapter(int index, std::string_view markup) {
    return {index, reader::filtered_text(markup)};
}

std::string repeated(std::string_view text, int count) {
    std::string result;
    for (int index = 0; index < count; ++index) {
        result += text;
    }
    return result;
}

std::vector<std::string> ids(const SasayakiMatchData& result) {
    std::vector<std::string> values;
    for (const auto& match : result.matches) {
        values.push_back(match.id);
    }
    return values;
}

}  // namespace

class MatcherTest : public QObject {
    Q_OBJECT

private slots:
    void recovers_after_introduction_and_omitted_passage();
    void folds_width_and_case_in_reader_coordinates();
    void folds_katakana_to_hiragana();
    void leaves_unrelated_cues_unmatched();
    void rejects_repeated_global_exact_passages();
    void recovers_a_global_approximate_passage();
    void confirms_global_jump_with_following_narration();
    void rejects_a_jump_to_the_colophon();
    void does_not_match_across_chapters();
    void infers_a_bounded_gap_and_marks_it();
    void aligns_spelling_variations_between_anchors();
    void does_not_infer_across_chapters();
    void rejects_gaps_without_context();
    void does_not_infer_unbounded_cues();
    void skips_invalid_cues();
    void limits_short_cues_to_the_local_window();
    void splits_legacy_cues_at_sentences();
    void hard_splits_and_merges_a_short_tail();
    void counts_unmatched_split_pieces();
    void supports_cancellation();
    void handles_empty_inputs();
};

void MatcherTest::recovers_after_introduction_and_omitted_passage() {
    const std::string source = repeated("序文の説明がここにあります", 30) + first + second
        + repeated("音声では省略されている挿話です", 40) + third;
    const auto book = chapter(3, source);
    const auto result = match_cues({
        cue("intro", "音声版の制作会社からのお知らせです", 0, 3),
        cue("first", first, 0, 3),
        cue("second", "私は小さな明かりを頼りに暗い廊下を歩いていた。", 4, 7),
        cue("third", third, 8, 11),
    }, {book}, 50);

    const std::vector<std::string> expected{"first", "second", "third"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.unmatched, 1);
    const auto& fuzzy = result.matches[1];
    const auto text = QString::fromStdString(book.text).toUcs4();
    QCOMPARE(text.mid(fuzzy.start, fuzzy.length),
             QString::fromStdString(reader::filtered_text(second)).toUcs4());
    for (const auto& match : result.matches) {
        QCOMPARE(match.chapter_index, 3);
    }
}

void MatcherTest::folds_width_and_case_in_reader_coordinates() {
    const auto result = match_cues({
        cue("hello", "Hello world", 0, 3),
        cue("alice", "Alice crosses the unusually quiet street", 4, 7),
    }, {chapter(0, "<body><ruby>HELLO<rt>reading</rt></ruby> ＷＯＲＬＤ."
                   " Alice crossed the unusually quiet street.</body>")}, 50);

    const std::vector<std::string> expected{"hello", "alice"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.matches.front().start, 0);
    QCOMPARE(result.matches.front().length, 10);
}

void MatcherTest::folds_katakana_to_hiragana() {
    const auto result = match_cues({
        cue("kana", "どらごん\u3000かたかな", 2, 5),
    }, {chapter(9, "序章ドラゴンｶﾀｶﾅ")}, 2);

    const std::vector<std::string> expected{"kana"};
    QCOMPARE(ids(result), expected);
    const auto& match = result.matches.front();
    QCOMPARE(match.chapter_index, 9);
    QCOMPARE(match.start, 2);
    QCOMPARE(match.length, 8);
    QCOMPARE(match.text, std::string("どらごん\u3000かたかな"));
    QVERIFY(!match.context_inferred);
}

void MatcherTest::leaves_unrelated_cues_unmatched() {
    const auto result = match_cues({
        cue("wrong", "On the moon a machine counted seventeen different colors.", 0, 3),
        cue("short", "そうです", 4, 7),
    }, {chapter(0, repeated("序文の説明がここにあります", 30) + first + second
                       + repeated("音声では省略されている挿話です", 40) + third)}, 50);

    QVERIFY(result.matches.empty());
    QCOMPARE(result.unmatched, 2);
}

void MatcherTest::rejects_repeated_global_exact_passages() {
    const auto result = match_cues({cue("repeated", first, 0, 3)},
        {chapter(0, repeated("前置き", 100) + first + repeated("別の場面", 100) + first)}, 50);

    QVERIFY(result.matches.empty());
    QCOMPARE(result.unmatched, 1);
}

void MatcherTest::recovers_a_global_approximate_passage() {
    const auto result = match_cues({
        cue("anchor", "窓の外には霧に覆われた山々がどこまでも続いていた。", 0, 3),
    }, {chapter(0, repeated("前置き", 200) + third)}, 50);

    const std::vector<std::string> expected{"anchor"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.matches.front().start, 600);
    QCOMPARE(result.matches.front().length, reader::readable_character_count(third));
}

void MatcherTest::confirms_global_jump_with_following_narration() {
    const std::string prefix(70, 'z');
    const std::string anchor = "abcdefghijkl";
    const std::vector<SasayakiCue> cues{
        cue("jump", anchor, 0, 3),
        cue("first-confirmation", "mnopqr", 4, 7),
        cue("second-confirmation", "stuvwxyzab", 8, 11),
    };
    const auto confirmed = match_cues(cues, {chapter(0, prefix + anchor + "mnopqrstuvwxyzab")}, 10);
    const std::vector<std::string> expected{"jump", "first-confirmation", "second-confirmation"};
    QCOMPARE(ids(confirmed), expected);
    QCOMPARE(confirmed.matches.front().start, 70);

    const auto too_far = match_cues(cues,
        {chapter(0, prefix + anchor + "mnopqr" + std::string(590, 'z') + "stuvwxyzab")}, 10);
    QVERIFY(too_far.matches.empty());
    QCOMPARE(too_far.unmatched, 3);
}

void MatcherTest::rejects_a_jump_to_the_colophon() {
    const auto preface = repeated("前書きの説明です", 40);
    const std::string publisher = "この音声作品は北の星出版の制作でお届けします";
    const auto result = match_cues({
        cue("publisher", publisher, 0, 3),
        cue("body-one", first, 4, 7),
        cue("body-two", second, 8, 11),
    }, {chapter(2, preface + first + second), chapter(10, publisher)}, 50);

    const std::vector<std::string> expected{"body-one", "body-two"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.unmatched, 1);
    QCOMPARE(result.matches.front().start, reader::readable_character_count(preface));
    for (const auto& match : result.matches) {
        QCOMPARE(match.chapter_index, 2);
    }
}

void MatcherTest::does_not_match_across_chapters() {
    const auto result = match_cues({
        cue("crossing", "森の奥には誰も知らない古い遺跡が残されていた", 0, 3),
    }, {chapter(1, "森の奥には誰も知らない"), chapter(2, "古い遺跡が残されていた")}, 50);

    QVERIFY(result.matches.empty());
    QCOMPARE(result.unmatched, 1);
}

void MatcherTest::infers_a_bounded_gap_and_marks_it() {
    const auto result = match_cues({
        cue("left", first, 0, 3),
        cue("reading", "たぶん", 3, 4),
        cue("right", second, 4, 7),
    }, {chapter(4, first + "多分。" + second)}, 50);

    const std::vector<std::string> expected{"left", "reading", "right"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.unmatched, 0);
    const auto left_length = reader::readable_character_count(first);
    const auto right_length = reader::readable_character_count(second);
    const auto& left = result.matches[0];
    const auto& inferred = result.matches[1];
    const auto& right = result.matches[2];
    QCOMPARE(left.start, 0);
    QCOMPARE(left.length, left_length);
    QCOMPARE(inferred.start, left_length);
    QCOMPARE(inferred.length, 2);
    QCOMPARE(right.start, left_length + 2);
    QCOMPARE(right.length, right_length);
    QCOMPARE(left.start_time, 0.0);
    QCOMPARE(left.end_time, 3.0);
    QCOMPARE(inferred.start_time, 3.0);
    QCOMPARE(inferred.end_time, 4.0);
    QCOMPARE(right.start_time, 4.0);
    QCOMPARE(right.end_time, 7.0);
    QVERIFY(!left.context_inferred);
    QVERIFY(inferred.context_inferred);
    QVERIFY(!right.context_inferred);
}

void MatcherTest::aligns_spelling_variations_between_anchors() {
    const std::string source = first + "今日は学校へ行く。意外と近い。ドラゴン。" + second;
    const auto result = match_cues({
        cue("left", first, 0, 3),
        cue("kana", "きょうは学校へ行く", 3, 5),
        cue("homophone", "以外と近い", 5, 7),
        cue("katakana", "どらごん", 7, 8),
        cue("right", second, 8, 11),
    }, {chapter(4, source)}, 50);

    const std::vector<std::string> expected{"left", "kana", "homophone", "katakana", "right"};
    QCOMPARE(ids(result), expected);
    for (std::size_t index = 1; index < result.matches.size(); ++index) {
        const auto& previous = result.matches[index - 1];
        QVERIFY(previous.start + previous.length <= result.matches[index].start);
        QVERIFY(!result.matches[index].context_inferred);
    }
    QCOMPARE(result.matches.back().start,
             reader::readable_character_count(source) - reader::readable_character_count(second));
}

void MatcherTest::does_not_infer_across_chapters() {
    const auto result = match_cues({
        cue("left", first, 0, 3),
        cue("gap", "たぶん", 3, 4),
        cue("right", second, 4, 7),
    }, {chapter(1, first + "多分。"), chapter(2, "然うか。" + second)}, 50);

    const std::vector<std::string> expected{"left", "right"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.unmatched, 1);
    QCOMPARE(result.matches.back().chapter_index, 2);
    QCOMPARE(result.matches.back().start, 3);
}

void MatcherTest::rejects_gaps_without_context() {
    struct Scenario {
        std::string source_gap;
        SasayakiCue left;
        SasayakiCue gap;
        SasayakiCue right;
    };
    const std::vector<Scenario> scenarios{
        {"夕暮れの街で旅人は一息ついた。", cue("left", first, 0, 3),
         cue("advert", "This is an advertisement.", 3, 5), cue("right", second, 5, 8)},
        {repeated("資料記録書類参考一覧", 10), cue("left", first, 0, 3),
         cue("omitted", "たぶん", 3, 4), cue("right", second, 4, 7)},
        {"多分。", cue("left", first, 0, 3),
         cue("interruption", "たぶん", 30, 31), cue("right", second, 31, 34)},
        {"多分。", cue("left", first, 10, 13),
         cue("unordered", "たぶん", 0, 1), cue("right", second, 14, 17)},
    };
    for (const auto& scenario : scenarios) {
        const auto result = match_cues({scenario.left, scenario.gap, scenario.right},
            {chapter(0, first + scenario.source_gap + second)}, 50);
        const std::vector<std::string> expected{"left", "right"};
        QCOMPARE(ids(result), expected);
        QCOMPARE(result.unmatched, 1);
    }
}

void MatcherTest::does_not_infer_unbounded_cues() {
    const auto result = match_cues({
        cue("head", "たぶん", 0, 1),
        cue("left", first, 1, 4),
        cue("right", second, 4, 7),
        cue("tail", "そうか", 7, 8),
    }, {chapter(0, "多分。" + first + second + "然うか。")}, 50);

    const std::vector<std::string> expected{"left", "right"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.unmatched, 2);
}

void MatcherTest::skips_invalid_cues() {
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto result = match_cues({
        cue("anchor", "anchor", 0, 3),
        cue("empty", "", 3, 4),
        cue("no-readable-text", "！？ \u3000", 4, 5),
        cue("infinite-start", "target", -infinity, 6),
        cue("infinite-end", "target", 5, infinity),
        cue("nan-start", "target", nan, 7),
        cue("nan-end", "target", 6, nan),
        cue("negative", "target", -1, 8),
        cue("zero-duration", "target", 8, 8),
        cue("reversed", "target", 9, 8),
        cue("short-marker", "＊echo", 9, 10),
        cue("five-character-marker", "＊hello", 10, 11),
        cue("valid", "target", 11, 12),
    }, {chapter(0, "anchorechohellotarget")}, 50);

    const std::vector<std::string> expected{"anchor", "five-character-marker", "valid"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.unmatched, 10);
}

void MatcherTest::limits_short_cues_to_the_local_window() {
    const std::vector<SasayakiCue> cues{cue("anchor", "anchor", 0, 3), cue("short", "echo", 3, 4)};
    const auto at_cap = match_cues(cues, {chapter(0, "anchor" + std::string(30, 'z') + "echo")}, 500);
    const std::vector<std::string> both{"anchor", "short"};
    QCOMPARE(ids(at_cap), both);
    QCOMPARE(at_cap.matches.back().start, 36);

    const auto beyond_cap = match_cues(cues, {chapter(0, "anchor" + std::string(31, 'z') + "echo")}, 500);
    const std::vector<std::string> anchor_only{"anchor"};
    QCOMPARE(ids(beyond_cap), anchor_only);
    QCOMPARE(beyond_cap.unmatched, 1);

    const auto negative_window = match_cues(cues, {chapter(0, "anchorzecho")}, -10);
    QCOMPARE(ids(negative_window), anchor_only);
    const auto adjacent = match_cues(cues, {chapter(0, "anchorecho")}, -10);
    QCOMPARE(ids(adjacent), both);
    const auto unanchored = match_cues({cue("short", "echo", 0, 1)}, {chapter(0, "echo")}, 50);
    QVERIFY(unanchored.matches.empty());
    QCOMPARE(unanchored.unmatched, 1);
}

void MatcherTest::splits_legacy_cues_at_sentences() {
    const std::string tail = third + "村の広場には色とりどりの商品が並び大勢の人が集まっていた。"
        + "遠くの時計塔から正午を知らせる鐘の音がゆっくりと聞こえてきた。"
        + "旅人は鞄から古い地図を取り出して次に向かう場所を確認した。";
    const auto result = match_cues({
        cue("long", first + "私は小さな明かりを頼りに暗い廊下を歩いていた。" + tail, 0, 90),
    }, {chapter(0, first + second + tail)}, 50);

    const std::vector<std::string> expected{
        "long-part-0", "long-part-1", "long-part-2", "long-part-3", "long-part-4", "long-part-5",
    };
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.unmatched, 0);
    QCOMPARE(result.matches.front().start_time, 0.0);
    QCOMPARE(result.matches.back().end_time, 90.0);
    QCOMPARE(result.matches.front().text, first);
    for (std::size_t index = 1; index < result.matches.size(); ++index) {
        const auto& previous = result.matches[index - 1];
        const auto& current = result.matches[index];
        QVERIFY(previous.end_time <= current.start_time);
        QVERIFY(previous.start + previous.length <= current.start);
    }
}

void MatcherTest::hard_splits_and_merges_a_short_tail() {
    const std::string head(100, 'a');
    const std::string tail = std::string(22, 'b') + "!ccccccc";
    const auto text = head + "  " + tail;
    const auto result = match_cues({cue("long", text, 10, 142)}, {chapter(0, text)}, 0);

    const std::vector<std::string> expected{"long-part-0", "long-part-1"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.unmatched, 0);
    QCOMPARE(result.matches[0].text, head);
    QCOMPARE(result.matches[0].start_time, 10.0);
    QCOMPARE(result.matches[0].end_time, 110.0);
    QCOMPARE(result.matches[0].length, 100);
    QCOMPARE(result.matches[1].text, tail);
    QCOMPARE(result.matches[1].start_time, 110.0);
    QCOMPARE(result.matches[1].end_time, 142.0);
    QCOMPARE(result.matches[1].start, 100);
    QCOMPARE(result.matches[1].length, 29);
}

void MatcherTest::counts_unmatched_split_pieces() {
    const std::string head(100, 'a');
    const auto result = match_cues({
        cue("long", head + std::string(100, 'b') + std::string(8, 'c'), 0, 208),
    }, {chapter(0, head)}, 0);

    const std::vector<std::string> expected{"long-part-0"};
    QCOMPARE(ids(result), expected);
    QCOMPARE(result.unmatched, 2);
}

void MatcherTest::supports_cancellation() {
    int polls = 0;
    const auto cancelled = [&polls] {
        ++polls;
        return true;
    };
    const auto source = chapter(0, "anchor");
    const auto result = match_cues({
        cue("anchor", "anchor", 0, 3),
        cue("long", std::string(200, 'z'), 3, 203),
    }, {source}, 50, cancelled);

    QVERIFY(polls > 0);
    QVERIFY(result.matches.empty());
    QCOMPARE(result.unmatched, 3);
}

void MatcherTest::handles_empty_inputs() {
    const auto empty = match_cues({}, {}, 50);
    QVERIFY(empty.matches.empty());
    QCOMPARE(empty.unmatched, 0);

    const auto no_cues = match_cues({}, {chapter(0, "anchor")}, 50);
    QVERIFY(no_cues.matches.empty());
    QCOMPARE(no_cues.unmatched, 0);

    const auto no_chapters = match_cues({cue("anchor", "anchor", 0, 3)}, {}, 50);
    QVERIFY(no_chapters.matches.empty());
    QCOMPARE(no_chapters.unmatched, 1);
}

}  // namespace niratan::sasayaki

QTEST_APPLESS_MAIN(niratan::sasayaki::MatcherTest)
#include "tst_matcher.moc"
