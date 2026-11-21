// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the ASS/SSA plain-text cue path of SubtitleParser, following
// script/test_video_subtitles.swift (the ass, invalidTimestampASS and
// hybridASS fixtures) and the ASSSubtitleTextSanitizer cases of
// script/test_video_embedded_subtitle_payload.swift. Assertions are named
// after the behaviours in notes/specs/player-ass-subtitle.md; the render
// plan, style alignment and effects split are out of scope.

#include "services/player/subtitle_document.h"

#include <QTest>

#include <optional>

namespace niratan::player {
namespace {

QUrl subtitle_url(const QString& name) {
    return QUrl::fromLocalFile("/tmp/" + name);
}

SubtitleParseResult parse(const QByteArray& data, const QString& name,
                          std::optional<SubtitleFormat> hint = std::nullopt) {
    return parse_subtitle_document(data, subtitle_url(name), hint);
}

QByteArray bytes(const QString& text) {
    return text.toUtf8();
}

// One ASS event in the default field order with the given Text value.
QByteArray ass_with_dialogue(const QString& text) {
    return bytes(QStringLiteral("[Events]\n"
                                "Format: Layer, Start, End, Style, Name, MarginL, MarginR, "
                                "MarginV, Effect, Text\n"
                                "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,") +
                 text + QStringLiteral("\n"));
}

std::optional<SubtitleCue> single_ass_cue(const QString& text) {
    const auto result = parse(ass_with_dialogue(text), "sanitize.ass");
    if (!result.document || result.document->cues.size() != 1) {
        return std::nullopt;
    }
    return result.document->cues.front();
}

QStringList texts(const SubtitleDocument& document) {
    QStringList result;
    for (const SubtitleCue& cue : document.cues) {
        result.append(cue.text);
    }
    return result;
}

class AssSubtitleTest : public QObject {
    Q_OBJECT

   private slots:
    void format_addsAssAndSsaWhoseHintWins() {
        const QByteArray ass = bytes("[Events]\n"
                                     "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,星\n");
        const auto lower = parse(ass, "clip.ass");
        QCOMPARE(lower.error, SubtitleParseError::none);
        QVERIFY(lower.document);
        QCOMPARE(lower.document->format, SubtitleFormat::ass);
        QCOMPARE(texts(*lower.document), QStringList{"星"});

        const auto upper = parse(ass, "clip.ASS");
        QVERIFY(upper.document);
        QCOMPARE(upper.document->format, SubtitleFormat::ass);

        const auto ssa = parse(ass, "clip.SSA");
        QVERIFY(ssa.document);
        QCOMPARE(ssa.document->format, SubtitleFormat::ssa);

        const auto hinted = parse(ass, "sample.srt", SubtitleFormat::ass);
        QVERIFY(hinted.document);
        QCOMPARE(hinted.document->format, SubtitleFormat::ass);
        QCOMPARE(texts(*hinted.document), QStringList{"星"});

        const auto other = parse(ass, "clip.txt");
        QVERIFY(!other.document);
        QCOMPARE(other.error, SubtitleParseError::unsupported_format);

        for (const QString& dotless : {QStringLiteral("ass"), QStringLiteral("ssa")}) {
            const auto source = parse(ass, dotless);
            QVERIFY(!source.document);
            QCOMPARE(source.error, SubtitleParseError::unsupported_format);
        }
    }

    void sections_gateDialogueLinesToEvents() {
        const QByteArray data =
            bytes("Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,before-any-section\n"
                  "[Script Info]\n"
                  "Dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,in-info\n"
                  "[V4+ Styles]\n"
                  "Dialogue: 0,0:00:05.00,0:00:06.00,Default,,0,0,0,,in-styles\n"
                  "[EVENTS]\n"
                  "Dialogue: 0,0:00:07.00,0:00:08.00,Default,,0,0,0,,first-events\n"
                  "[Script Info]\n"
                  "Dialogue: 0,0:00:09.00,0:00:10.00,Default,,0,0,0,,back-in-info\n"
                  "[Events]\n"
                  "Dialogue: 0,0:00:11.00,0:00:12.00,Default,,0,0,0,,second-events\n");
        const auto result = parse(data, "sections.ass");
        QVERIFY(result.document);
        QCOMPARE(texts(*result.document), (QStringList{"first-events", "second-events"}));

        const auto indented = parse(bytes("  [Events]  \n"
                                          "  Format: Layer, Start, End, Style, Name, MarginL, "
                                          "MarginR, MarginV, Effect, Text\n"
                                          "  Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,"
                                          "indented\n"),
                                    "indent.ass");
        QVERIFY(indented.document);
        QCOMPARE(texts(*indented.document), QStringList{"indented"});

        const auto cr_only = parse(bytes("[Events]\r"
                                         "Format: Layer, Start, End, Style, Name, MarginL, "
                                         "MarginR, MarginV, Effect, Text\r"
                                         "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,"
                                         "cr-only\r"),
                                   "cr.ass");
        QVERIFY(cr_only.document);
        QCOMPARE(texts(*cr_only.document), QStringList{"cr-only"});

        const auto blank_run = parse(bytes("[Events]\n"
                                           "Format: Layer, Start, End, Style, Name, MarginL, "
                                           "MarginR, MarginV, Effect, Text\n"
                                           "\n"
                                           "\n"
                                           "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,"
                                           "blank\n"),
                                     "blank.ass");
        QVERIFY(blank_run.document);
        QCOMPARE(blank_run.document->cues.front().id, QStringLiteral("ass-5"));
    }

    void dialoguePrefixIsAnchoredAndCaseInsensitive() {
        const QByteArray data = bytes("[Events]\n"
                                      "Comment: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,commented\n"
                                      "Dialogue : 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,spaced\n"
                                      "dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,lower\n"
                                      "note: 0,0:00:05.00,0:00:06.00,Default,,0,0,0,,other\n");
        const auto result = parse(data, "prefix.ass");
        QVERIFY(result.document);
        QCOMPARE(texts(*result.document), QStringList{"lower"});
    }

    void formatLineAdoptsReorderedFieldsAndTextKeepsCommas() {
        const auto reordered = parse(
            bytes("[Events]\n"
                  "Format: Start, Layer, End, Name, Style, MarginV, MarginL, MarginR, Effect, "
                  "Text\n"
                  "Dialogue: 0:00:01.00,0,0:00:03.00,JP,Bottom,24,10,20,,主台詞\n"),
            "reordered.ass");
        QVERIFY(reordered.document);
        QCOMPARE(reordered.document->cues.size(), 1);
        QCOMPARE(reordered.document->cues.front().start_time, 1.0);
        QCOMPARE(reordered.document->cues.front().end_time, 3.0);
        QCOMPARE(reordered.document->cues.front().text, QStringLiteral("主台詞"));

        const auto commas = parse(bytes("[Events]\n"
                                        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, "
                                        "MarginV, Effect, Text\n"
                                        "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,Hello, "
                                        "world, again\n\n"
                                        "Format: Layer, Start, End, Style\n"
                                        "Dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,"
                                        "defaults stay\n\n"
                                        "Dialogue: 0,0:00:05.00,0:00:06.00\n"),
                                  "commas.ass");
        QVERIFY(commas.document);
        QCOMPARE(texts(*commas.document), (QStringList{"Hello, world, again", "defaults stay"}));
        QCOMPARE(commas.document->cues.front().start_time, 1.0);
        QCOMPARE(commas.document->cues.at(1).start_time, 3.0);

        const auto text_middle = parse(bytes("[Events]\n"
                                             "Format: Start, End, Text, Style\n"
                                             "Dialogue: 0:00:01.00,0:00:02.00,Hello, "
                                             "world,Default\n"),
                                       "text-middle.ass");
        QVERIFY(text_middle.document);
        QCOMPARE(text_middle.document->cues.size(), 1);
        QCOMPARE(text_middle.document->cues.front().text, QStringLiteral("Hello, world"));
    }

    void timing_mustParseAndEndAtOrAfterStart() {
        const QByteArray data = bytes(
            "[Events]\n"
            "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
            "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,Valid before\n"
            "Dialogue: 0,0:nan,0:00:03.00,Default,,0,0,0,,Invalid NaN\n"
            "Dialogue: 0,0:00:03.00,0:inf,Default,,0,0,0,,Invalid infinity\n"
            "Dialogue: 0,10000000000000000:00:00.00,10000000000000000:00:01.00,Default,,0,0,0,,"
            "Invalid overflow\n"
            "Dialogue: 0,-1:00:00.00,0:00:04.00,Default,,0,0,0,,Invalid negative\n"
            "Dialogue: 0,0:00:04.00,0:00:05.00,Default,,0,0,0,,Valid after\n"
            "Dialogue: 0,0:00:06.00,0:00:06.00,Default,,0,0,0,,Equal\n"
            "Dialogue: 0,0:00:08.00,0:00:07.00,Default,,0,0,0,,Reversed\n"
            "Dialogue: 0,,0:00:09.00,Default,,0,0,0,,Empty start\n"
            "Dialogue: 0,0:0a:00.00,0:00:10.00,Default,,0,0,0,,Unparsable\n"
            "Dialogue: 0,0:00:11.23,0:00:12.00,Default,,0,0,0,,Precise\n");
        const auto result = parse(data, "timing.ass");
        QVERIFY(result.document);
        QCOMPARE(texts(*result.document),
                 (QStringList{"Valid before", "Valid after", "Equal", "Precise"}));
        QCOMPARE(result.document->cues.at(2).start_time, 6.0);
        QCOMPARE(result.document->cues.at(2).end_time, 6.0);
        QCOMPARE(result.document->cues.at(3).start_time, 11.23);
    }

    void textSanitizer_stripsOverridesAndDrawings() {
        struct Case {
            QString input;
            QString expected;
        };
        const QList<Case> cases = {
            {QStringLiteral("{\\an8}こんにちは\\N世界, with comma"),
             QStringLiteral("こんにちは\n世界, with comma")},
            {QStringLiteral("{\\pos(1,2)}次{\\i1}です"), QStringLiteral("次です")},
            {QStringLiteral("{\\p1}m 0 0 l 10 10{\\p0}文字"), QStringLiteral("文字")},
            {QStringLiteral("{\\p1}m 0 0 l 10 10{\\p0}{\\i1}查词文本{\\i0}"),
             QStringLiteral("查词文本")},
            {QStringLiteral("{\\p1}drop{\\pX}still{\\p0}keep"), QStringLiteral("keep")},
            {QStringLiteral("{\\i1\\p1}draw{\\p0}keep"), QStringLiteral("keep")},
            {QStringLiteral("{\\p-1}text"), QStringLiteral("text")},
            {QStringLiteral("A\\hB"), QStringLiteral("A B")},
            {QStringLiteral("A{B"), QStringLiteral("A{B")},
            {QStringLiteral("A\\NB\\nC\\hD"), QStringLiteral("A\nB\nC D")},
        };
        for (const Case& test : cases) {
            const auto cue = single_ass_cue(test.input);
            QVERIFY2(cue.has_value(), qPrintable(test.input));
            QCOMPARE(cue->text, test.expected);
        }

        for (const QString& input : {QStringLiteral("{\\p1}m 0 0 l 100 0 100 100 0 100{\\p0}"),
                                     QStringLiteral("{\\p1}m 0 0 l 20 20"),
                                     QStringLiteral("{\\i1}{\\b1}"), QStringLiteral("\\h \\h")}) {
            const auto result = parse(ass_with_dialogue(input), "sanitize.ass");
            QVERIFY2(!result.document, qPrintable(input));
            QCOMPARE(result.error, SubtitleParseError::no_valid_cues);
        }
    }

    void cueIds_useNormalisedLineNumbers() {
        const auto numbered = parse(bytes("[Script Info]\n"
                                          "ScriptType: v4.00+\n"
                                          "\n"
                                          "[Events]\n"
                                          "Format: Layer, Start, End, Style, Name, MarginL, "
                                          "MarginR, MarginV, Effect, Text\n"
                                          "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,"
                                          "first\n"),
                                    "ids.ass");
        QVERIFY(numbered.document);
        QCOMPARE(numbered.document->cues.front().id, QStringLiteral("ass-6"));

        const auto fixture = parse(
            bytes("[Script Info]\n"
                  "ScriptType: v4.00+\n"
                  "\n"
                  "[V4+ Styles]\n"
                  "Format: Name, Fontname, Fontsize, PrimaryColour\n"
                  "Style: Default,Arial,20,&H00FFFFFF\n"
                  "\n"
                  "[Events]\n"
                  "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, "
                  "Text\n"
                  "Comment: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,comment should be ignored\n"
                  "Dialogue: 0,0:00:01.23,0:00:03.45,Default,,0,0,0,,{\\an8}こんにちは\\N世界, "
                  "with comma\n"
                  "Dialogue: 0,0:00:04.00,0:00:05.00,Default,,0,0,0,,{\\\\pos(1,2)}次{\\i1}です\n"),
            "sample.ass");
        QVERIFY(fixture.document);
        QCOMPARE(fixture.document->cues.at(0).id, QStringLiteral("ass-11"));
        QCOMPARE(fixture.document->cues.at(1).id, QStringLiteral("ass-12"));
        QCOMPARE(fixture.document->cues.at(0).text, QStringLiteral("こんにちは\n世界, with comma"));
        QCOMPARE(fixture.document->cues.at(1).text, QStringLiteral("次です"));
    }

    void cues_sortByStartThenEnd() {
        const auto result =
            parse(bytes("[Events]\n"
                        "Dialogue: 0,0:00:04.00,0:00:05.00,Default,,0,0,0,,late\n"
                        "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,long\n"
                        "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,short\n"),
                  "sort.ass");
        QVERIFY(result.document);
        QCOMPARE(texts(*result.document), (QStringList{"short", "long", "late"}));
    }

    void noValidCues_coversEmptyAndDroppedDialogue() {
        const auto none = parse(bytes("[Script Info]\nScriptType: v4.00+\n"), "none.ass");
        QVERIFY(!none.document);
        QCOMPARE(none.error, SubtitleParseError::no_valid_cues);

        const auto comments = parse(bytes("[Events]\n"
                                          "Comment: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,"
                                          "metadata\n"),
                                    "comments.ass");
        QVERIFY(!comments.document);
        QCOMPARE(comments.error, SubtitleParseError::no_valid_cues);

        const auto mixed = parse(ass_with_dialogue(QStringLiteral("star")) +
                                     bytes("Dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,"
                                           "{\\p1}m 0 0 l 10 10\n"),
                                 "mixed.ass");
        QVERIFY(mixed.document);
        QCOMPARE(texts(*mixed.document), QStringList{"star"});
    }

    void warnings_stayEmptyForAss() {
        const auto result = parse(bytes("[Events]\n"
                                        "Dialogue: 0,0:nan,0:00:02.00,Default,,0,0,0,,bad "
                                        "timing\n"
                                        "Dialogue: 0,0:00:03.00,0:00:04.00,Default,,0,0,0,,"
                                        "{\\p1}m 0 0 l 10 10{\\p0}\n"
                                        "Dialogue: 0,0:00:05.00,0:00:06.00,Default,,0,0,0,,"
                                        "kept\n"),
                                  "warnings.ass");
        QVERIFY(result.document);
        QVERIFY(result.document->warnings.isEmpty());
        QCOMPARE(texts(*result.document), QStringList{"kept"});
    }
};

}  // namespace
}  // namespace niratan::player

QTEST_APPLESS_MAIN(niratan::player::AssSubtitleTest)
#include "tst_ass_subtitle.moc"
