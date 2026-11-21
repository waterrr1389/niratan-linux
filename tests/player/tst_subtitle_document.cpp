// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for SubtitleParser SRT/WebVTT parsing and SubtitleCueStore, following
// script/test_video_subtitles.swift and the cue-store cases of
// Features/Video/Subtitles/SubtitleCueStore.swift. Assertions are named after
// the behaviours in notes/specs/player-subtitle-document.md; ASS is the next
// module and is not exercised here.

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

QByteArray utf16_bytes(const QString& text, bool little_endian) {
    QByteArray out;
    out.append(little_endian ? '\xFF' : '\xFE');
    out.append(little_endian ? '\xFE' : '\xFF');
    for (const QChar character : text) {
        const char16_t value = character.unicode();
        if (little_endian) {
            out.append(char(value & 0xFF));
            out.append(char(value >> 8));
        } else {
            out.append(char(value >> 8));
            out.append(char(value & 0xFF));
        }
    }
    return out;
}

std::optional<SubtitleCue> single_cue(const QByteArray& start, const QByteArray& end) {
    const auto result = parse("1\n" + start + " --> " + end + "\nA\n", "stamp.srt");
    if (!result.document || result.document->cues.isEmpty()) {
        return std::nullopt;
    }
    return result.document->cues.front();
}

SubtitleCue cue(const QString& id, double start, double end, const QString& text = {}) {
    return {id, start, end, text};
}

SubtitleDocument document_of(const QList<SubtitleCue>& cues) {
    SubtitleDocument document;
    document.format = SubtitleFormat::srt;
    document.cues = cues;
    return document;
}

QStringList ids(const QList<SubtitleCue>& cues) {
    QStringList result;
    for (const SubtitleCue& entry : cues) {
        result.append(entry.id);
    }
    return result;
}

class SubtitleDocumentTest : public QObject {
    Q_OBJECT

   private slots:
    void format_comesFromLowercasedExtension() {
        const auto srt = parse(bytes("1\n00:00:01,000 --> 00:00:02,000\n星\n"), "clip.SRT");
        QCOMPARE(srt.error, SubtitleParseError::none);
        QVERIFY(srt.document);
        QCOMPARE(srt.document->format, SubtitleFormat::srt);
        QCOMPARE(srt.document->cues.size(), 1);
        QCOMPARE(srt.document->source, subtitle_url("clip.SRT"));

        const auto vtt =
            parse(bytes("WEBVTT\n\n00:00.500 --> 00:02.000\nこんにちは\n"), "clip.vtt");
        QCOMPARE(vtt.error, SubtitleParseError::none);
        QVERIFY(vtt.document);
        QCOMPARE(vtt.document->format, SubtitleFormat::web_vtt);
        QCOMPARE(vtt.document->cues.size(), 1);

        const auto other = parse(bytes("1\n00:00:01,000 --> 00:00:02,000\n星\n"), "clip.txt");
        QVERIFY(!other.document);
        QCOMPARE(other.error, SubtitleParseError::unsupported_format);
    }

    void formatHint_winsOverExtension() {
        const auto as_srt = parse(bytes("1\n00:00:01,000 --> 00:00:02,000\n<i>星</i>\n"),
                                  "sample.vtt", SubtitleFormat::srt);
        QVERIFY(as_srt.document);
        QCOMPARE(as_srt.document->format, SubtitleFormat::srt);
        QCOMPARE(as_srt.document->cues.front().text, QStringLiteral("<i>星</i>"));

        const auto as_vtt = parse(bytes("WEBVTT\n\n00:00.500 --> 00:02.000\n<i>星</i>\n"),
                                  "sample.srt", SubtitleFormat::web_vtt);
        QVERIFY(as_vtt.document);
        QCOMPARE(as_vtt.document->format, SubtitleFormat::web_vtt);
        QCOMPARE(as_vtt.document->cues.front().text, QStringLiteral("星"));
    }

    void decode_removesByteOrderMarksAndReplacesInvalidBytes() {
        const QByteArray srt = bytes("1\n00:00:01,000 --> 00:00:02,000\n星\n");
        QByteArray with_bom;
        with_bom.append("\xEF\xBB\xBF");
        with_bom.append(srt);
        const auto bom = parse(with_bom, "bom.srt");
        QVERIFY(bom.document);
        QCOMPARE(bom.document->cues.front().text, QStringLiteral("星"));

        QByteArray middle_feff = bytes("1\n00:00:01,000 --> 00:00:02,000\nA");
        middle_feff.append("\xEF\xBB\xBF");
        middle_feff.append("B\n");
        const auto feff = parse(middle_feff, "feff.srt");
        QVERIFY(feff.document);
        QCOMPARE(feff.document->cues.front().text, QStringLiteral("AB"));

        QByteArray invalid = bytes("1\n00:00:01,000 --> 00:00:02,000\n");
        invalid.append('\xFF');
        invalid.append('\n');
        const auto replaced = parse(invalid, "bad.srt");
        QVERIFY(replaced.document);
        QCOMPARE(replaced.document->cues.front().text, QString(QChar(0xFFFD)));
    }

    void decode_readsBomMarkedUtf16InEitherOrder() {
        const QString text = QStringLiteral("1\n00:00:01,000 --> 00:00:02,000\n星です\n");
        for (const bool little_endian : {true, false}) {
            const auto result = parse(utf16_bytes(text, little_endian), "utf16.srt");
            QVERIFY(result.document);
            QCOMPARE(result.document->cues.size(), 1);
            QCOMPARE(result.document->cues.front().text, QStringLiteral("星です"));
        }
    }

    void newlines_normaliseBeforeBlockSplit() {
        const QByteArray lf = bytes("1\n00:00:01,000 --> 00:00:02,000\n一行目\n二行目\n\n"
                                    "2\n00:00:03,000 --> 00:00:04,000\n三行目\n");
        QByteArray crlf = lf;
        crlf.replace("\n", "\r\n");
        QByteArray cr = lf;
        cr.replace("\n", "\r");
        const auto expected = parse(lf, "line.srt");
        const auto windows = parse(crlf, "line.srt");
        const auto classic = parse(cr, "line.srt");
        QVERIFY(expected.document);
        QVERIFY(windows.document);
        QVERIFY(classic.document);
        QCOMPARE(expected.document->cues.front().text, QStringLiteral("一行目\n二行目"));
        QCOMPARE(windows.document->cues, expected.document->cues);
        QCOMPARE(classic.document->cues, expected.document->cues);

        const auto wide = parse(bytes("1\n00:00:01,000 --> 00:00:02,000\nA\n\n\n\n"
                                      "2\n00:00:03,000 --> 00:00:04,000\nB\n"),
                                "wide.srt");
        QVERIFY(wide.document);
        QCOMPARE(wide.document->cues.size(), 2);
    }

    void blockParsing_dropsBlankPrefixAndReadsEndToken() {
        const auto leading = parse(bytes("  \n1\n00:00:01,000 --> 00:00:02,000\n星\n"), "lead.srt");
        QVERIFY(leading.document);
        QCOMPARE(leading.document->cues.size(), 1);
        QCOMPARE(leading.document->cues.front().text, QStringLiteral("星"));

        const auto inline_header =
            parse(bytes("WEBVTT\n00:00.500 --> 00:02.000\nA\n"), "inline-header.vtt");
        QVERIFY(inline_header.document);
        QCOMPARE(inline_header.document->cues.size(), 1);
        QCOMPARE(inline_header.document->cues.front().start_time, 0.5);
        QCOMPARE(inline_header.document->cues.front().end_time, 2.0);
        QCOMPARE(inline_header.document->cues.front().text, QStringLiteral("A"));

        const auto settings =
            parse(bytes("WEBVTT\n\n00:00.500 --> 00:02.000 position:50%\nこんにちは\n"), "s.vtt");
        QVERIFY(settings.document);
        QCOMPARE(settings.document->cues.front().start_time, 0.5);
        QCOMPARE(settings.document->cues.front().end_time, 2.0);
    }

    void blockParsing_rejectsMalformedBlocks() {
        const QByteArray valid = bytes("1\n00:00:01,000 --> 00:00:02,000\n有効\n\n");
        const QList<QByteArray> invalid = {
            bytes("2\nno timing line\n"),
            bytes("2\n00:00:03,000 --> 00:00:04,000 --> 00:00:05,000\n三つ\n"),
            bytes("2\n00:nan:01 --> 00:00:04,000\n時刻\n"),
            bytes("2\n00:00:05,000 --> 00:00:04,000\n逆\n"),
            bytes("2\n00:00:03,000 --> 00:00:04,000\n"),
        };
        for (const QByteArray& block : invalid) {
            const auto result = parse(valid + block, "blocks.srt");
            QVERIFY(result.document);
            QCOMPARE(result.document->cues.size(), 1);
            QCOMPARE(result.document->cues.front().text, QStringLiteral("有効"));
            QCOMPARE(result.document->warnings,
                     QStringList{QStringLiteral("Skipped subtitle block 2.")});
        }
    }

    void timestamps_acceptTwoAndThreeFieldsAndCommas() {
        const auto srt = single_cue("00:00:01,000", "00:00:02.250");
        QVERIFY(srt);
        QCOMPARE(srt->start_time, 1.0);
        QCOMPARE(srt->end_time, 2.25);

        const auto short_form = single_cue("00:00.500", "00:02.000");
        QVERIFY(short_form);
        QCOMPARE(short_form->start_time, 0.5);
        QCOMPARE(short_form->end_time, 2.0);

        const auto hours = single_cue("01:02:03.456", "01:02:04.456");
        QVERIFY(hours);
        QVERIFY(qAbs(hours->start_time - 3723.456) < 1e-9);
        QVERIFY(qAbs(hours->end_time - 3724.456) < 1e-9);
    }

    void timestamps_rejectNonFiniteNegativeAndOverflowingValues() {
        const auto valid = single_cue("00:00:01,000", "00:00:02,000");
        QVERIFY(valid);
        QCOMPARE(valid->start_time, 1.0);
        const QList<std::pair<QByteArray, QByteArray>> invalid = {
            {"00:nan:01", "00:00:04,000"},
            {"-1:00:00.00", "00:00:04,000"},
            {"10000000000000000:00:00.00", "10000000000000000:00:01.00"},
            {"1", "00:00:04,000"},
            {"00:00:01:02", "00:00:04,000"},
            {"-0:00:-0.0006", "00:00:04,000"},
        };
        for (const auto& [start, end] : invalid) {
            QVERIFY2(!single_cue(start, end).has_value(), start.constData());
        }
        QVERIFY(!single_cue("00:00:05,000", "00:00:02,000").has_value());
    }

    void cueIds_useDeclaredLineOrBlockIndex() {
        const auto vtt = parse(bytes("WEBVTT\n\nintro\n00:00.500 --> 00:02.000\nハロー\n\n"
                                     "00:01.500 --> 00:03.000\nまた\n"),
                               "sample.vtt");
        QVERIFY(vtt.document);
        QCOMPARE(vtt.document->cues.size(), 2);
        QCOMPARE(vtt.document->cues.at(0).id, QStringLiteral("intro"));
        QCOMPARE(vtt.document->cues.at(1).id, QStringLiteral("2"));

        const auto spaced =
            parse(bytes(QStringLiteral(" \u3000 \n00:00.500 --> 00:02.000\nA\n")), "s.vtt");
        QVERIFY(spaced.document);
        QCOMPARE(spaced.document->cues.front().id, QStringLiteral("0"));

        const auto blank_id = parse(
            bytes(QStringLiteral("id-line\n \u3000 \n00:00.500 --> 00:02.000\nA\n")), "s.vtt");
        QVERIFY(blank_id.document);
        QCOMPARE(blank_id.document->cues.front().id, QStringLiteral("0"));

        const auto declared = parse(bytes("  42  \n00:00:01,000 --> 00:00:02,000\nA\n"), "s.srt");
        QVERIFY(declared.document);
        QCOMPARE(declared.document->cues.front().id, QStringLiteral("42"));
    }

    void srtText_isTrimmedButKeepsMarkup() {
        const auto padded = parse(bytes(QStringLiteral("1\n00:00:01,000 --> 00:00:02,000\n"
                                                       "\u3000星です\u3000\n")),
                                  "pad.srt");
        QVERIFY(padded.document);
        QCOMPARE(padded.document->cues.front().text, QStringLiteral("星です"));

        const auto marked =
            parse(bytes("1\n00:00:01,000 --> 00:00:02,000\n<i>星</i>\n"), "tag.srt");
        QVERIFY(marked.document);
        QCOMPARE(marked.document->cues.front().text, QStringLiteral("<i>星</i>"));

        const auto blank = parse(bytes(QStringLiteral("1\n00:00:01,000 --> 00:00:02,000\n"
                                                      "\u3000 \n")),
                                 "blank.srt");
        QVERIFY(!blank.document);
        QCOMPARE(blank.error, SubtitleParseError::no_valid_cues);
    }

    void webVttText_stripsTagsAndDecodesEntities() {
        const auto karaoke = parse(
            bytes("WEBVTT\n\n"
                  "00:04:49.000 --> 00:04:54.000 align:start position:0%\n"
                  "駅前<00:04:50.840><c>に</c><00:04:51.039><c>日高屋</c>"
                  "<00:04:51.560><c>以外</c><00:04:52.039><c>やっ</c><00:04:52.240><c>て</c>"
                  "<00:04:52.360><c>ない</c><00:04:52.600><c>みたい</c><00:04:52.880><c>な</c>"
                  "<00:04:53.120><c>状況</c>\n\n"
                  "00:04:54.000 --> 00:04:56.000\n"
                  "<v Speaker><b>Tea &amp; coffee</b></v>\n"),
            "karaoke.vtt");
        QVERIFY(karaoke.document);
        QCOMPARE(karaoke.document->cues.size(), 2);
        QCOMPARE(karaoke.document->cues.at(0).text,
                 QStringLiteral("駅前に日高屋以外やってないみたいな状況"));
        QCOMPARE(karaoke.document->cues.at(1).text, QStringLiteral("Tea & coffee"));

        const auto entities = parse(bytes("WEBVTT\n\n00:00.500 --> 00:02.000\n"
                                          "A&lrm;B&rlm;C&nbsp;D&lt;E&gt;F&quot;G&apos;H&amp;I\n"),
                                    "entities.vtt");
        QVERIFY(entities.document);
        QCOMPARE(entities.document->cues.front().text, QStringLiteral("ABC D<E>F\"G'H&I"));

        const auto nested =
            parse(bytes("WEBVTT\n\n00:00.500 --> 00:02.000\n&amp;lt;\n"), "nested.vtt");
        QVERIFY(nested.document);
        QCOMPARE(nested.document->cues.front().text, QStringLiteral("&lt;"));

        const auto lines = parse(bytes(QStringLiteral("WEBVTT\n\n00:00.500 --> 00:02.000\n"
                                                      "  A  \n \u3000 \n  B  \n")),
                                 "lines.vtt");
        QVERIFY(lines.document);
        QCOMPARE(lines.document->cues.front().text, QStringLiteral("A\nB"));
    }

    void warnings_reportSkippedBlocksExceptHeadersAndNotes() {
        const auto result = parse(bytes("1\n00:00:01,000 --> 00:00:02,000\nA\n\n"
                                        "NOTE a comment\n\n"
                                        "broken\n\n"
                                        "   \n\n"
                                        "4\n00:00:05,000 --> 00:00:06,000\nB\n\n"
                                        "note lowercase\n"),
                                  "warn.srt");
        QVERIFY(result.document);
        QCOMPARE(result.document->cues.size(), 2);
        QCOMPARE(result.document->warnings,
                 (QStringList{QStringLiteral("Skipped subtitle block 3."),
                              QStringLiteral("Skipped subtitle block 6.")}));

        const auto header = parse(bytes("WEBVTT\n\n00:00.500 --> 00:02.000\nA\n"), "header.vtt");
        QVERIFY(header.document);
        QVERIFY(header.document->warnings.isEmpty());
    }

    void cues_sortByStartThenEnd() {
        const auto out_of_order = parse(bytes("2\n00:00:03,500 --> 00:00:05,000\n二行目\nです\n\n"
                                              "1\n00:00:01,000 --> 00:00:02,250\n最初です\n"),
                                        "sample.srt");
        QVERIFY(out_of_order.document);
        QCOMPARE(out_of_order.document->cues.size(), 2);
        QCOMPARE(out_of_order.document->cues.at(0).text, QStringLiteral("最初です"));
        QCOMPARE(out_of_order.document->cues.at(1).text, QStringLiteral("二行目\nです"));
        QCOMPARE(out_of_order.document->cues.at(0).end_time, 2.25);

        const auto same_start = parse(bytes("b\n00:00:01,000 --> 00:00:03,000\n長い\n\n"
                                            "a\n00:00:01,000 --> 00:00:02,000\n短い\n"),
                                      "tie.srt");
        QVERIFY(same_start.document);
        QCOMPARE(ids(same_start.document->cues), (QStringList{"a", "b"}));
    }

    void noValidCues_isAnError() {
        const auto empty = parse({}, "empty.srt");
        QVERIFY(!empty.document);
        QCOMPARE(empty.error, SubtitleParseError::no_valid_cues);

        const auto garbage = parse(bytes("garbage\n\nmore garbage\n"), "garbage.srt");
        QVERIFY(!garbage.document);
        QCOMPARE(garbage.error, SubtitleParseError::no_valid_cues);
    }

    void store_splitsLongAndTimedCuesAndKeepsEndpointsInclusive() {
        const SubtitleCueStore store(document_of({
            cue(QStringLiteral("long"), 0.0, 31.0),
            cue(QStringLiteral("early"), 0.5, 2.0),
        }));
        QCOMPARE(ids(store.cues_at(0.5)), (QStringList{"long", "early"}));
        QCOMPARE(ids(store.cues_at(2.0)), (QStringList{"long", "early"}));
        QCOMPARE(ids(store.cues_at(2.25)), QStringList{"long"});
        QCOMPARE(ids(store.cues_at(30.5)), QStringList{"long"});
        QCOMPARE(ids(store.cues_at(31.0)), QStringList{"long"});
        QCOMPARE(ids(store.cues_at(0.49)), QStringList{"long"});
        QCOMPARE(ids(store.cues_at(31.1)), QStringList{});
    }

    void store_returnsOverlappingCuesSortedByStartThenEnd() {
        const SubtitleCueStore store(document_of({
            cue(QStringLiteral("B"), 1.5, 3.0),
            cue(QStringLiteral("A"), 0.5, 2.0),
            cue(QStringLiteral("short"), 1.0, 2.0),
            cue(QStringLiteral("long-end"), 1.0, 3.5),
        }));
        QCOMPARE(ids(store.cues_at(1.75)), (QStringList{"A", "short", "long-end", "B"}));
        QCOMPARE(ids(store.cues_at(0.25)), QStringList{});
        QCOMPARE(ids(store.cues_at(3.1)), QStringList{"long-end"});
        QCOMPARE(ids(store.cues_at(0.5)), QStringList{"A"});
    }

    void store_appliesSubtitleDelay() {
        const SubtitleCueStore store(
            document_of({cue(QStringLiteral("A"), 0.5, 2.0), cue(QStringLiteral("B"), 1.5, 3.0)}));
        QCOMPARE(ids(store.cues_at_playback(2.75, 1.0)), (QStringList{"A", "B"}));
        QCOMPARE(ids(store.cues_at_playback(0.75, -1.0)), (QStringList{"A", "B"}));
        QCOMPARE(ids(store.cues_at_playback(3.75, 1.0)), QStringList{"B"});
    }

    void store_slicesShowingBeforeAndAfter() {
        const SubtitleCueStore store(document_of({
            cue(QStringLiteral("first"), 0.0, 1.5),
            cue(QStringLiteral("second"), 2.0, 4.0),
            cue(QStringLiteral("third"), 5.0, 7.0),
        }));
        const auto gap = store.slice_at(4.5);
        QCOMPARE(ids(gap.showing), QStringList{});
        QCOMPARE(ids(gap.last_shown), QStringList{"second"});
        QCOMPARE(ids(gap.next_to_show), QStringList{"third"});

        const auto boundary = store.slice_at(1.5);
        QCOMPARE(ids(boundary.showing), QStringList{"first"});
        QCOMPARE(ids(boundary.last_shown), QStringList{});
        QCOMPARE(ids(boundary.next_to_show), QStringList{"second"});

        const auto start_boundary = store.slice_at(2.0);
        QCOMPARE(ids(start_boundary.showing), QStringList{"second"});
        QCOMPARE(ids(start_boundary.next_to_show), QStringList{"third"});
    }

    void store_sliceKeepsEqualEndsAndEqualStartsTogether() {
        const SubtitleCueStore store(document_of({
            cue(QStringLiteral("a"), 0.0, 2.0),
            cue(QStringLiteral("b"), 1.0, 2.0),
            cue(QStringLiteral("c"), 3.0, 4.0),
            cue(QStringLiteral("d"), 3.0, 5.0),
        }));
        const auto slice = store.slice_at(2.5);
        QCOMPARE(ids(slice.showing), QStringList{});
        QCOMPARE(ids(slice.last_shown), (QStringList{"a", "b"}));
        QCOMPARE(ids(slice.next_to_show), (QStringList{"c", "d"}));
    }

    void store_sliceAtPlaybackSubtractsDelay() {
        const SubtitleCueStore store(document_of({
            cue(QStringLiteral("first"), 0.0, 1.5),
            cue(QStringLiteral("second"), 2.0, 4.0),
            cue(QStringLiteral("third"), 5.0, 7.0),
        }));
        QCOMPARE(store.slice_at_playback(5.5, 1.0), store.slice_at(4.5));
        QCOMPARE(ids(store.slice_at_playback(5.5, 1.0).next_to_show), QStringList{"third"});
    }

    void store_alignsDelayToAdjacentCue() {
        const SubtitleCueStore store(document_of({
            cue(QStringLiteral("first"), 0.0, 1.5),
            cue(QStringLiteral("second"), 2.0, 4.0),
            cue(QStringLiteral("third"), 5.0, 7.0),
        }));
        QCOMPARE(store.delay_aligning_adjacent_cue(4.5, 0.0, SubtitleOffsetDirection::previous),
                 std::optional<double>(2.5));
        QCOMPARE(store.delay_aligning_adjacent_cue(4.5, 0.0, SubtitleOffsetDirection::next),
                 std::optional<double>(-0.5));
        QCOMPARE(store.delay_aligning_adjacent_cue(5.0, 0.5, SubtitleOffsetDirection::previous),
                 std::optional<double>(3.0));
        QCOMPARE(store.delay_aligning_adjacent_cue(3.25, 0.0, SubtitleOffsetDirection::previous),
                 std::optional<double>(3.25));
        QCOMPARE(store.delay_aligning_adjacent_cue(0.5, 0.0, SubtitleOffsetDirection::previous),
                 std::nullopt);

        const SubtitleCueStore equal_ends(
            document_of({cue(QStringLiteral("a"), 0.0, 2.0), cue(QStringLiteral("b"), 1.0, 2.0)}));
        QCOMPARE(
            equal_ends.delay_aligning_adjacent_cue(2.5, 0.0, SubtitleOffsetDirection::previous),
            std::optional<double>(1.5));
        QCOMPARE(store.delay_aligning_adjacent_cue(7.5, 0.0, SubtitleOffsetDirection::next),
                 std::nullopt);
    }
};

}  // namespace
}  // namespace niratan::player

QTEST_APPLESS_MAIN(niratan::player::SubtitleDocumentTest)
#include "tst_subtitle_document.moc"
