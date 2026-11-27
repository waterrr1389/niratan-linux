// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/sasayaki/sasayaki.h"
#include "services/sasayaki/srt_parser.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using niratan::sasayaki::encode_cues;
using niratan::sasayaki::match_data_from_json;
using niratan::sasayaki::match_data_to_json;
using niratan::sasayaki::parse_cues;
using niratan::sasayaki::playback_data_from_json;
using niratan::sasayaki::playback_data_to_json;
using niratan::sasayaki::read_cue_file;
using niratan::sasayaki::reader_progress;
using niratan::sasayaki::SasayakiCue;
using niratan::sasayaki::SasayakiMatch;
using niratan::sasayaki::SasayakiMatchData;
using niratan::sasayaki::SasayakiPlaybackData;

// Models/Sasayaki.swift and Features/Sasayaki/{SasayakiParser,SasayakiSRT}.swift
// are the specification; script/test_sasayaki_text_matcher.swift supplies the
// legacy documents that omit contextInferred.
namespace {

SasayakiCue cue(const std::string& id, double start, double end, const std::string& text) {
    SasayakiCue value;
    value.id = id;
    value.start_time = start;
    value.end_time = end;
    value.text = text;
    return value;
}

SasayakiMatch sample_match(const std::string& id, bool context_inferred) {
    SasayakiMatch match;
    match.id = id;
    match.start_time = 12.5;
    match.end_time = 13.75;
    match.text = "Matched text";
    match.chapter_index = 2;
    match.start = 100;
    match.length = 12;
    match.context_inferred = context_inferred;
    return match;
}

QByteArray sample_match_json() {
    return QByteArrayLiteral(
        "{\"id\":\"a\",\"startTime\":12.5,\"endTime\":13.75,\"text\":\"Matched text\","
        "\"chapterIndex\":2,\"start\":100,\"length\":12}");
}

QByteArray match_document(const QByteArray& match) {
    return QByteArrayLiteral("{\"matches\":[") + match + QByteArrayLiteral("],\"unmatched\":0}");
}

bool accepts_match_document(const QByteArray& document) {
    return match_data_from_json(QJsonDocument::fromJson(document)).has_value();
}

bool accepts_playback_document(const char* document) {
    return playback_data_from_json(QJsonDocument::fromJson(QByteArray(document))).has_value();
}

}  // namespace

class SasayakiTest : public QObject {
    Q_OBJECT

   private slots:
    void parses_a_multi_cue_file();
    void reads_cues_from_a_file();
    void an_unreadable_cue_file_is_none();
    void reads_comma_and_dot_milliseconds();
    void strips_the_bom_and_normalizes_line_endings();
    void joins_text_lines_and_trims_them();
    void numbers_cues_by_block_position();
    void rejects_invalid_timing_lines();
    void drops_blocks_without_text();
    void ignores_lines_before_the_timing_line();
    void encodes_cues_as_srt();
    void encodes_nothing_for_no_valid_cue();
    void match_data_round_trips();
    void match_json_uses_the_swift_keys();
    void match_data_defaults_context_inferred_to_false();
    void match_data_rejects_missing_or_mistyped_keys();
    void match_data_ignores_extra_keys();
    void reader_progress_clamps_to_the_chapter();
    void playback_data_round_trips_with_a_bookmark();
    void playback_data_defaults_and_omits_the_bookmark();
    void playback_data_rejects_missing_or_mistyped_keys();
};

void SasayakiTest::parses_a_multi_cue_file() {
    const std::vector<SasayakiCue> cues = parse_cues(
        "1\n"
        "00:00:19,124 --> 00:00:22,016\n"
        "First line\n"
        "second line\n"
        "\n"
        "2\n"
        "00:00:24,148 --> 00:00:28,468\n"
        "＊シックスイーザー号、\n");

    QVERIFY(cues.size() == std::size_t(2));
    QCOMPARE(cues[0].id, std::string("0"));
    QCOMPARE(cues[0].start_time, 19.124);
    QCOMPARE(cues[0].end_time, 22.016);
    QCOMPARE(cues[0].text, std::string("First line second line"));
    QCOMPARE(cues[1].id, std::string("1"));
    QCOMPARE(cues[1].start_time, 24.148);
    QCOMPARE(cues[1].end_time, 28.468);
    QCOMPARE(cues[1].text, std::string("＊シックスイーザー号、"));
}

void SasayakiTest::reads_comma_and_dot_milliseconds() {
    const std::vector<SasayakiCue> comma =
        parse_cues("00:01:02,500 --> 00:01:03,750\nComma\n");
    const std::vector<SasayakiCue> dot =
        parse_cues("00:01:02.500 --> 00:01:03.750\nDot\n");

    QVERIFY(comma.size() == std::size_t(1));
    QVERIFY(dot.size() == std::size_t(1));
    QCOMPARE(comma[0].start_time, 62.5);
    QCOMPARE(comma[0].end_time, 63.75);
    QCOMPARE(dot[0].start_time, comma[0].start_time);
    QCOMPARE(dot[0].end_time, comma[0].end_time);
}

void SasayakiTest::strips_the_bom_and_normalizes_line_endings() {
    const std::vector<SasayakiCue> windows = parse_cues(
        "\xEF\xBB\xBF"
        "1\r\n00:00:01,000 --> 00:00:02,000\r\nLine one\r\n\r\n"
        "2\r\n00:00:03,000 --> 00:00:04,000\r\nLine two\r\n");

    QVERIFY(windows.size() == std::size_t(2));
    QCOMPARE(windows[0].id, std::string("0"));
    QCOMPARE(windows[0].text, std::string("Line one"));
    QCOMPARE(windows[1].text, std::string("Line two"));

    const std::vector<SasayakiCue> classic = parse_cues(
        "1\r00:00:05,000 --> 00:00:06,000\rClassic\r\r"
        "2\r00:00:07,000 --> 00:00:08,000\rMac\r");

    QVERIFY(classic.size() == std::size_t(2));
    QCOMPARE(classic[0].text, std::string("Classic"));
    QCOMPARE(classic[1].text, std::string("Mac"));
}

void SasayakiTest::joins_text_lines_and_trims_them() {
    const std::vector<SasayakiCue> cues = parse_cues(
        "00:00:01,000 --> 00:00:02,000\n"
        "  Indented line  \n"
        "\tSecond line\t\n"
        "   \n"
        "00:00:03,000 --> 00:00:04,000\n"
        "After a blank line\n");

    QVERIFY(cues.size() == std::size_t(2));
    QCOMPARE(cues[0].text, std::string("Indented line Second line"));
    QCOMPARE(cues[1].text, std::string("After a blank line"));

    // U+3000 is in category Zs, so it is trimmed at a line's ends.
    const std::vector<SasayakiCue> spaced =
        parse_cues("00:00:01,000 --> 00:00:02,000\n　全角　\n");
    QVERIFY(spaced.size() == std::size_t(1));
    QCOMPARE(spaced[0].text, std::string("全角"));
}

void SasayakiTest::numbers_cues_by_block_position() {
    // The rejected block still consumes index 0.
    const std::vector<SasayakiCue> rejected = parse_cues(
        "1\n"
        "00:00:05,000 --> 00:00:04,000\n"
        "Reversed\n"
        "\n"
        "2\n"
        "00:00:06,000 --> 00:00:07,000\n"
        "Kept\n");
    QVERIFY(rejected.size() == std::size_t(1));
    QCOMPARE(rejected[0].id, std::string("1"));

    // A timing line with no following line makes the first block empty.
    const std::vector<SasayakiCue> empty = parse_cues(
        "00:00:01,000 --> 00:00:02,000\n"
        "\n"
        "2\n"
        "00:00:03,000 --> 00:00:04,000\n"
        "Kept\n");
    QVERIFY(empty.size() == std::size_t(1));
    QCOMPARE(empty[0].id, std::string("1"));

    // Two leading newlines produce an empty first block, not a leading blank
    // line inside the first cue.
    const std::vector<SasayakiCue> leading =
        parse_cues("\n\n00:00:01,000 --> 00:00:02,000\nKept\n");
    QVERIFY(leading.size() == std::size_t(1));
    QCOMPARE(leading[0].id, std::string("1"));
}

void SasayakiTest::rejects_invalid_timing_lines() {
    const char* const documents[] = {
        "00:00:01,000 --> 00:00:60,000\nText\n",              // seconds 60
        "00:01:60,000 --> 00:02:00,000\nText\n",              // minutes 60
        "00:00:05,000 --> 00:00:05,000\nText\n",              // equal
        "00:00:06,000 --> 00:00:05,000\nText\n",              // reversed
        "00:01,000 --> 00:02,000\nText\n",                    // two components
        "00:00:00:01,000 --> 00:00:02,000\nText\n",           // four components
        "00:00:aa,000 --> 00:00:02,000\nText\n",              // not a number
        "00:-01:00,000 --> 00:00:02,000\nText\n",             // negative minutes
        "-1:00:00,000 --> 00:00:02,000\nText\n",              // negative hours
        "00:00:01,000 --> 00:00:02,000 --> 00:00:03,000\nText\n",
        "00:00:01,00000:00:02,000\nText\n",                   // no arrow
        "00:00:01,000 -->\nText\n",                           // empty end
    };

    for (const char* document : documents) {
        QVERIFY2(parse_cues(document).empty(), document);
    }
}

void SasayakiTest::drops_blocks_without_text() {
    const char* const documents[] = {
        "00:00:01,000 --> 00:00:02,000\n",      // timing line is last
        "00:00:01,000 --> 00:00:02,000\n   \n",  // spaces only
        "00:00:01,000 --> 00:00:02,000\n\t\n",   // tab only
    };

    for (const char* document : documents) {
        QVERIFY2(parse_cues(document).empty(), document);
    }
}

void SasayakiTest::ignores_lines_before_the_timing_line() {
    const std::vector<SasayakiCue> cues = parse_cues(
        "1\n"
        "A note before the timing line\n"
        "00:00:01,000 --> 00:00:02,000\n"
        "Text\n");

    QVERIFY(cues.size() == std::size_t(1));
    QCOMPARE(cues[0].text, std::string("Text"));
}

void SasayakiTest::encodes_cues_as_srt() {
    std::vector<SasayakiCue> cues;
    cues.push_back(cue("0", 10.0, 11.0, "later"));
    cues.push_back(cue("1", 1.0625, 2.0, "line one\r\nline two"));
    cues.push_back(cue("2", 0.5, 0.9, "first"));
    cues.push_back(cue("3", -1.0, 1.0, "negative start"));
    cues.push_back(cue("4", 3.0, 2.0, "reversed"));
    cues.push_back(cue("5", 4.0, 4.0001, "tiny"));
    cues.push_back(cue("6", 360000000.0, 360000001.0, "capped"));

    const std::string srt = encode_cues(cues);

    QCOMPARE(srt, std::string(
        "1\n"
        "00:00:00,500 --> 00:00:00,900\n"
        "first\n"
        "\n"
        "2\n"
        "00:00:01,063 --> 00:00:02,000\n"
        "line one line two\n"
        "\n"
        "3\n"
        "00:00:04,000 --> 00:00:04,001\n"
        "tiny\n"
        "\n"
        "4\n"
        "00:00:10,000 --> 00:00:11,000\n"
        "later\n"
        "\n"
        "5\n"
        "99999:59:59,000 --> 99999:59:59,000\n"
        "capped\n"));
}

void SasayakiTest::encodes_nothing_for_no_valid_cue() {
    const std::vector<SasayakiCue> none;
    QCOMPARE(encode_cues(none), std::string(""));

    std::vector<SasayakiCue> invalid;
    invalid.push_back(cue("0", -1.0, 1.0, "negative start"));
    invalid.push_back(cue("1", 2.0, 2.0, "empty duration"));
    invalid.push_back(
        cue("2", std::numeric_limits<double>::quiet_NaN(), 1.0, "not a number"));
    invalid.push_back(
        cue("3", std::numeric_limits<double>::infinity(), 2.0, "infinite"));
    QCOMPARE(encode_cues(invalid), std::string(""));
}

void SasayakiTest::match_data_round_trips() {
    SasayakiMatchData data;
    data.matches.push_back(sample_match("a", false));
    data.matches.push_back(sample_match("b", true));
    data.unmatched = 4;

    const auto decoded = match_data_from_json(match_data_to_json(data));

    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, data);
}

void SasayakiTest::match_json_uses_the_swift_keys() {
    SasayakiMatchData data;
    data.matches.push_back(sample_match("a", false));
    data.matches.push_back(sample_match("b", true));
    data.unmatched = 4;

    const QJsonObject root = match_data_to_json(data).object();
    QCOMPARE(root.value("unmatched").toInt(), 4);

    const QJsonArray matches = root.value("matches").toArray();
    QVERIFY(matches.size() == qsizetype(2));

    const QJsonObject first = matches[0].toObject();
    QCOMPARE(first.value("id").toString(), QString("a"));
    QCOMPARE(first.value("startTime").toDouble(), 12.5);
    QCOMPARE(first.value("endTime").toDouble(), 13.75);
    QCOMPARE(first.value("text").toString(), QString("Matched text"));
    QCOMPARE(first.value("chapterIndex").toInt(), 2);
    QCOMPARE(first.value("start").toInt(), 100);
    QCOMPARE(first.value("length").toInt(), 12);
    QVERIFY(!first.contains(QStringLiteral("contextInferred")));

    const QJsonObject second = matches[1].toObject();
    QCOMPARE(second.value("contextInferred").toBool(), true);
}

void SasayakiTest::match_data_defaults_context_inferred_to_false() {
    const QByteArray document = QByteArrayLiteral(
        "{\"matches\":[{\"id\":\"a\",\"startTime\":1,\"endTime\":2,\"text\":\"t\","
        "\"chapterIndex\":0,\"start\":0,\"length\":1}],\"unmatched\":0}");

    const auto decoded = match_data_from_json(QJsonDocument::fromJson(document));
    QVERIFY(decoded.has_value());
    QVERIFY(decoded->matches.size() == std::size_t(1));
    QVERIFY(!decoded->matches[0].context_inferred);

    QByteArray explicit_false = sample_match_json();
    explicit_false.replace(QByteArrayLiteral("\"length\":12"),
                           QByteArrayLiteral("\"length\":12,\"contextInferred\":false"));
    const auto false_document =
        match_data_from_json(QJsonDocument::fromJson(match_document(explicit_false)));
    QVERIFY(false_document.has_value());
    QVERIFY(false_document->matches.size() == std::size_t(1));
    QVERIFY(!false_document->matches[0].context_inferred);
}

void SasayakiTest::match_data_rejects_missing_or_mistyped_keys() {
    QVERIFY(accepts_match_document(match_document(sample_match_json())));

    QByteArray document = sample_match_json();
    document.replace(QByteArrayLiteral("\"id\":\"a\""), QByteArrayLiteral("\"id\":1"));
    QVERIFY(!accepts_match_document(match_document(document)));

    document = sample_match_json();
    document.replace(QByteArrayLiteral("\"startTime\":12.5"),
                     QByteArrayLiteral("\"startTime\":\"12.5\""));
    QVERIFY(!accepts_match_document(match_document(document)));

    document = sample_match_json();
    document.replace(QByteArrayLiteral("\"text\":\"Matched text\","), QByteArray());
    QVERIFY(!accepts_match_document(match_document(document)));

    document = sample_match_json();
    document.replace(QByteArrayLiteral("\"chapterIndex\":2"),
                     QByteArrayLiteral("\"chapterIndex\":2.5"));
    QVERIFY(!accepts_match_document(match_document(document)));

    document = sample_match_json();
    document.replace(QByteArrayLiteral("\"length\":12"),
                     QByteArrayLiteral("\"contextInferred\":\"yes\",\"length\":12"));
    QVERIFY(!accepts_match_document(match_document(document)));

    QVERIFY(!accepts_match_document(
        QByteArrayLiteral("{\"matches\":[]}")));
    QVERIFY(!accepts_match_document(
        QByteArrayLiteral("{\"matches\":[],\"unmatched\":\"0\"}")));
    QVERIFY(!accepts_match_document(
        QByteArrayLiteral("{\"matches\":{},\"unmatched\":0}")));
    QVERIFY(!accepts_match_document(
        QByteArrayLiteral("{\"matches\":[\"cue\"],\"unmatched\":0}")));
    QVERIFY(!accepts_match_document(QByteArrayLiteral("[]")));
}

void SasayakiTest::match_data_ignores_extra_keys() {
    const QByteArray document = QByteArrayLiteral(
        "{\"matches\":[{\"id\":\"a\",\"startTime\":1,\"endTime\":2,\"text\":\"t\","
        "\"chapterIndex\":0,\"start\":0,\"length\":1,\"note\":\"ignored\"}],"
        "\"unmatched\":5,\"version\":2}");

    const auto decoded = match_data_from_json(QJsonDocument::fromJson(document));

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->unmatched, 5);
    QVERIFY(decoded->matches.size() == std::size_t(1));
    QCOMPARE(decoded->matches[0].id, std::string("a"));
}

// B1: SasayakiMatch.readerProgress clamps start over the chapter count to
// 0...1 and reads an unknown chapter count as 0.
void SasayakiTest::reader_progress_clamps_to_the_chapter() {
    SasayakiMatch match;
    const auto progress = [&match](int start, int chapter_character_count) {
        match.start = start;
        return reader_progress(match, chapter_character_count);
    };

    QCOMPARE(progress(50, 200), 0.25);
    QCOMPARE(progress(-10, 200), 0.0);
    QCOMPARE(progress(250, 200), 1.0);
    QCOMPARE(progress(50, 0), 0.0);
    QCOMPARE(progress(0, 200), 0.0);
    QCOMPARE(progress(200, 200), 1.0);
}

void SasayakiTest::playback_data_round_trips_with_a_bookmark() {
    SasayakiPlaybackData data;
    data.last_position = 123.25;
    data.delay = -0.5;
    data.rate = 1.25F;
    data.audio_bookmark = QByteArray("\x00\x01\xFE\xFF", 4);

    const QJsonObject object = playback_data_to_json(data).object();
    QCOMPARE(object.value("lastPosition").toDouble(), 123.25);
    QCOMPARE(object.value("delay").toDouble(), -0.5);
    QCOMPARE(object.value("rate").toDouble(), 1.25);
    QCOMPARE(object.value("audioBookmark").toString(), QString("AAH+/w=="));

    const auto decoded = playback_data_from_json(playback_data_to_json(data));
    QVERIFY(decoded.has_value());
    QCOMPARE(*decoded, data);

    const auto from_hand_written = playback_data_from_json(QJsonDocument::fromJson(
        QByteArrayLiteral("{\"lastPosition\":1,\"audioBookmark\":\"AAH+/w==\"}")));
    QVERIFY(from_hand_written.has_value());
    QVERIFY(from_hand_written->audio_bookmark.has_value());
    QCOMPARE(*from_hand_written->audio_bookmark, QByteArray("\x00\x01\xFE\xFF", 4));
}

void SasayakiTest::playback_data_defaults_and_omits_the_bookmark() {
    const QByteArray document = QByteArrayLiteral("{\"lastPosition\":2.5,\"extra\":true}");

    const auto decoded = playback_data_from_json(QJsonDocument::fromJson(document));
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->last_position, 2.5);
    QCOMPARE(decoded->delay, 0.0);
    QCOMPARE(decoded->rate, 1.0F);
    QVERIFY(!decoded->audio_bookmark.has_value());

    const QJsonObject object = playback_data_to_json(*decoded).object();
    QVERIFY(!object.contains(QStringLiteral("audioBookmark")));
    QCOMPARE(object.value("lastPosition").toDouble(), 2.5);
    QCOMPARE(object.value("delay").toDouble(), 0.0);
    QCOMPARE(object.value("rate").toDouble(), 1.0);

    const auto round_trip = playback_data_from_json(playback_data_to_json(*decoded));
    QVERIFY(round_trip.has_value());
    QCOMPARE(*round_trip, *decoded);
}

void SasayakiTest::playback_data_rejects_missing_or_mistyped_keys() {
    const char* const documents[] = {
        "{\"delay\":0}",                              // lastPosition missing
        "{\"lastPosition\":\"2.5\"}",                 // wrong type
        "{\"lastPosition\":null}",                    // null is not a number
        "{\"lastPosition\":1,\"delay\":\"0\"}",       // wrong type
        "{\"lastPosition\":1,\"rate\":true}",         // wrong type
        "{\"lastPosition\":1,\"audioBookmark\":1}",   // wrong type
        "[]",                                         // not an object
    };

    for (const char* document : documents) {
        QVERIFY2(!accepts_playback_document(document), document);
    }

    const auto narrowed = playback_data_from_json(QJsonDocument::fromJson(
        QByteArrayLiteral("{\"lastPosition\":1,\"rate\":0.1}")));
    QVERIFY(narrowed.has_value());
    QCOMPARE(narrowed->rate, 0.1F);
}

void SasayakiTest::reads_cues_from_a_file() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path file =
        std::filesystem::path(directory.path().toStdString()) / "cues.srt";
    std::ofstream(file, std::ios::binary) << "1\r\n00:00:01,000 --> 00:00:02,500\r\nHello\r\n";

    const auto cues = read_cue_file(file);
    QVERIFY(cues.has_value());
    QCOMPARE(cues->size(), std::size_t{1});
    QCOMPARE(cues->front(), cue("0", 1.0, 2.5, "Hello"));
}

void SasayakiTest::an_unreadable_cue_file_is_none() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::filesystem::path root(directory.path().toStdString());
    QVERIFY(!read_cue_file(root / "missing.srt").has_value());
    QVERIFY(!read_cue_file(root).has_value());
}

QTEST_APPLESS_MAIN(SasayakiTest)

#include "tst_sasayaki.moc"
